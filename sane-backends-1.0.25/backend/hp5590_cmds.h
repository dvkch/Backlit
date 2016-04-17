/* sane - Scanner Access Now Easy.
   Copyright (C) 2007 Ilia Sotnikov <hostcc@gmail.com>
   HP ScanJet 4570c support by Markham Thomas
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

   This file is part of a SANE backend for
   HP ScanJet 4500C/4570C/5500C/5550C/5590/7650 Scanners
*/

#ifndef HP5590_H
#define HP5590_H

#include "hp5590_low.h"

#define TMA_MAX_X_INCHES	1.69
#define TMA_MAX_Y_INCHES	6

#define ADF_MAX_Y_INCHES	14

enum hp_scanner_types
{
  SCANNER_NONE = 0,
  SCANNER_HP4570,
  SCANNER_HP5550,
  SCANNER_HP5590,
  SCANNER_HP7650
};

enum scan_sources
{
  SOURCE_NONE = 1,
  SOURCE_FLATBED,
  SOURCE_ADF,
  SOURCE_ADF_DUPLEX,
  SOURCE_TMA_NEGATIVES,
  SOURCE_TMA_SLIDES
};

enum scan_modes
{
  MODE_NORMAL = 1,
  MODE_PREVIEW
};

enum color_depths
{
  DEPTH_BW = 1,
  DEPTH_GRAY,
  DEPTH_COLOR_24,
  DEPTH_COLOR_48
};

enum button_status
{
  BUTTON_NONE = 1,
  BUTTON_POWER,
  BUTTON_SCAN,
  BUTTON_COLLECT,
  BUTTON_FILE,
  BUTTON_EMAIL,
  BUTTON_COPY,
  BUTTON_UP,
  BUTTON_DOWN,
  BUTTON_MODE,
  BUTTON_CANCEL
};

enum hp5590_lamp_state
{
  LAMP_STATE_TURNOFF = 1,
  LAMP_STATE_TURNON,
  LAMP_STATE_SET_TURNOFF_TIME,
  LAMP_STATE_SET_TURNOFF_TIME_LONG
};

struct hp5590_model
{
  enum hp_scanner_types	scanner_type;
  unsigned int 		usb_vendor_id;
  unsigned int 		usb_product_id;
  const char 		*vendor_id;
  const char 		*model;
  const char 		*kind;
  enum proto_flags	proto_flags;
};

#define FEATURE_NONE	     0
#define FEATURE_ADF	1 << 0
#define FEATURE_TMA	1 << 1
#define FEATURE_LCD	1 << 2

struct scanner_info
{
  const char 	*model;
  const char 	*kind;
  unsigned int 	features;
  const char 	*fw_version;
  unsigned int 	max_dpi_x;
  unsigned int 	max_dpi_y;
  unsigned int 	max_pixels_x;
  unsigned int 	max_pixels_y;
  float 	max_size_x;
  float 	max_size_y;
  unsigned int 	max_motor_param;
  unsigned int 	normal_motor_param;
};

static SANE_Status hp5590_model_def (enum hp_scanner_types scanner_type,
				     const struct hp5590_model ** model);
static SANE_Status hp5590_vendor_product_id (enum hp_scanner_types scanner_type,
				      SANE_Word * vendor_id,
				      SANE_Word * product_id);
static SANE_Status hp5590_init_scanner (SANE_Int dn,
				 enum proto_flags proto_flags,
				 struct scanner_info **info,
				 enum hp_scanner_types scanner_type);
static SANE_Status hp5590_power_status (SANE_Int dn,
					enum proto_flags proto_flags);
static SANE_Status hp5590_read_max_scan_count (SANE_Int dn,
					enum proto_flags proto_flags,
					unsigned int *max_count);
static SANE_Status hp5590_select_source_and_wakeup (SANE_Int dn,
					     enum proto_flags proto_flags,
					     enum scan_sources source,
					     SANE_Bool extend_lamp_timeout);
static SANE_Status hp5590_stop_scan (SANE_Int dn,
				enum proto_flags proto_flags);
static SANE_Status hp5590_read_scan_count (SANE_Int dn,
				    enum proto_flags proto_flags,
				    unsigned int *count);
static SANE_Status hp5590_set_scan_params (SANE_Int dn,
				    enum proto_flags proto_flags,
				    struct scanner_info *scanner_info,
				    unsigned int top_x, unsigned int top_y,
				    unsigned int width, unsigned int height,
				    unsigned int dpi,
				    enum color_depths color_depth,
				    enum scan_modes scan_mode,
				    enum scan_sources scan_source);
static SANE_Status hp5590_send_forward_calibration_maps (SANE_Int dn,
					enum proto_flags proto_flags);
static SANE_Status hp5590_send_reverse_calibration_map (SANE_Int dn,
					enum proto_flags proto_flags);
static SANE_Status hp5590_inc_scan_count (SANE_Int dn,
					enum proto_flags proto_flags);
static SANE_Status hp5590_start_scan (SANE_Int dn,
				enum proto_flags proto_flags);
static SANE_Status hp5590_read (SANE_Int dn,
			 enum proto_flags proto_flags,
			 unsigned char *bytes,
			 unsigned int size, void *state);
static SANE_Status hp5590_read_buttons (SANE_Int dn,
				 enum proto_flags proto_flags,
				 enum button_status *status);
static SANE_Status hp5590_read_part_number (SANE_Int dn,
				enum proto_flags proto_flags);
static SANE_Status hp5590_calc_pixel_bits (unsigned int dpi,
				    enum color_depths color_depth,
				    unsigned int *pixel_bits);
static SANE_Status hp5590_is_data_available (SANE_Int dn,
				enum proto_flags proto_flags);
static SANE_Status hp5590_reset_scan_head (SANE_Int dn,
				enum proto_flags proto_flags);
#endif /* HP5590_H */
/* vim: sw=2 ts=8
 */
