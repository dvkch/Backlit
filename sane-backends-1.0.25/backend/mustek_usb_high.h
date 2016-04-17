/* sane - Scanner Access Now Easy.

   Copyright (C) 2000 Mustek.
   Originally maintained by Tom Wang <tom.wang@mustek.com.tw>

   Copyright (C) 2001, 2002 by Henning Meier-Geinitz.

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

   This file implements a SANE backend for Mustek 1200UB and similar 
   USB flatbed scanners.  */

#ifndef mustek_usb_high_h
#define mustek_usb_high_h
#include "mustek_usb_mid.h"

/* ---------------------------------- macros ------------------------------ */

#define I8O8RGB 0
#define I8O8MONO 1
#define I4O1MONO 2

/* ---------------------------------- types ------------------------------- */

struct Mustek_Usb_Device;

typedef SANE_Status (*Powerdelay_Function) (ma1017 *, SANE_Byte);

typedef SANE_Status
  (*Getline_Function) (struct Mustek_Usb_Device * dev, SANE_Byte *,
		       SANE_Bool is_order_invert);

typedef SANE_Status (*Backtrack_Function) (struct Mustek_Usb_Device * dev);

typedef enum Colormode
{
  RGB48 = 0,
  RGB42 = 1,
  RGB36 = 2,
  RGB30 = 3,
  RGB24 = 4,
  GRAY16 = 5,
  GRAY14 = 6,
  GRAY12 = 7,
  GRAY10 = 8,
  GRAY8 = 9,
  TEXT = 10,
  RGB48EXT = 11,
  RGB42EXT = 12,
  RGB36EXT = 13,
  RGB30EXT = 14,
  RGB24EXT = 15,
  GRAY16EXT = 16,
  GRAY14EXT = 17,
  GRAY12EXT = 18,
  GRAY10EXT = 19,
  GRAY8EXT = 20,
  TEXTEXT = 21
}
Colormode;

typedef enum Signal_State
{
  SS_UNKNOWN = 0,
  SS_BRIGHTER = 1,
  SS_DARKER = 2,
  SS_EQUAL = 3
}
Signal_State;

typedef struct Calibrator
{
  /* Calibration Data */
  SANE_Bool is_prepared;
  SANE_Word *k_white;
  SANE_Word *k_dark;
  /* Working Buffer */
  double *white_line;
  double *dark_line;
  SANE_Int *white_buffer;
  /* Necessary Parameters */
  SANE_Word k_white_level;
  SANE_Word k_dark_level;
  SANE_Word major_average;
  SANE_Word minor_average;
  SANE_Word filter;
  SANE_Word white_needed;
  SANE_Word dark_needed;
  SANE_Word max_width;
  SANE_Word width;
  SANE_Word threshold;
  SANE_Word *gamma_table;
  SANE_Byte calibrator_type;
}
Calibrator;

enum Mustek_Usb_Modes
{
  MUSTEK_USB_MODE_LINEART = 0,
  MUSTEK_USB_MODE_GRAY,
  MUSTEK_USB_MODE_COLOR
};

enum Mustek_Usb_Option
{
  OPT_NUM_OPTS = 0,

  OPT_MODE_GROUP,
  OPT_MODE,
  OPT_RESOLUTION,
  OPT_PREVIEW,

  OPT_GEOMETRY_GROUP,		/* 5 */
  OPT_TL_X,			/* top-left x */
  OPT_TL_Y,			/* top-left y */
  OPT_BR_X,			/* bottom-right x */
  OPT_BR_Y,			/* bottom-right y */

  OPT_ENHANCEMENT_GROUP,	/* 10 */
  OPT_THRESHOLD,
  OPT_CUSTOM_GAMMA,
  OPT_GAMMA_VECTOR,
  OPT_GAMMA_VECTOR_R,
  OPT_GAMMA_VECTOR_G,
  OPT_GAMMA_VECTOR_B,

  /* must come last: */
  NUM_OPTIONS
};

