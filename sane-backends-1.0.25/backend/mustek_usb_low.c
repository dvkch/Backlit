/* sane - Scanner Access Now Easy.

   Copyright (C) 2000 Mustek.
   Originally maintained by Tom Wang <tom.wang@mustek.com.tw>

   Copyright (C) 2001 - 2004 by Henning Meier-Geinitz.

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

#include <unistd.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei_usb.h"
#include "mustek_usb_low.h"


SANE_Status
usb_low_init (ma1017 ** chip_address)
{
  SANE_Int i;
  ma1017 *chip;

  DBG (7, "usb_low_init: start\n");
  if (!chip_address)
    return SANE_STATUS_INVAL;

  chip = (ma1017 *) malloc (sizeof (ma1017));

  if (!chip)
    {
      DBG (3, "usb_low_init: couldn't malloc %ld bytes for chip\n",
	   (long int) sizeof (ma1017));
      *chip_address = 0;
      return SANE_STATUS_NO_MEM;
    }
  *chip_address = chip;

  /* io */
  chip->is_rowing = SANE_FALSE;
  chip->is_opened = SANE_FALSE;
  chip->fd = -1;

  /* Construction/Destruction */
  chip->is_opened = SANE_FALSE;
  chip->is_rowing = SANE_FALSE;

  /* A2 */
  chip->append = 0x00;
  chip->test_sram = 0x00;
  chip->fix_pattern = 0x00;
  /* A4 */
  chip->select = 0x00;
  chip->frontend = 0x00;
  /* A6 */
  chip->rgb_sel_pin = 0x02;
  chip->asic_io_pins = 0x9c;
  /* A7 */
  chip->timing = 0xe8;
  chip->sram_bank = 0x02;
  /* A8 */
  chip->dummy_msb = 0x00;
  chip->ccd_width_msb = 0x00;
  chip->cmt_table_length = 0x00;
  /* A9 */
  chip->cmt_second_pos = 0x00;
  /* A10 + A8ID5 */
  chip->ccd_width = 0x0c80;
  /* A11 + A8ID6 */
  chip->dummy = 0x0020;
  /* A12 + A13 */
  chip->byte_width = 0x09f6;
  /* A14 + A30W */
  chip->loop_count = 0x0db5;
  /* A15 */
  chip->motor_enable = 0x00;
  chip->motor_movement = 0x60;
  chip->motor_direction = 0x10;
  chip->motor_signal = 0x00;
  chip->motor_home = 0x00;
  /* A16 */
  chip->pixel_depth = 0x00;
  chip->image_invert = 0x00;
  chip->optical_600 = 0x00;
  chip->sample_way = 0x06;
  /* A17 + A18 + A19 */
  chip->red_ref = 0xff;
  chip->green_ref = 0xff;
  chip->blue_ref = 0xff;
  /* A20 + A21 + A22 */
  chip->red_pd = 0x00;
  chip->green_pd = 0x00;
  chip->blue_pd = 0x00;
  /* A23 */
  chip->a23 = 0x80;
  /* A24 */
  chip->fy1_delay = 0x00;
  chip->special_ad = 0x00;
  /* A27 */
  chip->sclk = 0x00;
  chip->sen = 0x00;
  chip->serial_length = 0x10;

  /* Use for Rowing */
  chip->get_row = NULL;

  chip->cmt_table_length_word = 0x00000000;
  chip->cmt_second_pos_word = 0x00000000;
  chip->row_size = 0x00;
  chip->soft_resample = 0x01;
  chip->total_lines = 0x00;
  chip->lines_left = 0x00;
  for (i = 0; i < 32; i++)
    chip->is_transfer_table[i] = SANE_FALSE;
  chip->sensor = ST_CANON600;
  chip->motor = MT_1200;

  chip->total_read_urbs = 0;
  chip->total_write_urbs = 0;
  DBG (7, "usb_low_init: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_exit (ma1017 * chip)
{
  DBG (7, "usb_low_exit: chip = %p\n", (void *) chip);
  if (chip)
    {
      if (chip->fd >= 0 && chip->is_opened)
	usb_low_close (chip);
      DBG (7, "usb_low_exit: freeing chip\n");
      free (chip);
    }
  DBG (5, "usb_low_exit: read %d URBs, wrote %d URBs\n", 
       chip->total_read_urbs, chip->total_write_urbs);
  DBG (7, "usb_low_exit: exit\n");
  return SANE_STATUS_GOOD;
}

/* A0 ~ A1 */
SANE_Status
usb_low_set_cmt_table (ma1017 * chip, SANE_Int index, Channel channel,
		       SANE_Bool is_move_motor, SANE_Bool is_transfer)
{
  SANE_Byte pattern = ((SANE_Byte) index) << 4;
  SANE_Byte reg_no = 0;
  SANE_Status status;

  DBG (7, "usb_low_set_cmt_table: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_cmt_table: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_cmt_table: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }
  if ((unsigned int) index > 31)
    {
      DBG (7, "usb_low_set_cmt_table: CMT index (%d) exceed 31", index);
      return SANE_STATUS_INVAL;
    }

  switch (channel)
    {
    case CH_RED:
      pattern |= 0x04;
      break;
    case CH_GREEN:
      pattern |= 0x08;
      break;
    case CH_BLUE:
      pattern |= 0x0c;
      break;
    default:
      break;
    }
  if (is_move_motor)
    pattern |= 0x02;
  if (is_transfer)
    pattern |= 0x01;
  if (index > 15)
    reg_no++;

  RIE (usb_low_write_reg (chip, reg_no, pattern));

  chip->is_transfer_table[index] = is_transfer;

  DBG (7, "usb_low_set_cmt_table: exit\n");

  return SANE_STATUS_GOOD;
}

/* A2 */
SANE_Status
usb_low_get_a2 (ma1017 * chip, SANE_Byte * value)
{
  SANE_Byte pattern;
  SANE_Status status;

  DBG (7, "usb_low_get_a2: start\n");
  if (!chip->is_opened)
    {
      DBG (3, "usb_low_get_a2: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_get_a2: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }
  RIE (usb_low_read_reg (chip, 2, &pattern));

  chip->append = pattern & 0x10;
  chip->test_sram = pattern & 0x20;
  chip->fix_pattern = pattern & 0x80;
  if (value)
    *value = pattern;
  DBG (7, "usb_low_get_a2: exit, value =%d\n", pattern);
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_start_cmt_table (ma1017 * chip)
{
  SANE_Byte data_field[2];
  SANE_Status status;
  size_t n;

  DBG (7, "usb_low_start_cmt_table: start\n");

  data_field[0] = 0x02 | chip->append | chip->test_sram | chip->fix_pattern;
  data_field[1] = 2;

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_start_cmt_table: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (7, "usb_low_start_cmt_table: Already Rowing\n");
      return SANE_STATUS_INVAL;
    }

  data_field[1] |= 0x60;
  n = 2;
  status = sanei_usb_write_bulk (chip->fd, data_field, &n);
  if (status != SANE_STATUS_GOOD || n != 2)
    {
      DBG (3, "usb_low_start_cmt_table: can't write, wanted 2 bytes, "
	   "wrote %lu bytes\n", (unsigned long int) n);
      return SANE_STATUS_IO_ERROR;
    }
  chip->total_write_urbs++;
  chip->is_rowing = SANE_TRUE;
  DBG (7, "usb_low_start_cmt_table: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_stop_cmt_table (ma1017 * chip)
{
  SANE_Byte data_field[2];
  SANE_Byte read_byte;
  size_t n;
  SANE_Status status;

  DBG (7, "usb_low_stop_cmt_table: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_stop_cmt_table: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (!chip->is_rowing)
    {
      DBG (7, "usb_low_stop_cmt_table: Not Rowing yet\n");
      return SANE_STATUS_INVAL;
    }

  data_field[0] = 0x01 | chip->append | chip->test_sram | chip->fix_pattern;
  data_field[1] = 2;
  data_field[1] |= 0x80;
  n = 2;
  status = sanei_usb_write_bulk (chip->fd, data_field, &n);
  if (status != SANE_STATUS_GOOD || n != 2)
    {
      DBG (3, "usb_low_stop_cmt_table: couldn't write, wanted 2 bytes, wrote "
	   "%lu bytes\n", (unsigned long int) n);
      return SANE_STATUS_IO_ERROR;
    }
  chip->total_write_urbs++;
  n = 1;
  status = sanei_usb_read_bulk (chip->fd, &read_byte, &n);
  if (status != SANE_STATUS_GOOD || n != 1)
    {
      DBG (3, "usb_low_stop_cmt_table: couldn't read, wanted 1 byte, got %lu "
	   "bytes\n", (unsigned long int) n);
      return SANE_STATUS_IO_ERROR;
    }
  chip->total_read_urbs++;
  chip->is_rowing = SANE_FALSE;

  DBG (7, "usb_low_stop_cmt_table: exit\n");

  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_test_sram_mode (ma1017 * chip, SANE_Bool is_test)
{
  SANE_Byte data, reg_no;
  SANE_Status status;

  DBG (7, "usb_low_set_test_sram_mode: start\n");

  data = chip->append | chip->test_sram | chip->fix_pattern;
  reg_no = 2;

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_test_sram_mode: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_test_sram_mode: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  if (is_test)
    chip->test_sram = 0x20;
  else
    chip->test_sram = 0x00;

  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_test_sram_mode: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_fix_pattern (ma1017 * chip, SANE_Bool is_fix)
{
  SANE_Byte data, reg_no;
  SANE_Status status;

  DBG (7, "usb_low_set_fix_pattern: start\n");

  data = chip->append | chip->test_sram | chip->fix_pattern;
  reg_no = 2;

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_fix_pattern: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_fix_pattern: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  if (is_fix)
    chip->fix_pattern = 0x80;
  else
    chip->fix_pattern = 0x00;

  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_fix_pattern: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_adjust_timing (ma1017 * chip, SANE_Byte data)
{
  SANE_Status status;
  SANE_Byte reg_no;

  DBG (7, "usb_low_adjust_timing: start\n");

  reg_no = 3;

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_adjust_timing: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_adjust_timing: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_adjust_timing: exit\n");

  return SANE_STATUS_GOOD;
}

/* A4 */
SANE_Status
usb_low_get_a4 (ma1017 * chip, SANE_Byte * value)
{
  SANE_Status status;
  SANE_Byte pattern;

  DBG (7, "usb_low_get_a4: start\n");
  if (!chip->is_opened)
    {
      DBG (3, "usb_low_get_a4: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_get_a4: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  RIE (usb_low_read_reg (chip, 4, &pattern));

  chip->select = pattern & 0xfe;
  chip->frontend = pattern & 0x01;

  if (value)
    *value = pattern;

  DBG (7, "usb_low_get_a4: exit, value=%d\n", pattern);

  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_select_timing (ma1017 * chip, SANE_Byte data)
{
  SANE_Status status;
  SANE_Byte reg_no;

  DBG (7, "usb_low_select_timing: start\n");

  reg_no = 4;

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_select_timing: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_select_timing: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  chip->select = data & 0xfe;
  chip->frontend = data & 0x01;

  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_select_timing: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_turn_frontend_mode (ma1017 * chip, SANE_Bool is_on)
{
  SANE_Status status;
  SANE_Byte data, reg_no;

  DBG (7, "usb_low_turn_frontend_mode: start\n");
  if (!chip->is_opened)
    {
      DBG (3, "usb_low_turn_frontend_mode: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_turn_frontend_mode: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  if (is_on)
    chip->frontend = 0x01;
  else
    chip->frontend = 0x00;

  data = chip->select | chip->frontend;
  reg_no = 4;

  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_turn_frontend_mode: exit\n");
  return SANE_STATUS_GOOD;
}

/* A6 */
SANE_Status
usb_low_get_a6 (ma1017 * chip, SANE_Byte * value)
{
  SANE_Byte pattern;
  SANE_Status status;

  DBG (7, "usb_low_get_a6: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_get_a6: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_get_a6: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }
  RIE (usb_low_read_reg (chip, 6, &pattern));

  chip->asic_io_pins = pattern & 0xdc;
  chip->rgb_sel_pin = pattern & 0x03;

  if (value)
    *value = pattern;

  DBG (7, "usb_low_get_a6: exit\n");

  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_asic_io_pins (ma1017 * chip, SANE_Byte data)
{
  SANE_Status status;
  SANE_Byte reg_no;

  DBG (7, "usb_low_set_asic_io_pins: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_asic_io_pins: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_asic_io_pins: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  chip->asic_io_pins = data & 0xdc;

  data = chip->asic_io_pins | chip->rgb_sel_pin;
  reg_no = 6;

  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_asic_io_pins: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_rgb_sel_pins (ma1017 * chip, SANE_Byte data)
{
  SANE_Status status;
  SANE_Byte reg_no;

  DBG (7, "usb_low_set_rgb_sel_pins: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_rgb_sel_pins: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_rgb_sel_pins: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }
  chip->rgb_sel_pin = data & 0x03;
  data = chip->asic_io_pins | chip->rgb_sel_pin;
  reg_no = 6;

  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_rgb_sel_pins: exit\n");
  return SANE_STATUS_GOOD;	/* was false? */
}

/* A7 */
SANE_Status
usb_low_get_a7 (ma1017 * chip, SANE_Byte * value)
{
  SANE_Byte pattern;
  SANE_Status status;

  DBG (7, "usb_low_get_a7: start\n");
  if (!chip->is_opened)
    {
      DBG (3, "usb_low_get_a7: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_get_a7: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }
  RIE (usb_low_read_reg (chip, 7, &pattern));

  if (value)
    *value = pattern;

  chip->timing = pattern & 0xfc;
  chip->sram_bank = pattern & 0x03;

  DBG (7, "usb_low_get_a7: exit\n");

  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_timing (ma1017 * chip, SANE_Byte data)
{
  SANE_Status status;
  SANE_Byte reg_no;

  DBG (7, "usb_low_set_timing: start\n");
  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_timing: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_timing: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  chip->timing = data & 0xfc;
  data = chip->timing | chip->sram_bank;
  reg_no = 7;

  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_timing: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_sram_bank (ma1017 * chip, Banksize banksize)
{
  SANE_Status status;
  SANE_Byte data, reg_no;

  DBG (7, "usb_low_set_sram_bank: start\n");
  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_sram_bank: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_sram_bank: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  switch (banksize)
    {
    case BS_4K:
      chip->sram_bank = 0x00;
      break;
    case BS_8K:
      chip->sram_bank = 0x01;
      break;
    case BS_16K:
      chip->sram_bank = 0x02;
      break;
    default:
      DBG (3, "usb_low_set_sram_bank: bsBankSize error\n");
      return SANE_STATUS_INVAL;
      break;
    }
  data = chip->timing | chip->sram_bank;
  reg_no = 7;

  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_sram_bank: exit\n");
  return SANE_STATUS_GOOD;
}

/* A8 */
SANE_Status
usb_low_get_a8 (ma1017 * chip, SANE_Byte * value)
{
  SANE_Byte pattern;
  SANE_Status status;

  DBG (7, "usb_low_get_a8: start\n");
  if (!chip->is_opened)
    {
      DBG (3, "usb_low_get_a8: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_get_a8: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  RIE (usb_low_read_reg (chip, 8, &pattern));

  chip->dummy_msb = pattern & 0x40;
  chip->ccd_width_msb = pattern & 0x20;
  chip->cmt_table_length = pattern & 0x1f;
  chip->ccd_width =
    ((chip->ccd_width / 32) & 0x00ff) * 32 +
    ((chip->ccd_width_msb == 0) ? 0 : 0x0100 * 32);
  chip->dummy =
    ((chip->dummy / 32) & 0x00ff) * 32 +
    ((chip->dummy_msb == 0) ? 0 : 0x0100 * 32);

  if (value)
    *value = pattern;

  DBG (7, "usb_low_get_a8: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_cmt_table_length (ma1017 * chip, SANE_Byte table_length)
{
  SANE_Status status;
  SANE_Byte data, reg_no;

  DBG (7, "usb_low_set_cmt_table_length: start\n");
  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_cmt_table_length: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_cmt_table_length: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }
  if (table_length > 32)
    {
      DBG (3, "usb_low_set_cmt_table_length: length %d exceeds 32\n",
	   (int) table_length);
      return SANE_STATUS_INVAL;
    }
  if (table_length == 0)
    {
      DBG (3, "usb_low_set_cmt_table_length: length is 0\n");
      return SANE_STATUS_INVAL;
    }

  chip->cmt_table_length = table_length - 1;
  chip->cmt_table_length_word = (SANE_Word) table_length;
  data = chip->cmt_table_length | chip->ccd_width_msb | chip->dummy_msb;
  reg_no = 8;

  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_cmt_table_length: exit\n");
  return SANE_STATUS_GOOD;
}

/* A9 */
SANE_Status
usb_low_get_a9 (ma1017 * chip, SANE_Byte * value)
{
  SANE_Byte pattern;
  SANE_Status status;

  DBG (7, "usb_low_get_a9: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_get_a9: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_get_a9: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }
  RIE (usb_low_read_reg (chip, 9, &pattern));

  chip->cmt_second_pos = pattern & 0x1f;

  if (value)
    *value = pattern;

  DBG (7, "usb_low_get_a9: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_cmt_second_position (ma1017 * chip, SANE_Byte position)
{
  SANE_Byte data, reg_no;
  SANE_Status status;

  DBG (7, "usb_low_set_cmt_second_position: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_cmt_second_position: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_cmt_second_position: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }
  if (position > 31)
    {
      DBG (3, "usb_low_set_cmt_second_position: length: %d exceeds 31\n",
	   (int) position);
      return SANE_STATUS_INVAL;
    }

  chip->cmt_second_pos = position;
  chip->cmt_second_pos_word = (SANE_Word) (position);
  data = chip->cmt_second_pos;
  reg_no = 9;

  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_cmt_second_position: exit\n");

  return SANE_STATUS_GOOD;
}

/* A10 + A8ID5 */
SANE_Status
usb_low_get_a10 (ma1017 * chip, SANE_Byte * value)
{
  SANE_Byte pattern;
  SANE_Status status;

  DBG (7, "usb_low_get_a10: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_get_a10: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_get_a10: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  RIE (usb_low_read_reg (chip, 10, &pattern));

  chip->ccd_width =
    ((SANE_Word) (pattern)) * 32 +
    ((chip->ccd_width_msb == 0) ? 0 : 0x0100 * 32);

  if (value)
    *value = pattern;

  DBG (7, "usb_low_get_a10: exit\n");

  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_ccd_width (ma1017 * chip, SANE_Word ccd_width)
{
  SANE_Status status;
  SANE_Byte data, reg_no;

  DBG (7, "usb_low_set_ccd_width: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_ccd_width: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_ccd_width: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }
  if (ccd_width / 32 > 0x01ff)
    {
      DBG (3, "usb_low_set_ccd_width: width %d too high\n", (int) ccd_width);
      return SANE_STATUS_INVAL;
    }

  chip->ccd_width = ccd_width;
  ccd_width /= 32;
  if (HIBYTE (ccd_width) == 0x01)
    chip->ccd_width_msb = 0x20;
  else
    chip->ccd_width_msb = 0x00;

  data = chip->cmt_table_length | chip->ccd_width_msb | chip->dummy_msb;
  reg_no = 8;
  RIE (usb_low_write_reg (chip, reg_no, data));

  data = LOBYTE (ccd_width);
  reg_no = 10;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_ccd_width: exit\n");
  return SANE_STATUS_GOOD;
}

/* A11 + A8ID6 */
SANE_Status
usb_low_get_a11 (ma1017 * chip, SANE_Byte * value)
{
  SANE_Byte pattern;
  SANE_Status status;

  DBG (7, "usb_low_get_a11: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_get_a11: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_get_a11: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  RIE (usb_low_read_reg (chip, 11, &pattern));

  chip->dummy =
    ((SANE_Word) (pattern)) * 32 + ((chip->dummy_msb == 0) ? 0 : 0x0100 * 32);

  if (value)
    *value = pattern;

  DBG (7, "usb_low_get_a11: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_dummy (ma1017 * chip, SANE_Word dummy)
{
  SANE_Status status;
  SANE_Byte data, reg_no;

  DBG (7, "usb_low_set_dummy: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_dummy: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_dummy: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }
  if (dummy / 32 > 0x01ff)
    {
      DBG (7, "usb_low_set_dummy: width %d exceeded\n", (int) dummy);
      return SANE_STATUS_INVAL;
    }

  chip->dummy = dummy;
  dummy /= 32;
  dummy++;
  if (HIBYTE (dummy) == 0x01)
    chip->dummy_msb = 0x40;
  else
    chip->dummy_msb = 0x00;
  data = chip->cmt_table_length | chip->ccd_width_msb | chip->dummy_msb;
  reg_no = 8;
  RIE (usb_low_write_reg (chip, reg_no, data));

  data = LOBYTE (dummy);
  reg_no = 11;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_dummy: exit\n");
  return SANE_STATUS_GOOD;
}

/* A12 + A13 */
SANE_Status
usb_low_get_a12 (ma1017 * chip, SANE_Byte * value)
{
  SANE_Byte pattern;
  SANE_Status status;

  DBG (7, "usb_low_get_a12: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_get_a12: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_get_a12: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  RIE (usb_low_read_reg (chip, 12, &pattern));

  chip->byte_width = (chip->byte_width & 0x3f00) + ((SANE_Word) pattern);
  chip->soft_resample = (chip->soft_resample == 0) ? 1 : chip->soft_resample;
  chip->get_row =
    (chip->soft_resample == 1)
    ? &usb_low_get_row_direct : &usb_low_get_row_resample;
  chip->row_size = chip->byte_width / chip->soft_resample;

  if (value)
    *value = pattern;

  DBG (7, "usb_low_get_a12: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_get_a13 (ma1017 * chip, SANE_Byte * value)
{
  SANE_Byte pattern;
  SANE_Status status;

  DBG (7, "usb_low_get_a13: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_get_a13: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_get_a13: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  RIE (usb_low_read_reg (chip, 13, &pattern));

  chip->byte_width =
    (chip->byte_width & 0x00ff) + (((SANE_Word) (pattern & 0x3f)) << 8);
  chip->soft_resample = (chip->soft_resample == 0) ? 1 : chip->soft_resample;
  chip->get_row =
    (chip->soft_resample ==
     1) ? &usb_low_get_row_direct : &usb_low_get_row_resample;
  chip->row_size = chip->byte_width / chip->soft_resample;

  if (value)
    *value = pattern;

  DBG (7, "usb_low_get_a13: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_image_byte_width (ma1017 * chip, SANE_Word row_size)
{
  SANE_Byte data, reg_no;
  SANE_Status status;

  DBG (7, "usb_low_set_image_byte_width: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_image_byte_width: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_image_byte_width: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  chip->row_size = row_size;
  chip->soft_resample = (chip->soft_resample == 0) ? 1 : chip->soft_resample;
  chip->get_row = (chip->soft_resample == 1) ? &usb_low_get_row_direct
    : &usb_low_get_row_resample;
  chip->byte_width = chip->row_size * chip->soft_resample;
  if (chip->byte_width > 0x3fff)
    {
      DBG (3, "usb_low_set_image_byte_width: width %d exceeded\n",
	   (int) chip->byte_width);
      return SANE_STATUS_INVAL;
    }

  data = LOBYTE (chip->byte_width);
  reg_no = 12;
  RIE (usb_low_write_reg (chip, reg_no, data));

  data = HIBYTE (chip->byte_width);
  reg_no = 13;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_image_byte_width: exit\n");

  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_soft_resample (ma1017 * chip, SANE_Word soft_resample)
{
  SANE_Byte data, reg_no;
  SANE_Status status;

  DBG (7, "usb_low_set_soft_resample: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_soft_resample: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_soft_resample: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }
  if (soft_resample == 0x00)
    {
      DBG (3, "usb_low_set_soft_resample: soft_resample==0\n");
      return SANE_STATUS_INVAL;
    }

  chip->soft_resample = soft_resample;
  chip->get_row = (chip->soft_resample == 1) ? &usb_low_get_row_direct
    : &usb_low_get_row_resample;
  chip->byte_width = chip->row_size * chip->soft_resample;
  if (chip->byte_width > 0x3fff)
    {
      DBG (3, "usb_low_set_soft_resample: width %d exceeded",
	   (int) chip->byte_width);
      return SANE_STATUS_INVAL;
    }

  data = LOBYTE (chip->byte_width);
  reg_no = 12;
  RIE (usb_low_write_reg (chip, reg_no, data));

  data = HIBYTE (chip->byte_width);
  reg_no = 13;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_soft_resample: exit\n");
  return SANE_STATUS_GOOD;
}

/* A14 + A30W */
SANE_Status
usb_low_set_cmt_loop_count (ma1017 * chip, SANE_Word loop_count)
{
  SANE_Byte data, reg_no;
  SANE_Status status;

  DBG (7, "usb_low_set_cmt_loop_count: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_cmt_loop_count: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_cmt_loop_count: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  chip->loop_count = loop_count;

  data = LOBYTE (loop_count);
  reg_no = 14;
  RIE (usb_low_write_reg (chip, reg_no, data));

  data = HIBYTE (loop_count);
  reg_no = 30;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_cmt_loop_count: exit\n");
  return SANE_STATUS_GOOD;
}

/* A15 */
SANE_Status
usb_low_get_a15 (ma1017 * chip, SANE_Byte * value)
{
  SANE_Byte pattern;
  SANE_Status status;

  DBG (7, "usb_low_get_a15: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_get_a15: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_get_a15: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  RIE (usb_low_read_reg (chip, 15, &pattern));

  chip->motor_enable = pattern & 0x80;
  chip->motor_movement = pattern & 0x68;
  chip->motor_direction = pattern & 10;
  chip->motor_signal = pattern & 0x06;
  chip->motor_home = pattern & 0x01;

  if (value)
    *value = pattern;

  DBG (7, "usb_low_get_a15: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_enable_motor (ma1017 * chip, SANE_Bool is_enable)
{
  SANE_Byte data, reg_no;
  SANE_Status status;

  DBG (7, "usb_low_enable_motor: start\n");
  if (!chip->is_opened)
    {
      DBG (3, "usb_low_enable_motor: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_enable_motor: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  chip->motor_enable = 0x00;
  if (is_enable)
    chip->motor_enable |= 0x80;
  data = chip->motor_enable | chip->motor_movement
    | chip->motor_direction | chip->motor_signal | chip->motor_home;
  reg_no = 15;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_enable_motor: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_motor_movement (ma1017 * chip, SANE_Bool is_full_step,
			    SANE_Bool is_double_phase, SANE_Bool is_two_step)
{
  SANE_Byte data, reg_no;
  SANE_Status status;

  DBG (7, "usb_low_set_motor_movement: start\n");
  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_motor_movement: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_motor_movement: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  chip->motor_movement = 0x00;
  if (is_full_step)
    chip->motor_movement |= 0x40;
  if (is_double_phase)
    chip->motor_movement |= 0x20;
  if (is_two_step)
    chip->motor_movement |= 0x08;
  data = chip->motor_enable | chip->motor_movement
    | chip->motor_direction | chip->motor_signal | chip->motor_home;
  reg_no = 15;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_motor_movement:  exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_motor_direction (ma1017 * chip, SANE_Bool is_backward)
{
  SANE_Byte data, reg_no;
  SANE_Status status;

  DBG (7, "usb_low_set_motor_direction: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_motor_direction: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_motor_direction: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  chip->motor_direction = 0x00;
  if (is_backward)
    chip->motor_direction |= 0x10;
  data = chip->motor_enable | chip->motor_movement
    | chip->motor_direction | chip->motor_signal | chip->motor_home;
  reg_no = 15;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_motor_direction: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_motor_signal (ma1017 * chip, SANE_Byte signal)
{
  SANE_Byte data, reg_no;
  SANE_Status status;

  DBG (7, "usb_low_set_motor_signal: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_motor_signal: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_motor_signal: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  chip->motor_signal = signal & 0x06;
  data = chip->motor_enable | chip->motor_movement
    | chip->motor_direction | chip->motor_signal | chip->motor_home;
  reg_no = 15;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_motor_signal: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_move_motor_home (ma1017 * chip, SANE_Bool is_home,
			 SANE_Bool is_backward)
{
  SANE_Byte data, reg_no;
  SANE_Status status;

  DBG (7, "usb_low_move_motor_home: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_move_motor_home: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_move_motor_home: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  chip->motor_enable = 0x00;
  chip->motor_direction = 0x00;
  chip->motor_home = 0x00;
  if (is_backward)
    chip->motor_direction |= 0x10;
  if (is_home)
    {
      chip->motor_enable |= 0x80;
      chip->motor_home |= 0x01;
    }
  data = chip->motor_enable | chip->motor_movement
    | chip->motor_direction | chip->motor_signal | chip->motor_home;
  reg_no = 15;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_move_motor_home: exit\n");
  return SANE_STATUS_GOOD;
}

/* A16 */
SANE_Status
usb_low_get_a16 (ma1017 * chip, SANE_Byte * value)
{
  SANE_Byte pattern;
  SANE_Status status;

  DBG (7, "usb_low_get_a16: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_get_a16: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_get_a16: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  RIE (usb_low_read_reg (chip, 16, &pattern));

  chip->pixel_depth = pattern & 0xe0;
  chip->image_invert = pattern & 0x10;
  chip->optical_600 = pattern & 0x08;
  chip->sample_way = pattern & 0x07;

  if (value)
    *value = pattern;

  DBG (7, "usb_low_get_a16: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_image_dpi (ma1017 * chip, SANE_Bool is_optical600,
		       Sampleway sampleway)
{
  SANE_Byte data, reg_no;
  SANE_Status status;

  DBG (7, "usb_low_set_image_dpi: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_image_dpi: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_image_dpi: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  chip->optical_600 = 0x00;
  chip->sample_way = 0x00;
  if (is_optical600)
    chip->optical_600 |= 0x08;
  switch (sampleway)
    {
    case SW_P1P6:
      chip->sample_way = 0x01;
      break;
    case SW_P2P6:
      chip->sample_way = 0x02;
      break;
    case SW_P3P6:
      chip->sample_way = 0x03;
      break;
    case SW_P4P6:
      chip->sample_way = 0x04;
      break;
    case SW_P5P6:
      chip->sample_way = 0x05;
      break;
    case SW_P6P6:
      chip->sample_way = 0x06;
      break;
    default:
      DBG (3, "usb_low_set_image_dpi: swsample_way error\n");
      return SANE_STATUS_INVAL;
      break;
    }
  data = chip->pixel_depth | chip->image_invert | chip->optical_600
    | chip->sample_way;
  reg_no = 16;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_image_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_pixel_depth (ma1017 * chip, Pixeldepth pixeldepth)
{
  SANE_Byte data, reg_no;
  SANE_Status status;

  DBG (7, "usb_low_set_pixel_depth: start\n");
  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_pixel_depth: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_pixel_depth: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  chip->pixel_depth = 0x00;
  switch (pixeldepth)
    {
    case PD_1BIT:
      chip->pixel_depth = 0x80;
      break;
    case PD_4BIT:
      chip->pixel_depth = 0xc0;
      break;
    case PD_8BIT:
      chip->pixel_depth = 0x00;
      break;
    case PD_12BIT:
      chip->pixel_depth = 0x20;
      break;
    default:
      DBG (3, "usb_low_set_pixel_depth: pdPixelDepth error\n");
      return SANE_STATUS_INVAL;
    }
  data = chip->pixel_depth | chip->image_invert | chip->optical_600
    | chip->sample_way;
  reg_no = 16;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_SetPixelDeepth: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_invert_image (ma1017 * chip, SANE_Bool is_invert)
{
  SANE_Byte data, reg_no;
  SANE_Status status;

  DBG (7, "usb_low_invert_image: start\n");
  if (!chip->is_opened)
    {
      DBG (3, "usb_low_invert_image: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_invert_image: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  chip->image_invert = 0x00;
  if (is_invert)
    chip->image_invert |= 0x10;
  data = chip->pixel_depth | chip->image_invert | chip->optical_600
    | chip->sample_way;
  reg_no = 16;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_invert_image: exit\n");
  return SANE_STATUS_GOOD;
}

/* A17 + A18 + A19 */
SANE_Status
usb_low_get_a17 (ma1017 * chip, SANE_Byte * value)
{
  SANE_Byte pattern;
  SANE_Status status;

  DBG (7, "usb_low_get_a17: start\n");
  if (!chip->is_opened)
    {
      DBG (3, "usb_low_get_a17: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_get_a17: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  RIE (usb_low_read_reg (chip, 17, &pattern));

  chip->red_ref = pattern;

  if (value)
    *value = pattern;

  DBG (7, "usb_low_get_a17: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_get_a18 (ma1017 * chip, SANE_Byte * value)
{
  SANE_Byte pattern;
  SANE_Status status;

  DBG (7, "usb_low_get_a18: start\n");
  if (!chip->is_opened)
    {
      DBG (3, "usb_low_get_a18: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_get_a18: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  RIE (usb_low_read_reg (chip, 18, &pattern));

  chip->green_ref = pattern;

  if (value)
    *value = pattern;

  DBG (7, "usb_low_get_a18: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_get_a19 (ma1017 * chip, SANE_Byte * value)
{
  SANE_Byte pattern;
  SANE_Status status;

  DBG (7, "usb_low_get_a19: start\n");
  if (!chip->is_opened)
    {
      DBG (3, "usb_low_get_a19: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_get_a19:stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  RIE (usb_low_read_reg (chip, 19, &pattern));

  chip->blue_ref = pattern;

  if (value)
    *value = pattern;

  DBG (7, "usb_low_get_a19: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_red_ref (ma1017 * chip, SANE_Byte red_ref)
{
  SANE_Byte data, reg_no;
  SANE_Status status;

  DBG (7, "usb_low_set_red_ref: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_red_ref: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_red_ref: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  chip->red_ref = red_ref;
  data = red_ref;
  reg_no = 17;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_red_ref: stop\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_green_ref (ma1017 * chip, SANE_Byte green_ref)
{
  SANE_Byte data, reg_no;
  SANE_Status status;

  DBG (7, "usb_low_set_green_ref: start\n");


  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_green_ref: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_green_ref: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  chip->green_ref = green_ref;

  data = green_ref;
  reg_no = 18;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_green_ref: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_blue_ref (ma1017 * chip, SANE_Byte blue_ref)
{
  SANE_Byte data, reg_no;
  SANE_Status status;

  DBG (7, "usb_low_set_blue_ref: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_blue_ref: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_blue_ref: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  chip->blue_ref = blue_ref;

  data = blue_ref;
  reg_no = 19;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_blue_ref: stop\n");
  return SANE_STATUS_GOOD;
}

/* A20 + A21 + A22 */
SANE_Status
usb_low_get_a20 (ma1017 * chip, SANE_Byte * value)
{
  SANE_Byte pattern;
  SANE_Status status;

  DBG (7, "usb_low_get_a20: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_get_a20: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_get_a20: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }
  RIE (usb_low_read_reg (chip, 20, &pattern));

  chip->red_pd = pattern;

  if (value)
    *value = pattern;

  DBG (7, "usb_low_get_a20: stop\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_get_a21 (ma1017 * chip, SANE_Byte * value)
{
  SANE_Byte pattern;
  SANE_Status status;

  DBG (7, "usb_low_get_a21: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_get_a21: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_get_a21: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  RIE (usb_low_read_reg (chip, 21, &pattern));

  chip->green_pd = pattern;

  if (value)
    *value = pattern;

  DBG (7, "usb_low_get_a21: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_get_a22 (ma1017 * chip, SANE_Byte * value)
{
  SANE_Byte pattern;
  SANE_Status status;

  DBG (7, "usb_low_get_a22: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_get_a22: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_get_a22: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  RIE (usb_low_read_reg (chip, 22, &pattern));

  chip->blue_pd = pattern;

  if (value)
    *value = pattern;

  DBG (7, "usb_low_get_a22: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_red_pd (ma1017 * chip, SANE_Byte red_pd)
{
  SANE_Byte data, reg_no;
  SANE_Status status;

  DBG (7, "usb_low_set_red_pd: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_red_pd: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_red_pd: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  chip->red_pd = red_pd;

  data = chip->red_pd;
  reg_no = 20;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_red_pd: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_green_pd (ma1017 * chip, SANE_Byte green_pd)
{
  SANE_Byte data, reg_no;
  SANE_Status status;

  DBG (7, "usb_low_set_green_pd: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_green_pd: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_green_pd: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  chip->green_pd = green_pd;
  data = chip->green_pd;
  reg_no = 21;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_green_pd: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_blue_pd (ma1017 * chip, SANE_Byte blue_pd)
{
  SANE_Byte data, reg_no;
  SANE_Status status;

  DBG (7, "usb_low_set_blue_pd: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_blue_pd: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_blue_pd: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  chip->blue_pd = blue_pd;

  data = chip->blue_pd;
  reg_no = 22;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_blue_pd: exit\n");
  return SANE_STATUS_GOOD;
}

/* A23 */
SANE_Status
usb_low_get_a23 (ma1017 * chip, SANE_Byte * value)
{
  SANE_Byte pattern;
  SANE_Status status;

  DBG (7, "usb_low_get_a23: start\n");
  if (!chip->is_opened)
    {
      DBG (3, "usb_low_get_a23: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_get_a23: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  RIE (usb_low_read_reg (chip, 23, &pattern));

  chip->a23 = pattern;

  if (value)
    *value = pattern;

  DBG (7, "usb_low_get_a23: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_turn_peripheral_power (ma1017 * chip, SANE_Bool is_on)
{
  SANE_Byte data, reg_no;
  SANE_Status status;

  DBG (7, "usb_low_turn_peripheral_power: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_turn_peripheral_power: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_turn_peripheral_power: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  chip->a23 &= 0x7f;
  if (is_on)
    chip->a23 |= 0x80;
  data = chip->a23;
  reg_no = 23;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_turn_peripheral_power: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_turn_lamp_power (ma1017 * chip, SANE_Bool is_on)
{
  SANE_Byte data, reg_no;
  SANE_Status status;

  DBG (7, "usb_low_turn_lamp_power: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_turn_lamp_power: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_turn_lamp_power: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  chip->a23 &= 0xbf;
  if (is_on)
    chip->a23 |= 0x40;

  data = chip->a23;
  reg_no = 23;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_turn_lamp_power: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_io_3 (ma1017 * chip, SANE_Bool is_high)
{
  SANE_Byte data, reg_no;
  SANE_Status status;

  DBG (7, "usb_low_set_io_3: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_io_3: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_io_3: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  chip->a23 &= 0xf7;
  if (is_high)
    chip->a23 |= 0x08;

  data = chip->a23;
  reg_no = 23;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_io_3: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_led_light_all (ma1017 * chip, SANE_Bool is_light_all)
{
  SANE_Byte data, reg_no;
  SANE_Status status;

  DBG (7, "usb_low_set_led_light_all: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_led_light_all: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_led_light_all: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  chip->a23 &= 0xfe;
  if (is_light_all)
    chip->a23 |= 0x01;

  data = chip->a23;
  reg_no = 23;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_led_light_all: exit\n");
  return SANE_STATUS_GOOD;
}

/* A24 */
SANE_Status
usb_low_get_a24 (ma1017 * chip, SANE_Byte * value)
{
  SANE_Byte pattern;
  SANE_Status status;

  DBG (7, "usb_low_get_a24: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_get_a24: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_get_a24: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  RIE (usb_low_read_reg (chip, 24, &pattern));

  chip->fy1_delay = pattern & 0x01;
  chip->special_ad = pattern & 0x02;

  if (value)
    *value = pattern;

  DBG (7, "usb_low_get_a24: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_ad_timing (ma1017 * chip, SANE_Byte data)
{
  SANE_Byte reg_no;
  SANE_Status status;

  DBG (7, "usb_low_set_ad_timing: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_ad_timing: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_ad_timing: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  chip->fy1_delay = data & 0x01;
  chip->special_ad = data & 0x02;

  data = chip->special_ad | chip->fy1_delay;
  reg_no = 24;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_ad_timing: exit\n");
  return SANE_STATUS_GOOD;
}

/* A25 + A26 */
SANE_Status
usb_low_set_serial_byte1 (ma1017 * chip, SANE_Byte data)
{
  SANE_Byte reg_no;
  SANE_Status status;

  DBG (7, "usb_low_set_serial_byte1: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_serial_byte1: not opened\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_serial_byte1: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  reg_no = 25;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_serial_byte1: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_serial_byte2 (ma1017 * chip, SANE_Byte data)
{
  SANE_Byte reg_no;
  SANE_Status status;

  DBG (7, "usb_low_set_serial_byte2: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_serial_byte2: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_serial_byte2: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  reg_no = 26;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_serial_byte2: exit\n");
  return SANE_STATUS_GOOD;
}

/* A27 */
SANE_Status
usb_low_get_a27 (ma1017 * chip, SANE_Byte * value)
{
  SANE_Byte pattern;
  SANE_Status status;

  DBG (7, "usb_low_get_a27: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_get_a27: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_get_a27: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  RIE (usb_low_read_reg (chip, 27, &pattern));

  chip->sclk = pattern & 0x80;
  chip->sen = pattern & 0x40;
  chip->serial_length = pattern & 0x1f;

  if (value)
    *value = pattern;

  DBG (7, "usb_low_get_a27: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_set_serial_format (ma1017 * chip, SANE_Byte data)
{
  SANE_Byte reg_no;
  SANE_Status status;

  DBG (7, "usb_low_set_serial_format: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_set_serial_format: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_set_serial_format: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }


  chip->sclk = data & 0x80;
  chip->sen = data & 0x40;
  chip->serial_length = data & 0x1f;

  reg_no = 27;
  RIE (usb_low_write_reg (chip, reg_no, data));

  DBG (7, "usb_low_set_serial_format: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_get_home_sensor (ma1017 * chip)
{
  SANE_Byte data;
  SANE_Status status;

  DBG (7, "usb_low_get_home_sensor: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_get_home_sensor: not opened yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_get_home_sensor: stop rowing first\n");
      return SANE_STATUS_INVAL;
    }

  RIE (usb_low_read_reg (chip, 31, &data));

  DBG (7, "usb_low_get_home_sensor: exit\n");
  if ((data & 0x80) != 0)
    return SANE_STATUS_GOOD;
  else
    return SANE_STATUS_IO_ERROR;
}

/* Special Mode */
SANE_Status
usb_low_start_rowing (ma1017 * chip)
{
  SANE_Word line_of_first = 0;
  SANE_Word line_of_second = 0;
  SANE_Int i;
  SANE_Status status;

  DBG (7, "usb_low_start_rowing: start\n");

  if (chip->loop_count == 0)
    {
      DBG (3, "usb_low_start_rowing loop_count hasn't been set yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->cmt_table_length_word == 0)
    {
      DBG (3, "usb_low_start_rowing: cmt_table_length_word hasn't been set "
	   "yet\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->cmt_table_length_word <= chip->cmt_second_pos_word)
    {
      DBG (3, "usb_low_start_rowing: cmt_second_pos_word cannot be larger "
	   "than cmt_table_length_word\n");
      return SANE_STATUS_INVAL;
    }

  for (i = 0; i < (int) chip->cmt_second_pos_word; i++)
    {
      if (chip->is_transfer_table[i])
	line_of_first++;
    }
  for (; i < (int) chip->cmt_table_length_word; i++)
    {
      if (chip->is_transfer_table[i])
	{
	  line_of_first++;
	  line_of_second++;
	}
    }

  chip->total_lines =
    ((SANE_Word) (chip->loop_count - 1)) * line_of_second + line_of_first;
  chip->lines_left = chip->total_lines;

  RIE (usb_low_start_cmt_table (chip));

  DBG (7, "usb_low_start_rowing: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_stop_rowing (ma1017 * chip)
{
  SANE_Status status;

  DBG (7, "usb_low_stop_rowing: start\n");

  RIE (usb_low_stop_cmt_table (chip));

  DBG (7, "usb_low_stop_rowing: exit\n");
  return SANE_STATUS_GOOD;

}

SANE_Status
usb_low_wait_rowing_stop (ma1017 * chip)
{
  SANE_Status status;

  DBG (7, "usb_low_wait_rowing_stop: start\n");
  if (chip->total_lines != 0)
    {
      DBG (3, "usb_low_wait_rowing_stop: total_lines must be 0\n");
      return SANE_STATUS_INVAL;
    }

  RIE (usb_low_wait_rowing (chip));
  chip->is_rowing = SANE_FALSE;
  DBG (7, "usb_low_wait_rowing_stop: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_read_all_registers (ma1017 * chip)
{
  SANE_Status status;

  DBG (7, "usb_low_read_all_registers: start\n");

  RIE (usb_low_get_a2 (chip, 0));
  RIE (usb_low_get_a4 (chip, 0));
  RIE (usb_low_get_a6 (chip, 0));
  RIE (usb_low_get_a7 (chip, 0));
  RIE (usb_low_get_a8 (chip, 0));
  RIE (usb_low_get_a9 (chip, 0));
  RIE (usb_low_get_a10 (chip, 0));
  RIE (usb_low_get_a11 (chip, 0));
  RIE (usb_low_get_a12 (chip, 0));
  RIE (usb_low_get_a13 (chip, 0));
  RIE (usb_low_get_a15 (chip, 0));
  RIE (usb_low_get_a16 (chip, 0));
  RIE (usb_low_get_a17 (chip, 0));
  RIE (usb_low_get_a18 (chip, 0));
  RIE (usb_low_get_a19 (chip, 0));
  RIE (usb_low_get_a20 (chip, 0));
  RIE (usb_low_get_a21 (chip, 0));
  RIE (usb_low_get_a22 (chip, 0));
  RIE (usb_low_get_a23 (chip, 0));
  RIE (usb_low_get_a24 (chip, 0));
  RIE (usb_low_get_a27 (chip, 0));

  return SANE_STATUS_GOOD;
  DBG (7, "usb_low_read_all_registers: exit\n");
}

SANE_Status
usb_low_get_row (ma1017 * chip, SANE_Byte * data, SANE_Word * lines_left)
{
  SANE_Status status;

  DBG (7, "usb_low_get_row: start\n");
  RIE ((*chip->get_row) (chip, data, lines_left));
  DBG (7, "usb_low_get_row: exit\n");
  return SANE_STATUS_GOOD;;
}

SANE_Status
usb_low_get_row_direct (ma1017 * chip, SANE_Byte * data,
			SANE_Word * lines_left)
{
  SANE_Status status;

  DBG (7, "usb_low_get_row_direct: start\n");
  if (chip->lines_left == 0)
    {
      DBG (3, "usb_low_get_row_direct: lines_left == 0\n");
      return SANE_STATUS_INVAL;
    }

  if (chip->lines_left <= 1)
    {
      RIE (usb_low_read_rows (chip, data, chip->byte_width));
      RIE (usb_low_wait_rowing (chip));

      chip->lines_left = 0x00;
      chip->is_rowing = SANE_FALSE;
      *lines_left = 0;
    }
  else
    {
      RIE (usb_low_read_rows (chip, data, chip->byte_width));
      chip->lines_left--;
      *lines_left = chip->lines_left;
    }
  DBG (7, "usb_low_get_row_direct: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_get_row_resample (ma1017 * chip, SANE_Byte * data,
			  SANE_Word * lines_left)
{
  static SANE_Byte resample_buffer[8 * 1024];
  SANE_Word *pixel_temp;
  SANE_Word i;
  SANE_Word j;
  SANE_Word k;
  SANE_Status status;

  DBG (7, "usb_low_get_row_resample: start\n");

  if (chip->lines_left == 0)
    {
      DBG (3, "usb_low_get_row_resample: lines_left == 0\n");
      return SANE_STATUS_INVAL;
    }

  if (chip->lines_left <= 1)
    {
      RIE (usb_low_read_rows (chip, resample_buffer, chip->byte_width));

      if ((chip->sensor == ST_CANON600) && (chip->pixel_depth == 0x20))
	{
	  pixel_temp = (SANE_Word *) malloc (6 * 1024 * sizeof (SANE_Word));
	  if (!pixel_temp)
	    return SANE_STATUS_NO_MEM;

	  j = 0;
	  for (i = 0; i < chip->byte_width; i += 3)
	    {
	      pixel_temp[j] = (SANE_Word) resample_buffer[i];
	      pixel_temp[j] |= ((SANE_Word)
				(resample_buffer[i + 1] & 0xf0)) << 4;
	      j++;
	      pixel_temp[j] = ((SANE_Word)
			       (resample_buffer[i + 1] & 0x0f)) << 8;
	      pixel_temp[j] |= (SANE_Word) resample_buffer[i + 2];
	      j++;
	    }

	  k = 0;
	  for (i = 0; i < j; i += chip->soft_resample * 2)
	    {
	      data[k] = (SANE_Byte) (pixel_temp[i] & 0x00ff);
	      k++;
	      data[k] = (SANE_Byte) ((pixel_temp[i] & 0x0f00) >> 4);
	      data[k] |= (SANE_Byte) ((pixel_temp[i + 2] & 0x0f00) >> 8);
	      k++;
	      data[k] = (SANE_Byte) (pixel_temp[i + 2] & 0x00ff);
	      k++;
	    }
	  free (pixel_temp);
	}
      else			/* fixme ? */
	{
	  for (i = 0; i < chip->byte_width; i += chip->soft_resample)
	    *(data++) = resample_buffer[i];
	}
      RIE (usb_low_wait_rowing (chip));

      chip->lines_left = 0x00;
      chip->is_rowing = SANE_FALSE;
      *lines_left = 0;
    }
  else
    {
      RIE (usb_low_read_rows (chip, resample_buffer, chip->byte_width));

      if ((chip->sensor == ST_CANON600) && (chip->pixel_depth == 0x20))
	{
	  pixel_temp = (SANE_Word *) malloc (6 * 1024 * sizeof (SANE_Word));
	  if (!pixel_temp)
	    return SANE_STATUS_NO_MEM;

	  j = 0;
	  for (i = 0; i < chip->byte_width; i += 3)
	    {
	      pixel_temp[j] = (SANE_Word) resample_buffer[i];
	      pixel_temp[j] |=
		((SANE_Word) (resample_buffer[i + 1] & 0xf0)) << 4;
	      j++;
	      pixel_temp[j] =
		((SANE_Word) (resample_buffer[i + 1] & 0x0f)) << 8;
	      pixel_temp[j] |= (SANE_Word) resample_buffer[i + 2];
	      j++;
	    }

	  k = 0;
	  for (i = 0; i < j; i += chip->soft_resample * 2)
	    {
	      data[k] = (SANE_Byte) (pixel_temp[i] & 0x00ff);
	      k++;
	      data[k] = (SANE_Byte) ((pixel_temp[i] & 0x0f00) >> 4);
	      data[k] |= (SANE_Byte) ((pixel_temp[i + 2] & 0x0f00) >> 8);
	      k++;
	      data[k] = (SANE_Byte) (pixel_temp[i + 2] & 0x00ff);
	      k++;
	    }
	  free (pixel_temp);
	}
      else			/* fixme? */
	{
	  for (i = 0; i < chip->byte_width; i += chip->soft_resample)
	    *(data++) = resample_buffer[i];
	}
      chip->lines_left--;
      *lines_left = chip->lines_left;
    }
  DBG (7, "usb_low_get_row_resample: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_wait_rowing (ma1017 * chip)
{
  SANE_Byte read_byte;
  size_t n;
  SANE_Status status;

  DBG (7, "usb_low_wait_rowing: start\n");

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_wait_rowing: open first\n");
      return SANE_STATUS_INVAL;
    }
  if (!chip->is_rowing)
    {
      DBG (3, "usb_low_wait_rowing: not rowing\n");
      return SANE_STATUS_INVAL;
    }
  n = 1;
  status = sanei_usb_read_bulk (chip->fd, (SANE_Byte *) & read_byte, &n);
  if (status != SANE_STATUS_GOOD || n != 1)
    {
      DBG (3, "usb_low_wait_rowing: couldn't read: %s\n",
	   sane_strstatus (status));
      return SANE_STATUS_IO_ERROR;
    }
  chip->total_read_urbs++;
  chip->is_rowing = SANE_FALSE;
  DBG (7, "usb_low_wait_rowing: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_read_rows (ma1017 * chip, SANE_Byte * data, SANE_Word byte_count)
{
  size_t n, bytes_total;
  SANE_Status status;

  DBG (7, "usb_low_read_rows: start\n");
  if (!(chip->is_opened))
    {
      DBG (3, "usb_low_read_rows: is_opened==SANE_FALSE\n");
      return SANE_STATUS_INVAL;
    }
  if (!(chip->is_rowing))
    {
      DBG (3, "usb_low_read_rows: is_rowing==SANE_FALSE\n");
      return SANE_STATUS_INVAL;
    }

  n = MIN (byte_count, chip->max_block_size);
  bytes_total = 0;

  while ((SANE_Word) bytes_total < byte_count)
    {
      status =
	sanei_usb_read_bulk (chip->fd, (SANE_Byte *) (data + bytes_total),
			     &n);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (7, "usb_low_read_rows: problems during read: %s -- exiting\n",
	       sane_strstatus (status));
	  return status;
	}
      /* Count the number of URBs. This is a bit tricky, as we are reading
	 bigger chunks here but the scanner can only handle 64 bytes at once. */
      chip->total_read_urbs += ((n +  63) / 64);
      bytes_total += n;
      if ((SANE_Word) bytes_total != byte_count)
	{
	  DBG (7, "usb_low_read_rows:  wanted %d, got %d "
	       "bytes (%d in total) -- retrying\n", byte_count, (SANE_Word) n,
	       (SANE_Word) bytes_total);
	}
      n = MIN ((byte_count - (SANE_Word) bytes_total), chip->max_block_size);
    }

  DBG (7, "usb_low_read_rows: exit, read %d bytes\n",
       (SANE_Word) bytes_total);
  return SANE_STATUS_GOOD;
}


SANE_Status
usb_low_write_reg (ma1017 * chip, SANE_Byte reg_no, SANE_Byte data)
{
  size_t n;
  SANE_Status status;
  SANE_Byte data_field[2];

  data_field[0] = data;
  data_field[1] = reg_no;

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_write_reg: open first\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_write_reg: rowing, stop first\n");
      return SANE_STATUS_INVAL;
    }
  if (reg_no > 0x20)
    {
      DBG (3, "usb_low_write_reg: reg_no out of range\n");
      return SANE_STATUS_INVAL;
    }
  n = 2;
  status = sanei_usb_write_bulk (chip->fd, data_field, &n);
  if (status != SANE_STATUS_GOOD || n != 2)
    {
      DBG (3, "usb_low_write_reg: couldn't write, tried to write %d, "
	   "wrote %lu: %s\n", 2, (unsigned long int) n,
	   sane_strstatus (status));
      return SANE_STATUS_IO_ERROR;
    }
  chip->total_write_urbs++;
  DBG (7, "usb_low_write_reg: reg: 0x%02x, value: 0x%02x\n", reg_no, data);
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_read_reg (ma1017 * chip, SANE_Byte reg_no, SANE_Byte * data)
{
  SANE_Byte data_field[2];
  SANE_Byte read_byte;
  size_t n;
  SANE_Status status;

  data_field[0] = 0x00;
  data_field[1] = reg_no | 0x20;

  if (!chip->is_opened)
    {
      DBG (3, "usb_low_read_reg: open first\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_rowing)
    {
      DBG (3, "usb_low_read_reg: rowing, stop first\n");
      return SANE_STATUS_INVAL;
    }
  if (reg_no > 0x20)
    {
      DBG (3, "usb_low_read_reg: reg_no out of range\n");
      return SANE_STATUS_INVAL;
    }
  n = 2;
  DBG (5, "usb_low_read_reg: trying to write %lu bytes\n", (unsigned long int) n);

  status = sanei_usb_write_bulk (chip->fd, data_field, &n);
  if (status != SANE_STATUS_GOOD || n != 2)
    {
      DBG (3, "usb_low_read_reg: couldn't write, tried to write %d, "
	   "wrote %lu: %s\n", 2, (unsigned long int) n,
	   sane_strstatus (status));
      return SANE_STATUS_IO_ERROR;
    }
  chip->total_write_urbs++;
  n = 1;
  DBG (5, "usb_low_read_reg: trying to read %lu bytes\n", (unsigned long int) n);
  status = sanei_usb_read_bulk (chip->fd, (SANE_Byte *) & read_byte, &n);
  if (status != SANE_STATUS_GOOD || n != 1)
    {
      DBG (3, "usb_low_read_reg: couldn't read, tried to read %lu, "
	   "read %lu: %s\n", (unsigned long int) 1,
	   (unsigned long int) n, sane_strstatus (status));
      return SANE_STATUS_IO_ERROR;
    }
  chip->total_read_urbs++;
  if (data)
    *data = read_byte;
  DBG (7, "usb_low_read_reg: Reg: 0x%02x, Value: 0x%02x\n",
       reg_no, read_byte);
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_identify_scanner (SANE_Int fd, Mustek_Type * scanner_type)
{
  SANE_Status status;
  SANE_Word devvendor, devproduct;
  Mustek_Type devtype;

  DBG (7, "usb_low_identify_scanner: start\n");

  status = sanei_usb_get_vendor_product (fd, &devvendor, &devproduct);
  devtype = MT_UNKNOWN;
  if (status == SANE_STATUS_GOOD)
    {
      if (devvendor == 0x055f)
	{
	  switch (devproduct)
	    {
	    case 0x0001:
	      devtype = MT_1200CU;
	      break;
	    case 0x0002:
	      devtype = MT_600CU;
	      break;
	    case 0x0003:
	      devtype = MT_1200USB;
	      break;
	    case 0x0006:
	      devtype = MT_1200UB;
	      break;
	    case 0x0008:
	      devtype = MT_1200CU_PLUS;
	      break;
	    case 0x0873:
	      devtype = MT_600USB;
	      break;
	    default:
	      if (scanner_type)
		*scanner_type = devtype;
	      DBG (3, "usb_low_identify_scanner: unknown product id: "
		   "0x%04x\n", devproduct);
	      return SANE_STATUS_INVAL;
	      break;
	    }
	}
      else
	{
	  if (scanner_type)
	    *scanner_type = devtype;
	  DBG (3, "usb_low_identify_scanner: unknown vendor id: 0x%04d\n",
	       devvendor);
	  return SANE_STATUS_INVAL;
	}
    }
  if (scanner_type)
    *scanner_type = devtype;
  DBG (7, "usb_low_identify_scanner: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_open (ma1017 * chip, SANE_String_Const devname)
{
  SANE_Status status;
  Mustek_Type scanner_type;

  DBG (7, "usb_low_open: start: chip = %p\n", (void *) chip);

  if (chip->is_rowing)
    {
      DBG (3, "usb_low_open: already rowing\n");
      return SANE_STATUS_INVAL;
    }
  if (chip->is_opened)
    {
      DBG (3, "usb_low_open: already opened\n");
      return SANE_STATUS_INVAL;
    }

  status = sanei_usb_open ((SANE_String_Const) devname, &chip->fd);

  if (status == SANE_STATUS_GOOD)
    {
      DBG (7, "usb_low_open: device %s successfully opened\n", devname);
      chip->is_opened = SANE_TRUE;
      /* Try to get vendor and device ids */
      DBG (7, "usb_low_open: trying to identify device `%s'\n", devname);
      status = usb_low_identify_scanner (chip->fd, &scanner_type);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (3, "usb_low_open: device `%s' doesn't look like a supported "
	       "scanner\n", devname);
	  sanei_usb_close (chip->fd);
	  return status;
	}
      else
	{
	  if (scanner_type == MT_UNKNOWN)
	    {
	      DBG (3, "usb_low_open: device `%s' can't be identified\n",
		   devname);
	    }
	  else if (scanner_type != chip->scanner_type)
	    {
	      DBG (3, "usb_low_open: device `%s' is supported but"
		   "it's not the same as at the start\n", devname);
	      return SANE_STATUS_INVAL;
	    }
	}
    }
  else
    {
      DBG (1, "usb_low_open: device %s couldn't be opened: %s\n",
	   devname, sane_strstatus (status));
      return status;
    }

  chip->is_opened = SANE_TRUE;

  RIE (usb_low_read_all_registers (chip));

  DBG (7, "usb_low_open: exit, type is %d\n", scanner_type);
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_low_close (ma1017 * chip)
{
  DBG (7, "usb_low_close: start, chip=%p\n", (void *) chip);
  if (!chip->is_opened)
    {
      DBG (3, "usb_low_close: already close or never opened\n");
      return SANE_STATUS_INVAL;
    }

  if (chip->fd >= 0)
    {
      SANE_Byte dummy;

      if (chip->is_rowing)
	usb_low_stop_rowing (chip);
      /* Now make sure that both the number of written and
	 read URBs is even. Use some dummy writes/reads. That's to avoid
	 a nasty bug in the MA 1017 chipset that causes timeouts when
	 the number of URBs is odd (toggle bug). */
      if ((chip->total_read_urbs % 2) == 1)
	usb_low_get_a4 (chip, &dummy);
      if ((chip->total_write_urbs % 2) == 1)
	usb_low_set_fix_pattern (chip, SANE_FALSE);
      sanei_usb_close (chip->fd);
      chip->fd = -1;
    }
  chip->is_opened = SANE_FALSE;
  chip->is_rowing = SANE_FALSE;

  DBG (7, "usb_low_close: exit\n");
  return SANE_STATUS_GOOD;
}
