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

#ifndef mustek_usb_low_h
#define mustek_usb_low_h

#include "../include/sane/sane.h"


/* ---------------------------------- macros ------------------------------ */


/* calculate the minimum/maximum values */
#if defined(MIN)
#undef MIN
#endif
#if defined(MAX)
#undef MAX
#endif
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
/* return the lower/upper 8 bits of a 16 bit word */
#define HIBYTE(w) ((SANE_Byte)(((SANE_Word)(w) >> 8) & 0xFF))
#define LOBYTE(w) ((SANE_Byte)(w))
/* RIE: return if error */
#define RIE(function) do {status = function; if (status != SANE_STATUS_GOOD) \
                        return status;} while (SANE_FALSE)


/* ---------------------------------- types ------------------------------- */


typedef enum Mustek_Type
{
  MT_UNKNOWN = 0,
  MT_1200USB,
  MT_1200UB,
  MT_1200CU,
  MT_1200CU_PLUS,
  MT_600CU,
  MT_600USB
}
Mustek_Type;

typedef enum Sensor_Type
{
  ST_NONE = 0,
  ST_INI = 1,
  ST_INI_DARK = 2,
  ST_CANON300 = 3,
  ST_CANON600 = 4,
  ST_TOSHIBA600 = 5,
  ST_CANON300600 = 6,
  ST_NEC600 = 7
}
Sensor_Type;

typedef enum Motor_Type
{
  MT_NONE = 0,
  MT_600 = 1,
  MT_1200 = 2
}
Motor_Type;

struct ma1017;

typedef struct ma1017
{
  int fd;

  SANE_Bool is_opened;
  SANE_Bool is_rowing;

  /* A2 */
  SANE_Byte append;
  SANE_Byte test_sram;
  SANE_Byte fix_pattern;
  /* A4 */
  SANE_Byte select;
  SANE_Byte frontend;
  /* A6 */
  SANE_Byte rgb_sel_pin;
  SANE_Byte asic_io_pins;
  /* A7 */
  SANE_Byte timing;
  SANE_Byte sram_bank;
  /* A8 */
  SANE_Byte dummy_msb;
  SANE_Byte ccd_width_msb;
  SANE_Byte cmt_table_length;
  /* A9 */
  SANE_Byte cmt_second_pos;
  /* A10 + A8ID5 */
  SANE_Word ccd_width;
  /* A11 + A8ID6 */
  SANE_Word dummy;
  /* A12 + A13 */
  SANE_Word byte_width;
  /* A14 + A30W */
  SANE_Word loop_count;
  /* A15 */
  SANE_Byte motor_enable;
  SANE_Byte motor_movement;
  SANE_Byte motor_direction;
  SANE_Byte motor_signal;
  SANE_Byte motor_home;
  /* A16 */
  SANE_Byte pixel_depth;
  SANE_Byte image_invert;
  SANE_Byte optical_600;
  SANE_Byte sample_way;
  /* A17 + A18 + A19 */
  SANE_Byte red_ref;
  SANE_Byte green_ref;
  SANE_Byte blue_ref;
  /* A20 + A21 + A22 */
  SANE_Byte red_pd;
  SANE_Byte green_pd;
  SANE_Byte blue_pd;
  /* A23 */
  SANE_Byte a23;
  /* A24 */
  SANE_Byte fy1_delay;
  SANE_Byte special_ad;
  /* A27 */
  SANE_Byte sclk;
  SANE_Byte sen;
  SANE_Byte serial_length;

  /* Use for Rowing */
    SANE_Status (*get_row) (struct ma1017 * chip, SANE_Byte * row,
			    SANE_Word * lines_left);

  SANE_Word cmt_table_length_word;
  SANE_Word cmt_second_pos_word;
  SANE_Word row_size;
  SANE_Word soft_resample;
  SANE_Word total_lines;
  SANE_Word lines_left;
  SANE_Bool is_transfer_table[32];
  Sensor_Type sensor;
  Motor_Type motor;
  Mustek_Type scanner_type;
  SANE_Word max_block_size;

  SANE_Word total_read_urbs;
  SANE_Word total_write_urbs;
}
ma1017;

typedef enum Channel
{
  CH_NONE = 0,
  CH_RED = 1,
  CH_GREEN = 2,
  CH_BLUE = 3
}
Channel;

typedef enum Banksize
{
  BS_NONE = 0,
  BS_4K = 1,
  BS_8K = 2,
  BS_16K = 3
}
Banksize;