typedef struct Mustek_Usb_Device
{
  struct Mustek_Usb_Device *next;
  SANE_String name;
  SANE_Device sane;
  SANE_Range dpi_range;
  SANE_Range x_range;
  SANE_Range y_range;
  /* max width & max height in 300 dpi */
  SANE_Int max_width;
  SANE_Int max_height;

  ma1017 *chip;			/* registers of the scanner controller chip */

  Colormode scan_mode;
  SANE_Word x_dpi;
  SANE_Word y_dpi;
  SANE_Word x;
  SANE_Word y;
  SANE_Word width;
  SANE_Word height;
  SANE_Word bytes_per_row;
  SANE_Word bpp;

  SANE_Byte *scan_buffer;
  SANE_Byte *scan_buffer_start;
  size_t scan_buffer_len;

  SANE_Byte *temp_buffer;
  SANE_Byte *temp_buffer_start;
  size_t temp_buffer_len;

  SANE_Word line_switch;
  SANE_Word line_offset;

  SANE_Bool is_cis_detected;

  SANE_Word init_bytes_per_strip;
  SANE_Word adjust_length_300;
  SANE_Word adjust_length_600;
  SANE_Word init_min_expose_time;
  SANE_Word init_skips_per_row_300;
  SANE_Word init_skips_per_row_600;
  SANE_Word init_j_lines;
  SANE_Word init_k_lines;
  SANE_Word init_k_filter;
  SANE_Int init_k_loops;
  SANE_Word init_pixel_rate_lines;
  SANE_Word init_pixel_rate_filts;
  SANE_Word init_powerdelay_lines;
  SANE_Word init_home_lines;
  SANE_Word init_dark_lines;
  SANE_Word init_k_level;
  SANE_Byte init_max_power_delay;
  SANE_Byte init_min_power_delay;
  SANE_Byte init_adjust_way;
  double init_green_black_factor;
  double init_blue_black_factor;
  double init_red_black_factor;
  double init_gray_black_factor;
  double init_green_factor;
  double init_blue_factor;
  double init_red_factor;
  double init_gray_factor;

  SANE_Int init_red_rgb_600_pga;
  SANE_Int init_green_rgb_600_pga;
  SANE_Int init_blue_rgb_600_pga;
  SANE_Int init_mono_600_pga;
  SANE_Int init_red_rgb_300_pga;
  SANE_Int init_green_rgb_300_pga;
  SANE_Int init_blue_rgb_300_pga;
  SANE_Int init_mono_300_pga;
  SANE_Word init_expose_time;
  SANE_Byte init_red_rgb_600_power_delay;
  SANE_Byte init_green_rgb_600_power_delay;
  SANE_Byte init_blue_rgb_600_power_delay;
  SANE_Byte init_red_mono_600_power_delay;
  SANE_Byte init_green_mono_600_power_delay;
  SANE_Byte init_blue_mono_600_power_delay;
  SANE_Byte init_red_rgb_300_power_delay;
  SANE_Byte init_green_rgb_300_power_delay;
  SANE_Byte init_blue_rgb_300_power_delay;
  SANE_Byte init_red_mono_300_power_delay;
  SANE_Byte init_green_mono_300_power_delay;
  SANE_Byte init_blue_mono_300_power_delay;
  SANE_Byte init_threshold;

  SANE_Byte init_top_ref;
  SANE_Byte init_front_end;
  SANE_Byte init_red_offset;
  SANE_Byte init_green_offset;
  SANE_Byte init_blue_offset;

  SANE_Int init_rgb_24_back_track;
  SANE_Int init_mono_8_back_track;

  SANE_Bool is_open;
  SANE_Bool is_prepared;
  SANE_Word expose_time;
  SANE_Word dummy;
  SANE_Word bytes_per_strip;
  SANE_Byte *image_buffer;
  SANE_Byte *red;
  SANE_Byte *green;
  SANE_Byte *blue;
  Getline_Function get_line;
  Backtrack_Function backtrack;
  SANE_Bool is_adjusted_rgb_600_power_delay;
  SANE_Bool is_adjusted_mono_600_power_delay;
  SANE_Bool is_adjusted_rgb_300_power_delay;
  SANE_Bool is_adjusted_mono_300_power_delay;
  SANE_Bool is_evaluate_pixel_rate;
  SANE_Int red_rgb_600_pga;
  SANE_Int green_rgb_600_pga;
  SANE_Int blue_rgb_600_pga;
  SANE_Int mono_600_pga;
  SANE_Byte red_rgb_600_power_delay;
  SANE_Byte green_rgb_600_power_delay;
  SANE_Byte blue_rgb_600_power_delay;
  SANE_Byte red_mono_600_power_delay;
  SANE_Byte green_mono_600_power_delay;
  SANE_Byte blue_mono_600_power_delay;
  SANE_Int red_rgb_300_pga;
  SANE_Int green_rgb_300_pga;
  SANE_Int blue_rgb_300_pga;
  SANE_Int mono_300_pga;
  SANE_Byte red_rgb_300_power_delay;
  SANE_Byte green_rgb_300_power_delay;
  SANE_Byte blue_rgb_300_power_delay;
  SANE_Byte red_mono_300_power_delay;
  SANE_Byte green_mono_300_power_delay;
  SANE_Byte blue_mono_300_power_delay;
  SANE_Word pixel_rate;
  SANE_Byte threshold;
  SANE_Word *gamma_table;
  SANE_Word skips_per_row;

  /* CCD */
  SANE_Bool is_adjusted_mono_600_offset;
  SANE_Bool is_adjusted_mono_600_exposure;
  SANE_Word mono_600_exposure;

  Calibrator *red_calibrator;
  Calibrator *green_calibrator;
  Calibrator *blue_calibrator;
  Calibrator *mono_calibrator;

  SANE_Char device_name[256];

  SANE_Bool is_sensor_detected;
}
Mustek_Usb_Device;

