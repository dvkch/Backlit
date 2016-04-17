/* sane - Scanner Access Now Easy.

   ScanMaker 3840 Backend
   Copyright (C) 2005-7 Earle F. Philhower, III
   earle@ziplabel.com - http://www.ziplabel.com

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

#include <stdio.h>
#include <stdarg.h>

#ifndef BACKENDNAME
#include <usb.h>
#include <string.h>

#ifndef DEFINED_USB_HANDLE
#define DEFINED_USB_HANDLE
typedef usb_dev_handle *p_usb_dev_handle;
#endif

static p_usb_dev_handle find_device (unsigned int idVendor,
				    unsigned int idProduct);
#else
#include "../include/sane/sanei_usb.h"

#ifndef USBWRAPPER
#define USBWRAPPER

typedef SANE_Int p_usb_dev_handle;
#define usb_control_msg  my_usb_control_msg
#define usb_bulk_read    my_usb_bulk_read
#define usb_bulk_write   my_usb_bulk_write
static int my_usb_bulk_write (p_usb_dev_handle dev, int ep,
			      unsigned char *bytes,
			      int size, int timeout);
static int my_usb_bulk_read (p_usb_dev_handle dev, int ep,
			     unsigned char *bytes,
			     int size, int timeout);
static int my_usb_control_msg (p_usb_dev_handle dev, int requesttype,
			       int request, int value, int index,
			       unsigned char *bytes,
			       int size, int timeout);
#endif /* USBWRAPPER */

#endif

#include "sm3840_params.h"

static void idle_ab (p_usb_dev_handle udev);
static void write_regs (p_usb_dev_handle udev, int regs, unsigned char reg1,
			unsigned char val1,
			... /*unsigned char reg, unsigned char val, ... */ );
static int write_vctl (p_usb_dev_handle udev, int request, int value,
		       int index, unsigned char byte);
static int read_vctl (p_usb_dev_handle udev, int request, int value,
		      int index, unsigned char *byte);

#ifndef BACKENDNAME
static void record (p_usb_dev_handle udev, char *fname, int bytes);
static void record_image (p_usb_dev_handle udev, char *fname, int dpi,
			  int scanpix, int scanlines, int gray, char *head,
			  int bpp16);
static void check_buttons (p_usb_dev_handle udev, int *scan, int *print,
			   int *mail);
static void record_head (p_usb_dev_handle udev, char *fname, int bytes,
			 char *header);
#endif

static void poll1 (p_usb_dev_handle udev);
static void poll2 (p_usb_dev_handle udev);

static void reset_scanner (p_usb_dev_handle udev);


static void set_lightmap_white (unsigned short *map, int dpi, int color);

static void calc_lightmap (unsigned short *buff,
			   unsigned short *storage, int index, int dpi,
			   double gain, int offset);
static void select_pixels (unsigned short *map, int dpi, int start, int end);

static void record_mem (p_usb_dev_handle udev, unsigned char **dest,
			int bytes);
static void set_lamp_timer (p_usb_dev_handle udev, int timeout_in_mins);

static void set_gain_black (p_usb_dev_handle udev,
			    int r_gain, int g_gain, int b_gain,
			    int r_black, int g_black, int b_black);

static void idle_ab (p_usb_dev_handle udev);
static void write_regs (p_usb_dev_handle udev, int regs, unsigned char reg1,
			unsigned char val1,
			... /*unsigned char reg, unsigned char val, ... */ );
static int write_vctl (p_usb_dev_handle udev, int request, int value,
		       int index, unsigned char byte);
static int read_vctl (p_usb_dev_handle udev, int request, int value,
		      int index, unsigned char *byte);

static void download_lut8 (p_usb_dev_handle udev, int dpi, int incolor);

static void record_line (int reset,
			 p_usb_dev_handle udev,
			 unsigned char *storeline,
			 int dpi, int scanpix, int gray, int bpp16,
			 int *save_i,
			 unsigned char **save_scan_line,
			 unsigned char **save_dpi1200_remap,
			 unsigned char **save_color_remap);


static void prepare_params (SM3840_Params * params);

static void fix_endian_short (unsigned short *data, int count);

#define rd_timeout 10000
#define wr_timeout 10000
