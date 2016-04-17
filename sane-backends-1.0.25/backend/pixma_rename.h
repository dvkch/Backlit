/* SANE - Scanner Access Now Easy.

   Copyright (C) 2006-2007 Wittawat Yamwong <wittawat@web.de>

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
#ifndef PIXMA_RENAME_H
#define PIXMA_RENAME_H


#undef BACKEND_NAME
#define BACKEND_NAME pixma

#define pixma_cancel sanei_pixma_cancel
#define pixma_check_dpi sanei_pixma_check_dpi
#define pixma_check_result sanei_pixma_check_result
#define pixma_check_scan_param sanei_pixma_check_scan_param
#define pixma_cleanup sanei_pixma_cleanup
#define pixma_close sanei_pixma_close
#define pixma_cmd_transaction sanei_pixma_cmd_transaction
#define pixma_collect_devices sanei_pixma_collect_devices
#define pixma_connect sanei_pixma_connect
#define pixma_dbg DBG
#define pixma_disconnect sanei_pixma_disconnect
#define pixma_dump sanei_pixma_dump
#define pixma_enable_background sanei_pixma_enable_background
#define pixma_exec sanei_pixma_exec
#define pixma_exec_short_cmd sanei_pixma_exec_short_cmd
#define pixma_fill_gamma_table sanei_pixma_fill_gamma_table
#define pixma_find_scanners sanei_pixma_find_scanners
#define pixma_get_be16 sanei_pixma_get_be16
#define pixma_get_be32 sanei_pixma_get_be32
#define pixma_get_config sanei_pixma_get_config
#define pixma_get_device_config sanei_pixma_get_device_config
#define pixma_get_device_id sanei_pixma_get_device_id
#define pixma_get_device_model sanei_pixma_get_device_model
#define pixma_get_device_status sanei_pixma_get_device_status
#define pixma_get_string sanei_pixma_get_string
#define pixma_get_time sanei_pixma_get_time
#define pixma_hexdump sanei_pixma_hexdump
#define pixma_init sanei_pixma_init
#define pixma_io_cleanup sanei_pixma_io_cleanup
#define pixma_io_init sanei_pixma_io_init
#define pixma_map_status_errno sanei_pixma_map_status_errno
#define pixma_mp150_devices sanei_pixma_mp150_devices
#define pixma_mp730_devices sanei_pixma_mp730_devices
#define pixma_mp750_devices sanei_pixma_mp750_devices
#define pixma_mp810_devices sanei_pixma_mp810_devices
#define pixma_iclass_devices sanei_pixma_iclass_devices
#define pixma_newcmd sanei_pixma_newcmd
#define pixma_open sanei_pixma_open
#define pixma_print_supported_devices sanei_pixma_print_supported_devices
#define pixma_read_image sanei_pixma_read_image
#define pixma_read sanei_pixma_read
#define pixma_reset_device sanei_pixma_reset_device
#define pixma_scan sanei_pixma_scan
#define pixma_set_be16 sanei_pixma_set_be16
#define pixma_set_be32 sanei_pixma_set_be32
#define pixma_set_debug_level sanei_pixma_set_debug_level
#define pixma_set_interrupt_mode sanei_pixma_set_interrupt_mode
#define pixma_sleep sanei_pixma_sleep
#define pixma_strerror sanei_pixma_strerror
#define pixma_sum_bytes sanei_pixma_sum_bytes
#define pixma_wait_event sanei_pixma_wait_event
#define pixma_wait_interrupt sanei_pixma_wait_interrupt
#define pixma_write sanei_pixma_write


#endif