typedef struct Mustek_Usb_Scanner
{
  /* all the state needed to define a scan request: */
  struct Mustek_Usb_Scanner *next;

  SANE_Option_Descriptor opt[NUM_OPTIONS];
  Option_Value val[NUM_OPTIONS];

  SANE_Int channels;

  /* scan window in inches: top left x+y and width+height */
  double tl_x;
  double tl_y;
  double width;
  double height;
  /* scan window in dots (at current resolution): 
     top left x+y and width+height */
  SANE_Int tl_x_dots;
  SANE_Int tl_y_dots;
  SANE_Int width_dots;
  SANE_Int height_dots;

  SANE_Word bpp;

  SANE_Bool scanning;
  SANE_Parameters params;
  SANE_Word read_rows;
  SANE_Word red_gamma_table[256];
  SANE_Word green_gamma_table[256];
  SANE_Word blue_gamma_table[256];
  SANE_Word gray_gamma_table[256];
  SANE_Word linear_gamma_table[256];
  SANE_Word *red_table;
  SANE_Word *green_table;
  SANE_Word *blue_table;
  SANE_Word *gray_table;
  SANE_Word total_bytes;
  SANE_Word total_lines;
  /* scanner dependent/low-level state: */
  Mustek_Usb_Device *hw;
}
Mustek_Usb_Scanner;


/* ------------------- calibration function declarations ------------------ */


static SANE_Status
usb_high_cal_init (Calibrator * cal, SANE_Byte type, SANE_Word target_white,
		   SANE_Word target_dark);

static SANE_Status usb_high_cal_exit (Calibrator * cal);

static SANE_Status
usb_high_cal_embed_gamma (Calibrator * cal, SANE_Word * gamma_table);

static SANE_Status
usb_high_cal_prepare (Calibrator * cal, SANE_Word max_width);

static SANE_Status
usb_high_cal_setup (Calibrator * cal, SANE_Word major_average,
		    SANE_Word minor_average, SANE_Word filter,
		    SANE_Word width, SANE_Word * white_needed,
		    SANE_Word * dark_needed);

static SANE_Status
usb_high_cal_evaluate_white (Calibrator * cal, double factor);

static SANE_Status
usb_high_cal_evaluate_dark (Calibrator * cal, double factor);

static SANE_Status usb_high_cal_evaluate_calibrator (Calibrator * cal);

