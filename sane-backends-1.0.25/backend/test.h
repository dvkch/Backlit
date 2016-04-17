/* sane - Scanner Access Now Easy.

   Copyright (C) 2002-2006 Henning Meier-Geinitz <henning@meier-geinitz.de>

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

   This backend is for testing frontends.
*/

#ifndef test_h
#define test_h


typedef enum
{
  param_none = 0,
  param_bool,
  param_int,
  param_fixed,
  param_string
}
parameter_type;

typedef enum
{
  opt_num_opts = 0,
  opt_mode_group,
  opt_mode,
  opt_depth,
  opt_hand_scanner,
  opt_three_pass,		/* 5 */
  opt_three_pass_order,
  opt_resolution,
  opt_scan_source,
  opt_special_group,
  opt_test_picture,
  opt_invert_endianess,
  opt_read_limit,
  opt_read_limit_size,
  opt_read_delay,
  opt_read_delay_duration,
  opt_read_status_code,
  opt_ppl_loss,
  opt_fuzzy_parameters,
  opt_non_blocking,
  opt_select_fd,
  opt_enable_test_options,
  opt_print_options,
  opt_geometry_group,
  opt_tl_x,
  opt_tl_y,
  opt_br_x,
  opt_br_y,
  opt_bool_group,
  opt_bool_soft_select_soft_detect,
  opt_bool_hard_select_soft_detect,
  opt_bool_hard_select,
  opt_bool_soft_detect,
  opt_bool_soft_select_soft_detect_emulated,
  opt_bool_soft_select_soft_detect_auto,
  opt_int_group,
  opt_int,
  opt_int_constraint_range,
  opt_int_constraint_word_list,
  opt_int_array,
  opt_int_array_constraint_range,
  opt_int_array_constraint_word_list,
  opt_fixed_group,
  opt_fixed,
  opt_fixed_constraint_range,
  opt_fixed_constraint_word_list,
  opt_string_group,
  opt_string,
  opt_string_constraint_string_list,
  opt_string_constraint_long_string_list,
  opt_button_group,
  opt_button,
  /* must come last: */
  num_options
}
test_opts;


typedef struct Test_Device
{
  struct Test_Device *next;
  SANE_Device sane;
  SANE_Option_Descriptor opt[num_options];
  Option_Value val[num_options];
  SANE_Bool loaded[num_options];
  SANE_Parameters params;
  SANE_String name;
  SANE_Pid reader_pid;
  SANE_Int reader_fds;
  SANE_Int pipe;
  FILE *pipe_handle;
  SANE_Word pass;
  SANE_Word bytes_per_line;
  SANE_Word pixels_per_line;
  SANE_Word lines;
  SANE_Int bytes_total;
  SANE_Bool open;
  SANE_Bool scanning;
  SANE_Bool cancelled;
  SANE_Bool eof;
  SANE_Int number_of_scans;
}
Test_Device;

#endif /* test_h */