typedef enum Pixeldepth
{
  PD_NONE = 0,
  PD_1BIT = 1,
  PD_4BIT = 2,
  PD_8BIT = 3,
  PD_12BIT = 4
}
Pixeldepth;

typedef enum Sampleway
{
  SW_NONE = 0,
  SW_P1P6 = 1,
  SW_P2P6 = 2,
  SW_P3P6 = 3,
  SW_P4P6 = 4,
  SW_P5P6 = 5,
  SW_P6P6 = 6
}
Sampleway;

/* ------------------------- function declarations ------------------------ */

static SANE_Status usb_low_init (ma1017 ** chip);

static SANE_Status usb_low_exit (ma1017 * chip);

/* Register read and write functions */
/* A0 ~ A1 */
static SANE_Status
usb_low_set_cmt_table (ma1017 * chip, SANE_Int index, Channel channel,
		       SANE_Bool is_move_motor, SANE_Bool is_transfer);

/* A2 */
static SANE_Status usb_low_get_a2 (ma1017 * chip, SANE_Byte * value);

static SANE_Status usb_low_start_cmt_table (ma1017 * chip);

static SANE_Status usb_low_stop_cmt_table (ma1017 * chip);

static SANE_Status
usb_low_set_test_sram_mode (ma1017 * chip, SANE_Bool is_test);

static SANE_Status usb_low_set_fix_pattern (ma1017 * chip, SANE_Bool is_fix);

/* A3 */
static SANE_Status usb_low_adjust_timing (ma1017 * chip, SANE_Byte data);

/* A4 */
static SANE_Status usb_low_get_a4 (ma1017 * chip, SANE_Byte * value);

static SANE_Status usb_low_select_timing (ma1017 * chip, SANE_Byte data);

static SANE_Status
usb_low_turn_frontend_mode (ma1017 * chip, SANE_Bool is_on);

/* A6 */
static SANE_Status usb_low_get_a6 (ma1017 * chip, SANE_Byte * value);

static SANE_Status usb_low_set_asic_io_pins (ma1017 * chip, SANE_Byte data);

static SANE_Status usb_low_set_rgb_sel_pins (ma1017 * chip, SANE_Byte data);

/* A7 */
static SANE_Status usb_low_get_a7 (ma1017 * chip, SANE_Byte * value);

static SANE_Status usb_low_set_timing (ma1017 * chip, SANE_Byte data);

static SANE_Status usb_low_set_sram_bank (ma1017 * chip, Banksize banksize);

/* A8 */
static SANE_Status usb_low_get_a8 (ma1017 * chip, SANE_Byte * value);

static SANE_Status
usb_low_set_cmt_table_length (ma1017 * chip, SANE_Byte table_length);

/* A9 */
static SANE_Status usb_low_get_a9 (ma1017 * chip, SANE_Byte * value);

static SANE_Status
usb_low_set_cmt_second_position (ma1017 * chip, SANE_Byte position);

/* A10 + A8ID5 */
static SANE_Status usb_low_get_a10 (ma1017 * chip, SANE_Byte * value);

static SANE_Status usb_low_set_ccd_width (ma1017 * chip, SANE_Word ccd_width);

/* A11 + A8ID6 */
static SANE_Status usb_low_get_a11 (ma1017 * chip, SANE_Byte * value);

static SANE_Status usb_low_set_dummy (ma1017 * chip, SANE_Word dummy);

/* A12 + A13 */
static SANE_Status usb_low_get_a12 (ma1017 * chip, SANE_Byte * value);

static SANE_Status usb_low_get_a13 (ma1017 * chip, SANE_Byte * value);

static SANE_Status
usb_low_set_image_byte_width (ma1017 * chip, SANE_Word row_size);

static SANE_Status
usb_low_set_soft_resample (ma1017 * chip, SANE_Word soft_resample);

/* A14 + A30W */
static SANE_Status
usb_low_set_cmt_loop_count (ma1017 * chip, SANE_Word loop_count);

/* A15 */
static SANE_Status usb_low_get_a15 (ma1017 * chip, SANE_Byte * value);

static SANE_Status usb_low_enable_motor (ma1017 * chip, SANE_Bool is_enable);

static SANE_Status
usb_low_set_motor_movement (ma1017 * chip, SANE_Bool is_full_step,
			    SANE_Bool is_double_phase, SANE_Bool is_two_step);

static SANE_Status usb_low_set_motor_signal (ma1017 * chip, SANE_Byte signal);

static SANE_Status
usb_low_set_motor_direction (ma1017 * chip, SANE_Bool is_backward);

static SANE_Status
usb_low_move_motor_home (ma1017 * chip, SANE_Bool is_home,
			 SANE_Bool is_backward);