static SANE_Status
usb_high_cal_fill_in_white (Calibrator * cal, SANE_Word major,
			    SANE_Word minor, void *white_pattern);

static SANE_Status
usb_high_cal_fill_in_dark (Calibrator * cal, SANE_Word major,
			   SANE_Word minor, void *dark_pattern);

static SANE_Status
usb_high_cal_calibrate (Calibrator * cal, void *src, void *target);

static SANE_Status
usb_high_cal_i8o8_fill_in_white (Calibrator * cal, SANE_Word major,
				 SANE_Word minor, void *white_pattern);

static SANE_Status
usb_high_cal_i8o8_fill_in_dark (Calibrator * cal, SANE_Word major,
				SANE_Word minor, void *dark_pattern);

static SANE_Status
usb_high_cal_i8o8_mono_calibrate (Calibrator * cal, void *src, void *target);

static SANE_Status
usb_high_cal_i8o8_rgb_calibrate (Calibrator * cal, void *src, void *target);

static SANE_Status
usb_high_cal_i4o1_fill_in_white (Calibrator * cal, SANE_Word major,
				 SANE_Word minor, void *white_pattern);

static SANE_Status
usb_high_cal_i4o1_fill_in_dark (Calibrator * cal, SANE_Word major,
				SANE_Word minor, void *dark_pattern);

static SANE_Status
usb_high_cal_i4o1_calibrate (Calibrator * cal, void *src, void *target);

/* -------------------- scanning function declarations -------------------- */

static SANE_Status usb_high_scan_init (Mustek_Usb_Device * dev);

static SANE_Status usb_high_scan_exit (Mustek_Usb_Device * dev);

static SANE_Status usb_high_scan_prepare (Mustek_Usb_Device * dev);

static SANE_Status usb_high_scan_clearup (Mustek_Usb_Device * dev);

static SANE_Status
usb_high_scan_turn_power (Mustek_Usb_Device * dev, SANE_Bool is_on);

static SANE_Status usb_high_scan_back_home (Mustek_Usb_Device * dev);

static SANE_Status
usb_high_scan_set_threshold (Mustek_Usb_Device * dev, SANE_Byte threshold);

static SANE_Status
usb_high_scan_embed_gamma (Mustek_Usb_Device * dev, SANE_Word * gamma_table);

static SANE_Status usb_high_scan_reset (Mustek_Usb_Device * dev);

static SANE_Status
usb_high_scan_suggest_parameters (Mustek_Usb_Device * dev, SANE_Word dpi,
				  SANE_Word x, SANE_Word y, SANE_Word width,
				  SANE_Word height, Colormode color_mode);
static SANE_Status usb_high_scan_detect_sensor (Mustek_Usb_Device * dev);

static SANE_Status
usb_high_scan_setup_scan (Mustek_Usb_Device * dev, Colormode color_mode,
			  SANE_Word x_dpi, SANE_Word y_dpi,
			  SANE_Bool is_invert, SANE_Word x, SANE_Word y,
			  SANE_Word width);

static SANE_Status
usb_high_scan_get_rows (Mustek_Usb_Device * dev, SANE_Byte * block,
			SANE_Word rows, SANE_Bool is_order_invert);

static SANE_Status usb_high_scan_stop_scan (Mustek_Usb_Device * dev);

static SANE_Status
usb_high_scan_step_forward (Mustek_Usb_Device * dev, SANE_Int step_count);

static SANE_Status
usb_high_scan_safe_forward (Mustek_Usb_Device * dev, SANE_Int step_count);

static SANE_Status
usb_high_scan_init_asic (Mustek_Usb_Device * dev, Sensor_Type sensor);

static SANE_Status usb_high_scan_wait_carriage_home (Mustek_Usb_Device * dev);

static SANE_Status
usb_high_scan_hardware_calibration (Mustek_Usb_Device * dev);

static SANE_Status usb_high_scan_line_calibration (Mustek_Usb_Device * dev);

static SANE_Status usb_high_scan_prepare_scan (Mustek_Usb_Device * dev);

static SANE_Word
usb_high_scan_calculate_max_rgb_600_expose (Mustek_Usb_Device * dev,
					    SANE_Byte * ideal_red_pd,
					    SANE_Byte * ideal_green_pd,
					    SANE_Byte * ideal_blue_pd);

static SANE_Word
usb_high_scan_calculate_max_mono_600_expose (Mustek_Usb_Device * dev,
					     SANE_Byte * ideal_red_pd,
					     SANE_Byte * ideal_green_pd,
					     SANE_Byte * ideal_blue_pd);

static SANE_Status
usb_high_scan_prepare_rgb_signal_600_dpi (Mustek_Usb_Device * dev);

static SANE_Status
usb_high_scan_prepare_mono_signal_600_dpi (Mustek_Usb_Device * dev);

static SANE_Word
usb_high_scan_calculate_max_rgb_300_expose (Mustek_Usb_Device * dev,
					    SANE_Byte * ideal_red_pd,
					    SANE_Byte * ideal_green_pd,
					    SANE_Byte * ideal_blue_pd);

static SANE_Word
usb_high_scan_calculate_max_mono_300_expose (Mustek_Usb_Device * dev,
					     SANE_Byte * ideal_red_pd,
					     SANE_Byte * ideal_green_pd,
					     SANE_Byte * ideal_blue_pd);

static SANE_Status
usb_high_scan_prepare_rgb_signal_300_dpi (Mustek_Usb_Device * dev);

static SANE_Status
usb_high_scan_prepare_mono_signal_300_dpi (Mustek_Usb_Device * dev);

static SANE_Status
usb_high_scan_evaluate_max_level (Mustek_Usb_Device * dev,
				  SANE_Word sample_lines,
				  SANE_Int sample_length,
				  SANE_Byte * ret_max_level);

static SANE_Status
usb_high_scan_bssc_power_delay (Mustek_Usb_Device * dev,
				Powerdelay_Function set_power_delay,
				Signal_State * signal_state,
				SANE_Byte * target, SANE_Byte max,
				SANE_Byte min, SANE_Byte threshold,
				SANE_Int length);

static SANE_Status
usb_high_scan_adjust_rgb_600_power_delay (Mustek_Usb_Device * dev);

static SANE_Status
usb_high_scan_adjust_mono_600_power_delay (Mustek_Usb_Device * dev);

static SANE_Status
usb_high_scan_adjust_mono_600_exposure (Mustek_Usb_Device * dev);

#if 0
/* CCD */
static SANE_Status
usb_high_scan_adjust_mono_600_offset (Mustek_Usb_Device * dev);

static SANE_Status
usb_high_scan_adjust_mono_600_pga (Mustek_Usb_Device * dev);

static SANE_Status
usb_high_scan_adjust_mono_600_skips_per_row (Mustek_Usb_Device * dev);
#endif

static SANE_Status
usb_high_scan_adjust_rgb_300_power_delay (Mustek_Usb_Device * dev);

static SANE_Status
usb_high_scan_adjust_mono_300_power_delay (Mustek_Usb_Device * dev);

static SANE_Status
usb_high_scan_evaluate_pixel_rate (Mustek_Usb_Device * dev);

static SANE_Status usb_high_scan_calibration_rgb_24 (Mustek_Usb_Device * dev);

static SANE_Status usb_high_scan_calibration_mono_8 (Mustek_Usb_Device * dev);

static SANE_Status usb_high_scan_prepare_rgb_24 (Mustek_Usb_Device * dev);

static SANE_Status usb_high_scan_prepare_mono_8 (Mustek_Usb_Device * dev);

static SANE_Status
usb_high_scan_get_rgb_24_bit_line (Mustek_Usb_Device * dev,
				   SANE_Byte * line,
				   SANE_Bool is_order_invert);

static SANE_Status
usb_high_scan_get_mono_8_bit_line (Mustek_Usb_Device * dev,
				   SANE_Byte * line,
				   SANE_Bool is_order_invert);

static SANE_Status usb_high_scan_backtrack_rgb_24 (Mustek_Usb_Device * dev);

static SANE_Status usb_high_scan_backtrack_mono_8 (Mustek_Usb_Device * dev);

#endif /* mustek_usb_high_h */