/* A16 */
static SANE_Status usb_low_get_a16 (ma1017 * chip, SANE_Byte * value);

static SANE_Status
usb_low_set_image_dpi (ma1017 * chip, SANE_Bool is_optical600,
		       Sampleway sampleway);

static SANE_Status
usb_low_set_pixel_depth (ma1017 * chip, Pixeldepth pixeldepth);

static SANE_Status usb_low_invert_image (ma1017 * chip, SANE_Bool is_invert);

/* A17 + A18 + A19 */
static SANE_Status usb_low_get_a17 (ma1017 * chip, SANE_Byte * value);

static SANE_Status usb_low_get_a18 (ma1017 * chip, SANE_Byte * value);

static SANE_Status usb_low_get_a19 (ma1017 * chip, SANE_Byte * value);

static SANE_Status usb_low_set_red_ref (ma1017 * chip, SANE_Byte red_ref);

static SANE_Status usb_low_set_green_ref (ma1017 * chip, SANE_Byte green_ref);

static SANE_Status usb_low_set_blue_ref (ma1017 * chip, SANE_Byte blue_ref);

/* A20 + A21 + A22 */
static SANE_Status usb_low_get_a20 (ma1017 * chip, SANE_Byte * value);

static SANE_Status usb_low_get_a21 (ma1017 * chip, SANE_Byte * value);

static SANE_Status usb_low_get_a22 (ma1017 * chip, SANE_Byte * value);

static SANE_Status usb_low_set_red_pd (ma1017 * chip, SANE_Byte red_pd);

static SANE_Status usb_low_set_green_pd (ma1017 * chip, SANE_Byte green_pd);

static SANE_Status usb_low_set_blue_pd (ma1017 * chip, SANE_Byte blue_pd);

/* A23 */
static SANE_Status usb_low_get_a23 (ma1017 * chip, SANE_Byte * value);

static SANE_Status
usb_low_turn_peripheral_power (ma1017 * chip, SANE_Bool is_on);

static SANE_Status usb_low_turn_lamp_power (ma1017 * chip, SANE_Bool is_on);

static SANE_Status usb_low_set_io_3 (ma1017 * chip, SANE_Bool is_high);

static SANE_Status
usb_low_set_led_light_all (ma1017 * chip, SANE_Bool is_light_all);

/* A24 */
static SANE_Status usb_low_get_a24 (ma1017 * chip, SANE_Byte * value);

static SANE_Status usb_low_set_ad_timing (ma1017 * chip, SANE_Byte pattern);

/* A25 + A26 */
static SANE_Status usb_low_set_serial_byte1 (ma1017 * chip, SANE_Byte data);

static SANE_Status usb_low_set_serial_byte2 (ma1017 * chip, SANE_Byte data);

/* A27 */
static SANE_Status usb_low_get_a27 (ma1017 * chip, SANE_Byte * value);

static SANE_Status usb_low_set_serial_format (ma1017 * chip, SANE_Byte data);

/* A31 */
static SANE_Status usb_low_get_home_sensor (ma1017 * chip);

/* Special Mode */
static SANE_Status usb_low_start_rowing (ma1017 * chip);

static SANE_Status usb_low_stop_rowing (ma1017 * chip);

static SANE_Status usb_low_wait_rowing_stop (ma1017 * chip);

/* Global functions */
static SANE_Status usb_low_read_all_registers (ma1017 * chip);

static SANE_Status
usb_low_get_row (ma1017 * chip, SANE_Byte * data, SANE_Word * lines_left);

static SANE_Status
usb_low_get_row_direct (ma1017 * chip, SANE_Byte * data,
			SANE_Word * lines_left);

static SANE_Status
usb_low_get_row_resample (ma1017 * chip, SANE_Byte * data,
			  SANE_Word * lines_left);

/* Direct access */
static SANE_Status usb_low_wait_rowing (ma1017 * chip);

static SANE_Status
usb_low_read_rows (ma1017 * chip, SANE_Byte * data, SANE_Word byte_count);

static SANE_Status
usb_low_write_reg (ma1017 * chip, SANE_Byte reg_no, SANE_Byte data);

static SANE_Status
usb_low_read_reg (ma1017 * chip, SANE_Byte reg_no, SANE_Byte * data);

static SANE_Status
usb_low_identify_scanner (SANE_Int fd, Mustek_Type * scanner_type);

static SANE_Status usb_low_open (ma1017 * chip, const char *devname);

static SANE_Status usb_low_close (ma1017 * chip);

#endif /* defined mustek_usb_low_h */
