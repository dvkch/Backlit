/*
 * kodakaio.c - SANE library for Kodak ESP Aio scanners.
 *
 * Copyright (C)   2011-2015 Paul Newall
 *
 * Based on the Magicolor sane backend: 
 * Based on the epson2 sane backend:
 * Based on Kazuhiro Sasayama previous
 * work on epson.[ch] file from the SANE package.
 * Please see those files for additional copyrights.
 * Author: Paul Newall
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.

 * Modified 30/12/14 to fix bug where network connection was broken after 30s of idle time.
 * The connection is now made in sane_start and ended in sane_cancel.
 * 01/01/13 Now with adf, the scan can be padded to make up the full page length, 
 * or the page can terminate at the end of the paper. This is a selectable option.
 * 25/11/12 Using avahi now for net autodiscovery. Use configure option --enable-avahi
 */

/* 
Packages to add to a clean ubuntu install
libavahi-common-dev
libusb-dev
libsnmp-dev

convenient lines to paste
export SANE_DEBUG_KODAKAIO=20

for ubuntu prior to 12.10
./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var --enable-avahi --disable-latex BACKENDS="kodakaio test"

for ubuntu 12.10
./configure --prefix=/usr --libdir=/usr/lib/i386-linux-gnu --sysconfdir=/etc --localstatedir=/var --enable-avahi --disable-latex BACKENDS="kodakaio test"

for ubuntu 14.10
./configure --prefix=/usr --libdir=/usr/lib/x86_64-linux-gnu --sysconfdir=/etc --localstatedir=/var --enable-avahi --disable-latex BACKENDS="kodakaio test"

If you want to use the test backend, for example with sane-troubleshoot, you should enable it in /etc/sane.d/dll.conf

*/

/* SANE-FLOW-DIAGRAM  Kodakaio commands in [] brackets
 
   - sane_init() : initialize backend, attach scanners(devicename,0)
   . - sane_get_devices() : query list of scanner-devices
   . - sane_open() : open a particular scanner-device and attach_scanner(devicename,&dev)
   . . - sane_set_io_mode : set blocking-mode
   . . - sane_get_select_fd : get scanner-fd
   . . - sane_get_option_descriptor() : get option informations
   . . - sane_control_option() : change option values
   . .
   . . - sane_start() : start image aquisition [V,L,F,S,C,D,O,Z] first time or after cancel. [(F),E,G] every time
   . .   - sane_get_parameters() : returns actual scan-parameters
   . .   - sane_read() : read image-data (from pipe)
   . . - sane_cancel() : cancel operation, kill reader_process [(F), U]
   
   . - sane_close() : close opened scanner-device, do_cancel, free buffer and handle
   - sane_exit() : terminate use of backend, free devicename and device-struture
*/
/* FUNCTION-TREE
	sane_init
	sane_open
		device_detect
			k_dev_init
			open_scanner
			close_scanner
		sane_get_devices
		init_options
		(open_scanner - moved to sane_start 27/12/14 )
	sane_control_option
		getvalue
		setvalue
			search_string_list
			change_source
				activateOption
				deactivateOption
	sane_start
		open_scanner
		k_init_parametersta
		k_lock_scanner
			k_hello
		k_set_scanning_parameters
		print_params
		k_start_scan
			cmd_start_scan
				print_status
				k_send
				kodakaio_txrxack
	sane_get_parameters
		print_params
	sane_read
		k_read
			cmd_read_data (reads one block)
				k_recv
			cmp_array
	sane_cancel
		cmd_cancel_scan
		close_scanner
	sane_close
		(close_scanner - moved to sane_cancel 27/12/14)
	sane_exit
		free_devices
	k_recv
		kodakaio_net_read
		dump_hex_buffer_dense
	k_send
		sanei_kodakaio_net_write_raw
		dump_hex_buffer_dense
	open_scanner
		sanei_kodakaio_net_open
	close_scanner
		k_scan_finish
			cmd_cancel_scan
		sanei_kodakaio_net_close or sanei_usb_close
	detect_usb
		kodakaio_getNumberOfUSBProductIds
	attach_one_config - (Passed to sanei_configure_attach)
		kodakaio_getNumberOfUSBProductIds
		kodak_network_discovery
			client_callback
			browse_callback
				resolve_callback
					ProcessAvahiDevice
		attach_one_net
	attach_one_net
		attach
			device_detect
	attach_one_usb - (passed to sanei_usb_find_devices)
		attach
			device_detect
	k_lock_scanner
		kodakaio_txrx
			k_send
			k_recv
	kodakaio_txrxack
		k_send
		k_recv
	cmd_set_color_curve
		kodakaio_expect_ack
			k_recv
	cmd_cancel_scan
		kodakaio_txrxack
	cmd_set_scanning_parameters
		kodakaio_txrxack
	device_detect
		k_dev_init
*/


#define KODAKAIO_VERSION	02
#define KODAKAIO_REVISION	7
#define KODAKAIO_BUILD		2

/* for usb (but also used for net though it's not required). */
#define MAX_BLOCK_SIZE		32768

#define SCANNER_READ_TIMEOUT	15
/* POLL_ITN_MS sets the individual poll timeout for network discovery */
#define POLL_ITN_MS 20


/* debugging levels:
In terminal use: export SANE_DEBUG_KODAKAIO=40 to set the level to 40 or whatever
level you want.
Then you can scan with scanimage or simple-scan from terminal and see debug info

use these defines to promote certain functions that you are interested in 
define low values to make detail of a section appear when DBG level is low
define a high value eg 99 to get normal behaviour. */
#define DBG_READ		99
#define DBG_AUTO		99  /* for autodiscovery */

/*
normal levels. This system is a plan rather than a reality
 *
 *     127	recv buffer
 *     125	send buffer
 *	35	fine-grained status and progress
 *	30	sane_read
 *	25	setvalue, getvalue, control_option
 *	20	low-level (I/O) functions
 *	15	mid-level  functions
 *	10	high-level  functions
 *	 7	open/close/attach
 *	 6	print_params
 *	 5	basic functions
 *	 3	status info and progress
 *	 2	sane api
 *	 1	errors & warnings
 */

#include "sane/config.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <math.h>
#include <poll.h>
#include <time.h>

#if WITH_AVAHI
/* used for auto detecting network printers  */
#include <assert.h>
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

static AvahiSimplePoll *simple_poll = NULL; /* global because called by several functions */

#endif

#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/sanei_tcp.h"
#include "../include/sane/sanei_udp.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_backend.h"

#include "kodakaio.h"

/* vendor and product ids that are allowed */
#define SANE_KODAKAIO_VENDOR_ID	(0x040a)

#define min(x,y) (((x)<(y))?(x):(y))

/* I think these timeouts (ms) are defaults, overridden by any timeouts in the kodakaio.conf file */
static int K_SNMP_Timeout = 3000; /* used for any auto detection method */
static int K_Scan_Data_Timeout = 10000;
static int K_Request_Timeout = 5000;

/* static int bitposn=0; was used to pack bits into bytes in lineart mode */

/* This file is used to store directly the raster returned by the scanner for debugging
If RawScanPath has no length it will not be created */
FILE *RawScan = NULL;
/* example: char RawScanPath[] = "TestRawScan.pgm"; */
char RawScanPath[] = ""; /* empty path means no raw scan file is made */

/*
 *   Devices supported by this backend 
*/

/* kodak command strings */
static unsigned char KodakEsp_V[]      = {0x1b,'S','V',0,0,0,0,0};  /* version?? */
static unsigned char KodakEsp_v[]      = {0x1b,'s','v',0,0,0,0,0};  /* reply to version?? */
static unsigned char KodakEsp_Lock[]   = {0x1b,'S','L',0,0,0,0,0}; /* Locks scanner */
static unsigned char KodakEsp_UnLock[] = {0x1b,'S','U',0,0,0,0,0}; /* Unlocks scanner */
static unsigned char KodakEsp_Ack[]    = {0x1b,'S','S',0,0,0,0,0}; /* Acknowledge for all commands */
/* the bytes after esc S S 0 may indicate status: S S 0 1 = docs in adf */
static unsigned char KodakEsp_F[]      = {0x1b,'S','F',0,0,0,0,0}; /* Purpose not known? colour balance?*/
static unsigned char KodakEsp_Comp[]   = {0x1b,'S','C',3,8,3,0,0}; /* 3,8,3,1,0 does compression. */
/* The compression method is unknown */
/* static unsigned char KodakEsp_E[]   = {0x1b,'S','E',1,0,0,0,0};  NET Purpose not known */
/* the extra 1 below could be despeckle option? maybe only for Hero 9.1 but no errors with ESP5250 */
static unsigned char KodakEsp_E[]      = {0x1b,'S','E',1,1,0,0,0};
static unsigned char KodakEsp_Go[]     = {0x1b,'S','G',0,0,0,0,0}; /* Starts the scan */
/* Other commands are: D (resolution), O (top left), Z (bottom right), R, G, B (curves) */

/* What is the relationship between these and the ranges in cap? */
static SANE_Int kodakaio_resolution_list[] = {75, 150, 300, 600, 1200};
static SANE_Int kodakaio_depth_list[] = {1,8}; /* The first value is the number of following entries */

/* strings to try and match the model ';' separator
static unsigned char SupportedMatchString[] = "KODAK ESP;KODAK HERO;KODAK OFFICE HERO;ADVENT WiFi AIO;"; */

static struct KodakaioCap kodakaio_cap[] = {
/* usbid,commandtype, modelname, USBoutEP, USBinEP,
	opticalres, {dpi range}, pointer to res list, res list size
	max depth, pointer to depth list,
	flatbed x range, flatbed y range,
	adf present, adf duplex,
	adf x range, adf y range (y range should be set a little shorter than the paper being scanned)

The following are not used but may be in future
commandtype, max depth, pointer to depth list
*/

/* list of cap data the first scanner is the default 
*/
  /* KODAK AIO DEFAULT,  */
  {
      0x9999, "esp", "KODAK AIO DEFAULT",
      -1, 0x82, /* USBoutEP, USBinEP (-1 means find one) */
      600, {75, 600, 0}, kodakaio_resolution_list, 4,  /* 600 dpi max, 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_FALSE, SANE_FALSE, /* ADF, duplex */ 
      {0, SANE_FIX(100), 0}, {0, SANE_FIX(100), 0} /* ADF x/y ranges (TODO!) */
  },
  /* KODAK ESP 5100, */
  {
      0x4025, "esp", "KODAK ESP 5100 AiO",
      -1, 0x82, /* USBoutEP, USBinEP (-1 means find one) */
      600, {75, 600, 0}, kodakaio_resolution_list, 4,  /* 600 dpi max, 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_FALSE, SANE_FALSE, /* ADF, duplex */ 
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
  /* KODAK ESP 5300, */
  {
      0x4026, "esp", "KODAK ESP 5300 AiO",
      -1, 0x82, /* USBoutEP, USBinEP (-1 means find one) */
      600, {75, 600, 0}, kodakaio_resolution_list, 4,  /* 600 dpi max, 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_FALSE, SANE_FALSE, /* ADF, duplex */ 
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
  /* KODAK ESP 5500, */
  {
      0x4027, "esp", "KODAK ESP 5500 AiO",
      -1, 0x82, /* USBoutEP, USBinEP (-1 means find one) */
      600, {75, 600, 0}, kodakaio_resolution_list, 4,  /* 600 dpi max, 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_FALSE, SANE_FALSE, /* ADF, duplex */ 
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
  /* KODAK ESP 5000, */
  {
      0x4028, "esp", "KODAK ESP 5000 Series AiO",
      -1, 0x82, /* USBoutEP, USBinEP (-1 means find one) */
      600, {75, 600, 0}, kodakaio_resolution_list, 4,  /* 600 dpi max, 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_FALSE, SANE_FALSE, /* ADF, duplex */ 
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
   /* KODAK ESP 3300, */
  {
      0x4031, "esp", "KODAK ESP 3300 Series AiO",
      -1, 0x82, /* USBoutEP, USBinEP (-1 means find one) */
      600, {75, 600, 0}, kodakaio_resolution_list, 4,  /* 600 dpi max, 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_FALSE, SANE_FALSE, /* ADF, duplex */ 
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
  /* KODAK ESP5, */
  {
      0x4032, "esp", "KODAK ESP 5 AiO",
      -1, 0x82, /* USBoutEP, USBinEP (-1 means find one) */
      600, {75, 600, 0}, kodakaio_resolution_list, 4,  /* 600 dpi max, 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_FALSE, SANE_FALSE, /* ADF, duplex */ 
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
  /* KODAK ESP7, */
  {
      0x403E, "esp", "KODAK ESP 7 AiO",
      -1, 0x82, /* USBoutEP, USBinEP (-1 means find one) */
      600, {75, 600, 0}, kodakaio_resolution_list, 4,  /* 600 dpi max, 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_FALSE, SANE_FALSE, /* ADF, duplex */ 
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
  /* KODAK ESP9, */
  {
      0x403F, "esp", "KODAK ESP 9 AiO",
      -1, 0x82, /* USBoutEP, USBinEP (-1 means find one) */
      600, {75, 600, 0}, kodakaio_resolution_list, 4,  /* 600 dpi max, 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_FALSE, SANE_FALSE, /* ADF, duplex */ 
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
  /* KODAK ESP5210 or 5250, */
  {
      0x4041, "esp", "KODAK ESP 5200 Series AiO",
      -1, 0x82, /* USBoutEP, USBinEP (-1 means find one) */
      600, {75, 600, 0}, kodakaio_resolution_list, 4,  /* 600 dpi max, 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_FALSE, SANE_FALSE, /* ADF, duplex */ 
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
  /* KODAK ESP3200 ,  */
  {
      0x4043, "esp", "KODAK ESP 3200 Series AiO",
      -1, 0x82,
      600, {75, 600, 0}, kodakaio_resolution_list, 4,  /* 600 dpi max, 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_FALSE, SANE_FALSE, /* ADF, duplex */ 
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
  /* KODAK ESP4100 ,  */
  {
      0x4053, "esp", "KODAK ESP Office 4100 Series AiO",
      -1, 0x82,
      600, {75, 600, 0}, kodakaio_resolution_list, 4,  /* 600 dpi max, 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_FALSE, SANE_FALSE, /* ADF, duplex */ 
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
  /* KODAK ESP6100 ,  */
  {
      0x4054, "esp", "KODAK ESP Office 6100 Series AiO",
      -1, 0x82,
      600, {75, 600, 0}, kodakaio_resolution_list, 4,  /* 600 dpi max, 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_TRUE, SANE_FALSE, /* ADF, duplex */ 
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
  /* KODAK ESP7200 ,  */
  {
      0x4056, "esp", "KODAK ESP 7200 Series AiO",
      -1, 0x82,
      600, {75, 600, 0}, kodakaio_resolution_list, 4,  /* 600 dpi max, 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_FALSE, SANE_FALSE, /* ADF, duplex */ 
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
  /* KODAK ESP C110 ,  */
  {
      0x4057, "esp", "KODAK ESP C110 AiO",
      -1, 0x82,
      600, {75, 600, 0}, kodakaio_resolution_list, 4,  /* 600 dpi max, 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_FALSE, SANE_FALSE, /* ADF, duplex */ 
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
  /* KODAK ESP C115 ,  */
  {
      0x4058, "esp", "KODAK ESP C115 AiO",
      -1, 0x82,
      600, {75, 600, 0}, kodakaio_resolution_list, 4,  /* 600 dpi max, 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_FALSE, SANE_FALSE, /* ADF, duplex */ 
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
  /* KODAK ESP 2150 ,  */
  {
      0x4059, "esp", "KODAK ESP Office 2150 Series",
      -1, 0x82,
      600, {75, 600, 0}, kodakaio_resolution_list, 4,  /* 600 dpi max, 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_TRUE, SANE_FALSE, /* ADF, duplex */ 
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
  /* KODAK ESP C310 ,  */
  {
      0x405D, "esp", "KODAK ESP C310 AiO",
      -1, 0x82,
      600, {75, 600, 0}, kodakaio_resolution_list, 4,  /* 600 dpi max, 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_FALSE, SANE_FALSE, /* ADF, duplex */ 
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
  /* KODAK ESP C315 ,  */
  {
      0x405E, "esp", "KODAK ESP C315 AiO",
      -1, 0x82,
      600, {75, 600, 0}, kodakaio_resolution_list, 4,  /* 600 dpi max, 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_FALSE, SANE_FALSE, /* ADF, duplex */ 
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
  /* ADVENT AW10,   */
  {
      0x4060, "esp", "ADVENT WiFi AIO AW10",
      -1, 0x82,
      600, {75, 600, 0}, kodakaio_resolution_list, 4, /* 600 dpi max, {from, to, 0} 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_TRUE, SANE_FALSE, /* ADF, duplex. */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
  /* KODAK HERO 6.1,   */
  {
      0x4062, "esp", "KODAK OFFICE HERO 6.1 AiO",
      -1, 0x82,
      600, {75, 600, 0}, kodakaio_resolution_list, 4, /* 600 dpi max, {from, to, 0} 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_TRUE, SANE_FALSE, /* ADF, duplex. */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
  /* KODAK HERO 7.1,   */
  {
      0x4063, "esp", "KODAK HERO 7.1 AiO",
      -1, 0x82,
      600, {75, 600, 0}, kodakaio_resolution_list, 4, /* 600 dpi max, {from, to, 0} 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_FALSE, SANE_TRUE, /* ADF, duplex. */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
  /* KODAK HERO 5.1,   */
  {
      0x4064, "esp", "KODAK HERO 5.1 AiO",
      -1, 0x82,
      600, {75, 600, 0}, kodakaio_resolution_list, 4, /* 600 dpi max, {from, to, 0} 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_FALSE, SANE_TRUE, /* ADF, duplex.*/
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
  /* KODAK ESP9200 ,  */
  {
      0x4065, "esp", "KODAK ESP 9200 Series AiO",
      -1, 0x82,
      600, {75, 600, 0}, kodakaio_resolution_list, 4,  /* 600 dpi max, 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_TRUE, SANE_FALSE, /* ADF, duplex */ 
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
  /* KODAK ESP2170 ,  */
  {
      0x4066, "esp", "KODAK ESP Office 2170 Series",
      -1, 0x82,
      1200, {75, 1200, 0}, kodakaio_resolution_list, 5, /* 1200 dpi optical, {from, to, 0} 5 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_TRUE, SANE_FALSE, /* ADF, duplex */ 
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
  /* KODAK HERO 9.1,   */
  {
      0x4067, "esp", "KODAK HERO 9.1 AiO",
      -1, 0x82,
      1200, {75, 1200, 0}, kodakaio_resolution_list, 5, /* 1200 dpi optical, {from, to, 0} 5 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_TRUE, SANE_FALSE, /* ADF, duplex. */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
  /* KODAK HERO 4.1,   */
  {
      0x4069, "esp", "KODAK HERO 4.1 AiO",
      -1, 0x82,
      600, {75, 600, 0}, kodakaio_resolution_list, 4, /* 600 dpi max, {from, to, 0} 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_TRUE, SANE_FALSE, /* ADF, duplex.*/

      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },

  /* KODAK HERO 3.1,   */
  {
      0x406D, "esp", "KODAK HERO 3.1 AiO",
      -1, 0x82,
      600, {75, 600, 0}, kodakaio_resolution_list, 4, /* 600 dpi max, {from, to, 0} 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_FALSE, SANE_TRUE, /* ADF, duplex. */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  },
  /* spare use for specified usbid */
  {
      0, "esp", "specified",
      -1, 0x82,
      600, {75, 600, 0}, kodakaio_resolution_list, 4, /* 600 dpi max, {from, to, 0} 4 resolutions */
      8, kodakaio_depth_list,                          /* color depth max 8, list above */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(11.7 * MM_PER_INCH), 0}, /* FBF x/y ranges */
      SANE_FALSE, SANE_TRUE, /* ADF, duplex. */
      {0, SANE_FIX(8.5 * MM_PER_INCH), 0}, {0, SANE_FIX(14 * MM_PER_INCH), 0} /* 14 ADF x/y ranges */
  }
};

/****************************************************************************
 *   General configuration parameter definitions ****************************************************************************/


/*
 * Definition of the mode_param struct, that is used to
 * specify the valid parameters for the different scan modes.
 *
 * The depth variable gets updated when the bit depth is modified.
 */

/* could be affecting what data sane delivers */
static struct mode_param mode_params[] = {
	{0x03, 3, 24},  /* Color, 3 colors, 24 bit */
	{0x02, 1, 8}, /* Grayscale, 1 color, 8 bit */
	{0x00, 1, 1}  /* Lineart, 1 color, 8 bit (was 8 bit)   */
};

static SANE_String_Const mode_list[] = {
	SANE_VALUE_SCAN_MODE_COLOR,
	SANE_VALUE_SCAN_MODE_GRAY,
	SANE_VALUE_SCAN_MODE_LINEART,
	NULL
};

static const SANE_String_Const adf_mode_list[] = {
	SANE_I18N("Simplex"),
	SANE_I18N("Duplex"),
	NULL
};

/* Define the different scan sources */
#define FBF_STR	SANE_I18N("Flatbed")
#define ADF_STR	SANE_I18N("Automatic Document Feeder")

/*
 * source list need one dummy entry (save device settings is crashing).
 * NOTE: no const - this list gets created while exploring the capabilities
 * of the scanner. Here space is reserved for 3 entries + NULL ?
 */

static SANE_String_Const source_list[] = {
	FBF_STR,
	NULL,
	NULL,
	NULL
};

static const SANE_Range percent_range_fixed = {SANE_FIX(0.0), SANE_FIX(100.0), SANE_FIX(1.0)};
/*static const SANE_Range percent_range_int = {0, 100, 1};*/

/* prototypes */
static SANE_Status attach_one_usb(SANE_String_Const devname);
static SANE_Status attach_one_net(SANE_String_Const devname, unsigned int device);
void kodakaio_com_str(unsigned char *buf, char *fmt_buf);
int cmparray (unsigned char *array1, unsigned char *array2, size_t len);
static struct KodakaioCap *get_device_from_identification (const char *ident, const char *vid, const char *pid);
void ProcessAvahiDevice(const char *device_id, const char *vid, const char *pid, const char *ip_addr);


/* Some utility functions */

static size_t
max_string_size(const SANE_String_Const strings[])
{
/* returns the length of the longest string in an array of strings */
	size_t size, max_size = 0;
	int i;

	for (i = 0; strings[i]; i++) {
		size = strlen(strings[i]) + 1;
		if (size > max_size)
			max_size = size;
	}
	return max_size;
}



static void
print_params(const SANE_Parameters params, int level)
{
	DBG(level, "formats: binary=?, grey=%d, colour=%d\n",SANE_FRAME_GRAY, SANE_FRAME_RGB );
	DBG(level, "params.format          = %d\n", params.format);
	DBG(level, "params.last_frame      = %d\n", params.last_frame);
	DBG(level, "params.bytes_per_line  = %d\n", params.bytes_per_line);
	DBG(level, "params.pixels_per_line = %d\n", params.pixels_per_line);
	DBG(level, "params.lines           = %d\n", params.lines);
	DBG(level, "params.depth           = %d\n", params.depth);
}

static void
print_status(KodakAio_Scanner *s,int level)
{
	DBG(level, "print_status with level %d\n", level);
	DBG(level, "s->bytes_unread          = %d\n", s->bytes_unread);
/*
	DBG(level, "params.last_frame      = %d\n", params.last_frame);
	DBG(level, "params.bytes_per_line  = %d\n", params.bytes_per_line);
	DBG(level, "params.pixels_per_line = %d\n", params.pixels_per_line);
	DBG(level, "params.lines           = %d\n", params.lines);
	DBG(level, "params.depth           = %d\n", params.depth);
*/
}

/****************************************************************************
 *   Low-level Network communication functions ****************************************************************************/

static int
kodakaio_net_read(struct KodakAio_Scanner *s, unsigned char *buf, size_t wanted,
		       SANE_Status * status)
/* there seems to be a condition where this returns no error and no data without detecting a timeout
That is probably if the scanner disconnected the network connection
*/
{
	size_t size, read = 0;
	struct pollfd fds[1];
	int pollreply;

	*status = SANE_STATUS_GOOD;

	/* poll for data-to-be-read (using K_Request_Timeout) */
	fds[0].fd = s->fd;
	fds[0].events = POLLIN;
	fds[0].revents = 0;
	if ((pollreply = poll (fds, 1, K_Request_Timeout)) <= 0) {
		if (pollreply == 0)
			DBG(1, "net poll timeout\n");
		else
			/* pollreply is -ve */
			DBG(1, "net poll error\n");
		*status = SANE_STATUS_IO_ERROR;
		return read;
	}
	else if((fds[0].revents & POLLIN) && !(fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))) {
		while (read < wanted) {
			DBG(50, "reading: read %lu, wanted %lu\n",read, wanted);
			size = sanei_tcp_read(s->fd, buf + read, wanted - read);
			if (size == 0) {
				DBG(1, "No data read. Scanner may have disconnected\n");
				break;
			}
			read += size;
		}

		if (read == 0)
			*status = SANE_STATUS_IO_ERROR;

		DBG(32, "net read %d bytes:%x,%x,%x,%x,%x,%x,%x,%x\n",read,buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7]);

		return read;
	}
	else
		DBG(1, "Unknown problem with poll\n");
		return read;
}


static int
sanei_kodakaio_net_write_raw(struct KodakAio_Scanner *s,
			      const unsigned char *buf, size_t buf_size,
			      SANE_Status *status)
{
	DBG(32, "net write:%x,%x,%x,%x,%x,%x,%x,%x\n",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7]);
	
	sanei_tcp_write(s->fd, buf, buf_size);
	/* TODO: Check whether sending failed... */

	*status = SANE_STATUS_GOOD;
	return buf_size;
}

static SANE_Status
sanei_kodakaio_net_open(struct KodakAio_Scanner *s)
{
	struct timeval tv;

	tv.tv_sec = 5;
	tv.tv_usec = 0;

	DBG(5, "%s\n", __func__);

	setsockopt(s->fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,  sizeof(tv));
	return SANE_STATUS_GOOD;
}

static SANE_Status
sanei_kodakaio_net_close(struct KodakAio_Scanner *s)
{
	NOT_USED(s);
	/* Does nothing - maybe should close the socket ? */
	return SANE_STATUS_GOOD;
}


/****************************************************************************
 *   Low-level USB communication functions ****************************************************************************/

static int
kodakaio_getNumberOfUSBProductIds (void)
{
  return sizeof (kodakaio_cap) / sizeof (struct KodakaioCap);
}

/****************************************************************************
 *   low-level communication commands ****************************************************************************/

static void dump_hex_buffer_dense (int level, const unsigned char *buf, size_t buf_size)
{
	size_t k;
	char msg[1024], fmt_buf[1024];
	memset (&msg[0], 0x00, 1024);
	memset (&fmt_buf[0], 0x00, 1024);
	for (k = 0; k < min(buf_size, 80); k++) {
		if (k % 16 == 0) {
			if (k>0) {
				DBG (level, "%s\n", msg);
				memset (&msg[0], 0x00, 1024);
			}
			sprintf (fmt_buf, "     0x%04lx  ", (unsigned long)k);
			strcat (msg, fmt_buf);
		}
		if (k % 8 == 0) {
			strcat (msg, " ");
		}
		sprintf (fmt_buf, " %02x" , buf[k]);
		strcat (msg, fmt_buf);
	}
	if (msg[0] != 0 ) {
		DBG (level, "%s\n", msg);
	}
}

/* changing params to char seems to cause a stack problem */
void kodakaio_com_str(unsigned char *buf, char *fmt_buf)
{
/* returns a printable string version of the first 8 bytes assuming they are a kodakaio command*/
	if(buf[0] == 0x1b) {
		sprintf (fmt_buf, "esc %c %c %02x %02x %02x %02x %02x",
			 buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
	}
	else {
		sprintf (fmt_buf, "%02x %02x %02x %02x %02x %02x %02x %02x",
			buf[0],buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
	}
}

static int
k_send(KodakAio_Scanner * s, void *buf, size_t buf_size, SANE_Status * status)
{
	char fmt_buf[25];

	kodakaio_com_str(buf, fmt_buf);
	DBG(15, "%s: size = %lu :%s\n", __func__, (u_long) buf_size, fmt_buf);

	if (DBG_LEVEL >= 125) {
		const unsigned char *s = buf;
		DBG(125, "complete buffer:\n");
		dump_hex_buffer_dense (125, s, buf_size);
	}

	if (s->hw->connection == SANE_KODAKAIO_NET) {
		return sanei_kodakaio_net_write_raw(s, buf, buf_size, status);
	} else if (s->hw->connection == SANE_KODAKAIO_USB) {
		size_t n;
		n = buf_size;
		*status = sanei_usb_write_bulk(s->fd, buf, &n);
		DBG(50, "USB: wrote %lu bytes, status: %s\n", (unsigned long)n, sane_strstatus(*status));
		return n;
	}

	*status = SANE_STATUS_INVAL;
	return 0;
}

static ssize_t
k_recv(KodakAio_Scanner * s, void *buf, ssize_t buf_size,
	    SANE_Status * status)
{
/* requests and receives data this function makes the split between USB and NET 
this function called by a number of others 

In USB mode, this function will wait until data is available for a maximum of SCANNER_READ_TIMEOUT seconds.
In NET mode the timeout is in kodakaio_net_read
*/
	ssize_t n = 0;
	char fmt_buf[25];
	time_t time_start;
	time_t time_now;
	struct timespec usb_delay, usb_rem;
	usb_delay.tv_sec = 0;
	usb_delay.tv_nsec = 300000000; /* 0.3 sec */

	if (s->hw->connection == SANE_KODAKAIO_NET) {
		
		time(&time_start);
		DBG(min(16,DBG_READ), "[%ld]  %s: net req size = %ld  ", (long) time_start, __func__, (long) buf_size);
 		n = kodakaio_net_read(s, buf, buf_size, status);
		DBG(min(16,DBG_READ), "returned %d\n", n);
		if (*status != SANE_STATUS_GOOD) {
			DBG(1, "%s: err returned from kodakaio_net_read, %s\n", __func__, sane_strstatus(*status));
		}


	} else if (s->hw->connection == SANE_KODAKAIO_USB) {
		/* Start the clock for USB timeout */
		time(&time_start);

		/* Loop until we have data */
		while (n == 0) {
			n = buf_size;
/* but what if the data is an exact number of blocks? */
			DBG(min(16,DBG_READ), "[%ld]  %s: usb req size = %ld  ", (long) time_start,  __func__, (long) n);
			*status = sanei_usb_read_bulk(s->fd, (SANE_Byte *) buf, (size_t *) & n);
			DBG(min(16,DBG_READ), "returned %ld\n", (long) n);

			if(*status != SANE_STATUS_GOOD) {

				DBG(min(16,DBG_READ), "sanei_usb_read_bulk gave %s\n", sane_strstatus(*status));

				if (*status == SANE_STATUS_EOF) {
					/* If we have EOF status, wait for more data */
					time(&time_now);
					if (difftime(time_now, time_start) < SCANNER_READ_TIMEOUT) {
						nanosleep(&usb_delay, &usb_rem);
					}
					else {
						/* Timeout */
						return n;
					}
				}
				else {
					/*  If we've encountered another type of error, return */
					return n;
				}
			}
		}
	}

	if (n == 8) {
		kodakaio_com_str(buf, fmt_buf);
		DBG(min(14,DBG_READ), "%s: size = %ld, got %s\n", __func__, (long int)n, fmt_buf);
	}
	/* dump buffer if appropriate */
	if (DBG_LEVEL >= 127 && n > 0) {
		const unsigned char* b=buf;
		dump_hex_buffer_dense (125, b, buf_size);
	}
	return n;
}


static SANE_Status
kodakaio_expect_ack(KodakAio_Scanner *s, unsigned char *rxbuf)
/* gets 8 byte reply, checks reply is an Ack and returns appropriate status */
{
	SANE_Status status;

	k_recv(s, rxbuf, 8, &status);
	if (status != SANE_STATUS_GOOD) {
		DBG(1, "%s: rx err, %s\n", __func__, sane_strstatus(status));
		return status;
	}
	/* strncmp ignores diffent possible responses like escSS00000 and escSS02000 */
	if (strncmp((char *)KodakEsp_Ack,(char *)rxbuf,4)!=0) {
		DBG (1, "No Ack received, Expected 0x%2x %2x %2x %2x... got 0x%2x %2x %2x %2x...\n",
		KodakEsp_Ack[0], KodakEsp_Ack[1], KodakEsp_Ack[2], KodakEsp_Ack[3],rxbuf[0], rxbuf[1], rxbuf[2], rxbuf[3]);
		return SANE_STATUS_IO_ERROR;
	}

	return status;
}

static SANE_Status
kodakaio_txrx(KodakAio_Scanner *s, unsigned char *txbuf, unsigned char *rxbuf)
/* Sends 8 byte data to scanner and returns reply and appropriate status. */
{
	SANE_Status status;
	ssize_t n = 0;

	k_send(s, txbuf, 8, &status);
	if (status != SANE_STATUS_GOOD) {
		DBG(1, "%s: tx err, %s\n", __func__, sane_strstatus(status));
		return status;
	}
	n = k_recv(s, rxbuf, 8, &status);
	if (status != SANE_STATUS_GOOD) {
		DBG(1, "%s: %s gave rx err, %s\n", __func__, "txvalue", sane_strstatus(status));
		return status;
	}
	if (n == 0) {
		DBG(1, "%s: try 1 k_recv returned 0 bytes with status %s\n", __func__, sane_strstatus(status));
		n = k_recv(s, rxbuf, 8, &status);
		if (status != SANE_STATUS_GOOD) {
			DBG(1, "%s: %s gave rx err, %s\n", __func__, "txvalue", sane_strstatus(status));
			return status;
		}
		if (n == 0) {
			DBG(1, "%s: try 2 k_recv returned 0 bytes with status %s\n", __func__, sane_strstatus(status));
			return status;
		}
	}
	return status;
}

static SANE_Status
kodakaio_txrxack(KodakAio_Scanner *s, unsigned char *txbuf, unsigned char *rxbuf)
/*
Sends 8 byte data to scanner, gets 8 byte reply, checks reply is an Ack
and returns appropriate status
*/
{
	SANE_Status status;

	k_send(s, txbuf, 8, &status);
	if (status != SANE_STATUS_GOOD) {
		DBG(1, "%s: tx err, %s\n", __func__, sane_strstatus(status));
		return status;
	}

	k_recv(s, rxbuf, 8, &status);
	if (status != SANE_STATUS_GOOD) {
		DBG(1, "%s: %s gave rx err, %s\n", __func__, "txvalue", sane_strstatus(status));
		return status;
	}
	/* strncmp ignores different possible responses like escSS00000 and escSS02000 */
	if (strncmp((char *)KodakEsp_Ack,(char *)rxbuf,3) == 0) { /* was 4 byte comp */
		if (rxbuf[4] == 0x01 && s->adf_loaded == SANE_FALSE) {
			s->adf_loaded = SANE_TRUE;
			DBG(5, "%s: News - docs in ADF\n", __func__);
		}
		else if (rxbuf[4] != 
0x01 && s->adf_loaded == SANE_TRUE) {
			s->adf_loaded = SANE_FALSE;
			DBG(5, "%s: News - ADF is empty\n", __func__);
		}
	}
	else {
		DBG (1, "No Ack received, Sent 0x%2x %2x %2x %2x... got 0x%2x %2x %2x %2x...\n",
		txbuf[0], txbuf[1], txbuf[2], txbuf[3],rxbuf[0], rxbuf[1], rxbuf[2], rxbuf[3]);
		return SANE_STATUS_IO_ERROR;
	}

	return status;
}

/* unused function
static ssize_t 
kodakaio_rxflush(KodakAio_Scanner *s)

Tries to get 64 byte reply
and returns number of bytes read

{
	SANE_Status status;
	unsigned char rxbuf[64];
	ssize_t n = 0;

	n = k_recv(s, rxbuf, 64, &status);
	if (status != SANE_STATUS_GOOD) {
		DBG(1, "%s: %s gave rx err, %s\n", __func__, "status", sane_strstatus(status));
	}
	DBG(5, "%s: flushed, %d bytes\n", __func__,  (int)n);
	return n;
}
*/

/*
 *   high-level communication commands 
*/

static SANE_Status
k_hello (KodakAio_Scanner * s)
{
	SANE_Status status;
	unsigned char reply[8];
	char fmt_buf[25];

	DBG(5, "%s\n", __func__);

/* check that there is nothing already in the input buffer before starting 
kodakaio_rxflush(s);
*/
/* preset the reply, so I can see if it gets changed */
reply[0] = 0; reply[1] = 1; reply[2] = 2; reply[3] = 3; reply[4] = 4; reply[5] = 5; reply[6] = 6; reply[7] = 7;

	if((status = kodakaio_txrx(s, KodakEsp_V, reply))!= SANE_STATUS_GOOD) {
		DBG(1, "%s: KodakEsp_V failure, %s\n", __func__, sane_strstatus(status));
		return SANE_STATUS_IO_ERROR;
	}

	if (strncmp((char *) reply, (char *) KodakEsp_v, 3)!=0) {
		kodakaio_com_str(reply, fmt_buf);
			DBG(1, "%s: KodakEsp_v err, got %s\n", __func__, fmt_buf);
			return SANE_STATUS_IO_ERROR;
	}


	DBG(5, "%s: OK %s\n", __func__, sane_strstatus(status));
	return status;
}

/* Start scan command */
static SANE_Status
cmd_start_scan (SANE_Handle handle, size_t expect_total)
/* expect_total is the expected total no of bytes just for info -ve value not a problem? or is it for size_t? */
{
	KodakAio_Scanner *s = (KodakAio_Scanner *) handle;
	SANE_Status status = SANE_STATUS_GOOD;
	unsigned char reply[8];
/*send the start command here */
	print_status(s, 5);
/*adf added 20/2/12, apparently an extra KodakEsp_F is sent when the adf is used */
	if (strcmp(source_list[s->val[OPT_SOURCE].w], ADF_STR) == 0) { /* adf is in use */
		if (! s->adf_loaded) return SANE_STATUS_CANCELLED; /* was SANE_STATUS_NO_DOCS; */
		if (kodakaio_txrxack(s, KodakEsp_F, reply)!= SANE_STATUS_GOOD) {
			DBG(1, "%s: Did not get a good reply to KodakEsp_F\n", __func__);
			return SANE_STATUS_IO_ERROR;
		}
	}

	if (kodakaio_txrxack(s, KodakEsp_E, reply)!= SANE_STATUS_GOOD) {
		DBG(1, "%s: Did not get a good reply to KodakEsp_E\n", __func__);
		return SANE_STATUS_IO_ERROR;
	}

	DBG(20, "starting the scan, expected total bytes %d\n",expect_total);
	k_send(s, KodakEsp_Go, 8, &status);

	if (status != SANE_STATUS_GOOD)
		DBG(1, "%s: KodakEsp_Go command NOT successfully sent\n", __func__);
	else {
		DBG(30, "%s: KodakEsp_Go command successfully sent\n", __func__);
		s->scanning = SANE_TRUE;
	}
	return status;
}

static SANE_Status
cmd_cancel_scan (SANE_Handle handle)
{
	KodakAio_Scanner *s = (KodakAio_Scanner *) handle;
	unsigned char reply[8];
/* adf added 20/2/12 should it be adf? or adf with paper in? */
	if (strcmp(source_list[s->val[OPT_SOURCE].w], ADF_STR) == 0) { /* adf */
		if (kodakaio_txrxack(s, KodakEsp_F, reply)!= SANE_STATUS_GOOD) 
		{
			DBG(1, "%s: KodakEsp_F command failed\n", __func__);
			return SANE_STATUS_IO_ERROR;
		}
		if (kodakaio_txrxack(s, KodakEsp_UnLock, reply)!= SANE_STATUS_GOOD) 
		{
			DBG(1, "%s: KodakEsp_UnLock command failed\n", __func__);
			return SANE_STATUS_IO_ERROR;
		}
		DBG(5, "%s unlocked the scanner with adf F U\n", __func__);
	}
	else { /* no adf */
		if (kodakaio_txrxack(s, KodakEsp_UnLock, reply)!= SANE_STATUS_GOOD)
		{
			DBG(1, "%s: KodakEsp_UnLock command failed\n", __func__);
			return SANE_STATUS_IO_ERROR;
		}
		DBG(5, "%s unlocked the scanner U\n", __func__);
	}
	s->scanning = SANE_FALSE;
	return SANE_STATUS_GOOD;
}

static SANE_Status
cmd_get_scanning_parameters(SANE_Handle handle,
			    SANE_Frame *format, SANE_Int *depth,
			    SANE_Int *data_pixels, SANE_Int *pixels_per_line,
			    SANE_Int *lines)
{
/* data_pixels is per line.
Old mc cmd read this stuff from the scanner. I don't think kodak can do that easily */

	KodakAio_Scanner *s = (KodakAio_Scanner *) handle;
	SANE_Status status = SANE_STATUS_GOOD;
	NOT_USED (format);
	NOT_USED (depth);

	DBG(10, "%s\n", __func__);

	/* Calculate returned values */
	*lines = s->params.lines;
	*pixels_per_line = s->params.pixels_per_line;
	*data_pixels = s->params.pixels_per_line;

	DBG (20, "%s: data_pixels = %u, lines = %u, "
		"pixels_per_line = %u)\n", __func__,
		*data_pixels, *lines, *pixels_per_line);
	return status;
}

/* Set color curve command, low level, sends commands to the scanner*/
static SANE_Status
cmd_set_color_curve(SANE_Handle handle, unsigned char col)
{
/* sends the color curve data for one color*/
	KodakAio_Scanner *s = (KodakAio_Scanner *) handle;
	SANE_Status status = SANE_STATUS_GOOD;
	unsigned char tx_col[8];
	unsigned char rx[8];
	unsigned char tx_curve[256];
	int i; /* 7/9/14 was unsigned char and that stopped the loop that made the linear curve from going to 255 */
	DBG(32, "%s: start\n", __func__);
	tx_col[0]=0x1b; tx_col[1]='S'; tx_col[2]='K'; tx_col[3]=col; tx_col[4]=0; tx_col[5]=0; tx_col[6]=0; tx_col[7]=0;
/* linear curve now but could send tailor made curves in future */	
	for(i=0;i<=255;++i) tx_curve[i]=i; /* 7/9/14 was i<255 the missing elements caused speckles */

	k_send(s, tx_col, 8, &status);
	if (status != SANE_STATUS_GOOD) {
		DBG(1, "%s: tx err, %s\n", __func__, "curve command");
		return status;
	}
	k_send(s, tx_curve, 256, &status);
	if (status != SANE_STATUS_GOOD) {
		DBG(1, "%s: tx err, %s\n", __func__, "curve data");
		return status;
	}
	if (kodakaio_expect_ack(s, rx) != SANE_STATUS_GOOD) return SANE_STATUS_IO_ERROR;
		DBG(10, "%s: sent curve OK, \n", __func__);
		return status;
}

/* Set scanning parameters command, low level, sends commands to the scanner*/
static SANE_Status
cmd_set_scanning_parameters(SANE_Handle handle,
	int resolution,
	int tl_x, int tl_y, int width, int height, unsigned char source)

/* NB. here int tl_x, int tl_y, int width, int height are in DPI units, not optres units! */
/* sends params to scanner, but should  we store them too? */
{
	KodakAio_Scanner *s = (KodakAio_Scanner *) handle;
	SANE_Status status = SANE_STATUS_GOOD;
	unsigned char tx_S[8];
	unsigned char tx_dpi[8];
	unsigned char tx_topleft[8];
	unsigned char tx_widthheight[8];
	unsigned char bufread[8];
	int i;

/*don't know the purpose of F yet. windows USB repeated 4 x why? does it affect the colour balance?*/
	DBG(8, "%s\n", __func__);
	for(i=0;i<4;++i) {
		kodakaio_txrxack(s, KodakEsp_F, bufread);
		sleep(1);
	}
/*	kodakaio_txrxack(s, KodakEsp_F, bufread);  for net, just once */

/* Source? bed /ADF */
	tx_S[0]=0x1b; tx_S[1]='S'; tx_S[2]='S'; tx_S[3]=source; tx_S[4]=0; tx_S[5]=0; tx_S[6]=0; tx_S[7]=0;
	kodakaio_txrxack(s, tx_S, bufread);

/* Compression */
	kodakaio_txrxack(s, KodakEsp_Comp, bufread);

/* DPI resolution */
	tx_dpi[0]=0x1b;
	tx_dpi[1]='S';
	tx_dpi[2]='D';
	tx_dpi[3]=resolution & 0xff;
	tx_dpi[4]=(resolution >> 8) & 0xff;
	tx_dpi[5]=resolution & 0xff;
	tx_dpi[6]=(resolution >> 8) & 0xff;
	tx_dpi[7]=0;
	kodakaio_txrxack(s, tx_dpi, bufread);

/* colour curves don't seem to be sent for usb preview 
but it seems to do no harm to send them */
	cmd_set_color_curve(s, 'R');
	cmd_set_color_curve(s, 'G');
	cmd_set_color_curve(s, 'B');


/* Origin top left s->tl_x and s->tl_y are in optres units
this command needs actual DPI units*/
	DBG(20, "%s: left (DPI)=%d, top (DPI)=%d\n", __func__, tl_x , tl_y);

	tx_topleft[0]=0x1b;
	tx_topleft[1]='S';
	tx_topleft[2]='O';
	tx_topleft[3]=(tl_x) & 0xff;
	tx_topleft[4]=((tl_x) >> 8) & 0xff;
	tx_topleft[5]=(tl_y) & 0xff;
	tx_topleft[6]=((tl_y) >> 8) & 0xff;
	tx_topleft[7]=0;
	kodakaio_txrxack(s, tx_topleft, bufread);

/* Z width height note the s->width and s->height are in optres units
this command needs actual DPI units*/
	tx_widthheight[0]=0x1b;
	tx_widthheight[1]='S';
	tx_widthheight[2]='Z';
	tx_widthheight[3]=(width) & 0xff;
	tx_widthheight[4]=((width) >> 8) & 0xff;
	tx_widthheight[5]=(height) & 0xff;
	tx_widthheight[6]=((height) >> 8) & 0xff;
	tx_widthheight[7]=0;
	kodakaio_txrxack(s, tx_widthheight, bufread);

	if (status != SANE_STATUS_GOOD)
		DBG(1, "%s: Data NOT successfully sent\n", __func__);
	else
		DBG(20, "%s: Data successfully sent\n", __func__);
	return status;
}

int
cmparray (unsigned char *array1, unsigned char *array2, size_t len)
{
/* compares len bytes of the arrays returns 0 if they match 
returns the first missmatch position if they don't match */
unsigned int i;
	for(i=0; i<len; ++i)
	{
		if(array1[i] != array2[i]) return -1;
	}
	return 0;
}

static SANE_Status
cmd_read_data (SANE_Handle handle, unsigned char *buf, size_t *len)
{
/* 
cmd_read_data is only used in k_read. It reads one block of data
read data using k_recv until you get the ackstring
when you get the ackstring return s->ack and do padding if the padding option is selected
if no padding option return EOF
*/
	KodakAio_Scanner *s = (KodakAio_Scanner *) handle;
	SANE_Status status;
	int oldtimeout = K_Request_Timeout;
	size_t bytecount;
	unsigned char *Last8; /* will point to the last 8 chars in buf */
	int i, line, lines;

	if (s->ack && s->val[OPT_PADDING].w) {
		/* do padding of whole block*/
		/* memset(buf, 0x80, *len);  need to work out the background colour for this */
		lines = *len / s->params.bytes_per_line;
		for (line=0; line < lines; ++line) {
			for (i=0; i< s->params.pixels_per_line; ++i) {
				buf[line * s->params.bytes_per_line + i] = s->background[0]; /*red */
				buf[line * s->params.bytes_per_line + s->params.pixels_per_line + i] = s->background[1];  /*green */
				buf[line * s->params.bytes_per_line + 2 * s->params.pixels_per_line + i] = s->background[2];  /*blue */
			}
		}
		s->bytes_unread -= *len;
		if (s->bytes_unread < 0)
			s->bytes_unread = 0;
		return SANE_STATUS_GOOD;
	}

	if (s->ack && !s->val[OPT_PADDING].w) {
		s->bytes_unread = 0;
		s->eof = SANE_TRUE;
		return SANE_STATUS_EOF;
	}

	/* Temporarily set the poll timeout long instead of short,
	 * because a color scan needs >5 seconds to initialize. Is this needed for kodak? maybe */
	K_Request_Timeout = K_Scan_Data_Timeout;
	sanei_usb_set_timeout (K_Scan_Data_Timeout);
	bytecount = k_recv(s, buf, *len, &status);
	K_Request_Timeout = oldtimeout;
	sanei_usb_set_timeout (oldtimeout);

/* We may need to do some special thing with the block request size to cope with usb blocks of any length
in order to keep the ack bytes, when using adf, at the end of one block ie not split between blocks.
But it seems that the scanner takes care of that, and gives you the ack as a separate 8 byte block */

	if (bytecount >= 8) {
		/* it may be the last block from the scanner so look for Ack response in last 8 bytes */
		Last8 = buf + bytecount - 8;

		/* only compare 4 bytes because we sometimes get escSS02.. or escSS00.. 
		is 4 the right number ? */
		if (cmparray(Last8,KodakEsp_Ack,4) == 0) {
			DBG(min(10,DBG_READ), "%s: found KodakEsp_Ack at %d bytes of %d\n", __func__, bytecount, *len);
			s->ack = SANE_TRUE;
			*len = bytecount - 8; /* discard the Ack response */
			s->bytes_unread -= *len; /* return a short block */
		}
		else {
		/* a not full buffer is returned usb does this */
		DBG(min(10,DBG_READ), "%s: buffer not full, got %d bytes of %d\n", __func__, bytecount, *len);
		*len = bytecount;
		s->bytes_unread -= bytecount;
		}
	}
	else {
		DBG(min(1,DBG_READ), "%s: tiny read, got %d bytes of %d\n", __func__, (int) bytecount, *len);
		return SANE_STATUS_IO_ERROR;
	}
	if (*len > s->params.bytes_per_line) {
		/* store average colour as background. That's not the ideal method but it's easy to implement. What's it used for? */
		lines = *len / s->params.bytes_per_line;
		s->background[0] = 0;
		s->background[1] = 0;
		s->background[2] = 0;

		for (line=0; line < lines; ++line) {
			for (i=0; i< s->params.pixels_per_line; ++i) {
				s->background[0] += buf[line * s->params.bytes_per_line + i]; /*red */
				s->background[1] += buf[line * s->params.bytes_per_line + s->params.pixels_per_line + i];  /*green */
				s->background[2] += buf[line * s->params.bytes_per_line + 2 * s->params.pixels_per_line + i];  /*blue */
			}
		}
		s->background[0] = s->background[0] / (lines * s->params.pixels_per_line);
		s->background[1] = s->background[1] / (lines * s->params.pixels_per_line);
		s->background[2] = s->background[2] / (lines * s->params.pixels_per_line);

	}

	if (status == SANE_STATUS_GOOD)
		if (s->bytes_unread <= 0)
			DBG(min(2,DBG_READ), "%s: Page fully read %d blocks, %ld bytes unread\n", __func__, s->counter, (long) s->bytes_unread);		
		else
			DBG(min(20,DBG_READ), "%s: Image data successfully read %ld bytes, %ld bytes unread\n", __func__, (long) bytecount, (long) s->bytes_unread);
	else if  (s->ack) /* was (status == SANE_STATUS_EOF) */
		DBG(min(2,DBG_READ), "%s: scanner data read ended %d blocks %ld bytes, %ld bytes unread\n", __func__, s->counter, (long) bytecount, (long) s->bytes_unread);
	else 
		DBG(min(1,DBG_READ), "%s: Image data read stopped with %s after %d blocks %ld bytes, %ld bytes unread\n", __func__, sane_strstatus(status), s->counter, (long) bytecount, (long) s->bytes_unread);

	return status;
}



/****************************************************************************
 *  kodakaio backend high-level operations ****************************************************************************/

static void
k_dev_init(Kodak_Device *dev, const char *devname, int conntype)
{
	DBG(5, "%s for %s\n", __func__,devname);

	dev->name = NULL;
	dev->model = NULL;
	dev->connection = conntype;
	dev->sane.name = devname;
	dev->sane.model = NULL;
	dev->sane.type = "flatbed scanner";
	dev->sane.vendor = "Kodak";
	dev->cap = &kodakaio_cap[CAP_DEFAULT];
}


static SANE_Status
k_set_model(KodakAio_Scanner * s, const char *model, size_t len)
{
	unsigned char *buf;
	unsigned char *p;
	struct Kodak_Device *dev = s->hw;

	if (len<1) return SANE_STATUS_INVAL; /* to handle missing model */

	buf = malloc(len + 1);
	if (buf == NULL)
		return SANE_STATUS_NO_MEM;

	memcpy(buf, model, len);
	buf[len] = '\0';

	p = &buf[len - 1];

	while (*p == ' ') {
		*p = '\0';
		p--;
	}

	if (dev->model)
		free(dev->model);

	dev->model = strndup((const char *) buf, len);
	dev->sane.model = dev->model;
	DBG(10, "%s: model is '%s'\n", __func__, dev->model);

	free(buf);

	return SANE_STATUS_GOOD;
}

static void
k_set_device (SANE_Handle handle, SANE_Word device)
{
	KodakAio_Scanner *s = (KodakAio_Scanner *) handle;
	Kodak_Device *dev = s->hw;
	int n;

	DBG(10, "%s: 0x%x\n", __func__, device);

	for (n = 0; n < NELEMS (kodakaio_cap); n++) {
		if (kodakaio_cap[n].id == device)
			break;
	}
	if (n < NELEMS(kodakaio_cap)) {
		dev->cap = &kodakaio_cap[n];
	} else {
		dev->cap = &kodakaio_cap[CAP_DEFAULT];
		DBG(1, " unknown device 0x%x, using default %s\n",
		    device, dev->cap->model);
	}
	k_set_model (s, dev->cap->model, strlen (dev->cap->model));

}

static SANE_Status
k_discover_capabilities(KodakAio_Scanner *s)
{
	SANE_Status status = SANE_STATUS_GOOD;
	Kodak_Device *dev = s->hw;
	SANE_String_Const *source_list_add = source_list;

	DBG(10, "%s\n", __func__);

	/* always add flatbed */
	*source_list_add++ = FBF_STR;
	/* TODO: How can I check for existence of an ADF??? */
	if (dev->cap->ADF == SANE_TRUE) {
		*source_list_add++ = ADF_STR;
		DBG(10, "%s: added adf to list\n", __func__);
	}

	/* TODO: Is there any capability that we can extract from the
	 *       device by some scanner command? So far, it looks like
	 *       the device does not support any reporting.
	 */

	dev->x_range = &dev->cap->fbf_x_range;
	dev->y_range = &dev->cap->fbf_y_range;

	DBG(10, "   x-range: %f %f\n", SANE_UNFIX(dev->x_range->min), SANE_UNFIX(dev->x_range->max));
	DBG(10, "   y-range: %f %f\n", SANE_UNFIX(dev->y_range->min), SANE_UNFIX(dev->y_range->max));

	DBG(5, "End of %s, status:%s\n", __func__, sane_strstatus(status));
	*source_list_add = NULL; /* add end marker to source list */
	return status;
}

static SANE_Status
k_setup_block_mode (KodakAio_Scanner *s)
{
/* works for USB and for net changing to make block size  = a number of complete lines 28/12/12 */
	s->block_len = MAX_BLOCK_SIZE / s->scan_bytes_per_line * s->scan_bytes_per_line;
	s->bytes_unread = s->data_len;
	s->counter = 0;
	s->bytes_read_in_line = 0;
	if (s->line_buffer)
		free(s->line_buffer);
	s->line_buffer = malloc(s->scan_bytes_per_line);
	if (s->line_buffer == NULL) {
		DBG(1, "out of memory (line %d)\n", __LINE__);
		return SANE_STATUS_NO_MEM;
	}

	DBG (10, " %s: Setup block mode - scan_bytes_per_line=%d, pixels_per_line=%d, depth=%d, data_len=%d, block_len=%d, blocks=%d, last_len=%d\n",
		__func__, s->scan_bytes_per_line, s->params.pixels_per_line, s->params.depth, s->data_len, s->block_len, s->blocks, s->last_len);
	return SANE_STATUS_GOOD;
}

/* Call the commands to set scanning parameters 
In the Kodak Aio the parameters are:
(x1b,"S","F",0,0,0,0,0)
(x1b,"S","S",1,0,0,0,0)
#It looks like the 4th param byte of the C command is compression 1=compress, 0=no compression
#1st (or 3rd) param could be channels, 2nd could be bits per channel
(x1b,"S","C",3,8,3,0,0)  #3,8,3,1,0) was what the kodak software used with compression
(x1b,"S","D",LowByte(Res),HighByte(Res),LowByte(Res),HighByte(Res),0) #resolution in DPI
SendColour(tcpCliSock,"R")
SendColour(tcpCliSock,"G")
SendColour(tcpCliSock,"B")
(x1b,"S","O",LowByte(x0*Res),HighByte(x0*Res),LowByte(y0*Res),HighByte(y0*Res),0) #top left in pixels
(x1b,"S","Z",LowByte(x1*Res),HighByte(x1*Res),LowByte(y1*Res),HighByte(y1*Res),0) #bot right in pixels
(x1b,"S","E",1,0,0,0,0)

*/

static SANE_Status
k_lock_scanner (KodakAio_Scanner * s)
{
	SANE_Status status;
	unsigned char reply[8];

	status = k_hello(s);
	if(status != SANE_STATUS_GOOD) {
		DBG(1, "%s k_hello failed with %s\n", __func__, sane_strstatus(status));
		return status;
	}

	if (kodakaio_txrxack(s, KodakEsp_Lock, reply)!= SANE_STATUS_GOOD) {
		DBG(1, "%s Could not lock scanner\n", __func__);
		return SANE_STATUS_IO_ERROR;
	}
	if (s->adf_loaded) DBG(5, "%s scanner locked, with docs in adf\n", __func__);
	else DBG(5, "%s scanner locked, with no docs in adf\n", __func__);
	return SANE_STATUS_GOOD;
}

static SANE_Status
k_set_scanning_parameters(KodakAio_Scanner * s)
{
	SANE_Status status;
	unsigned char rs, source;
	SANE_Int scan_pixels_per_line = 0;
	int dpi, optres;

	dpi = s->val[OPT_RESOLUTION].w;
	optres = s->hw->cap->optical_res;

	/* Find the resolution in the res list and assign the index (rs) */
	for (rs=0; rs < s->hw->cap->res_list_size; rs++ ) {
		if ( dpi == s->hw->cap->res_list[rs] )
			break;
	}

	/* ADF used? */
	if (strcmp(source_list[s->val[OPT_SOURCE].w], ADF_STR) == 0) {
		source = 0x00;
	} else {
		source = 0x01;
	}


	/* TODO: Any way to set PREVIEW??? */

	/* Remaining bytes unused */
	status = cmd_set_scanning_parameters(s, dpi, 
		s->left * dpi / optres, s->top * dpi / optres,  /* top/left start (dpi units)*/
		s->params.pixels_per_line, s->params.lines, /* extent was s->width, s->height*/
		source); /* source */

	if (status != SANE_STATUS_GOOD)
		DBG (1, "%s: Command cmd_set_scanning_parameters failed, %s\n",
		     __func__, sane_strstatus(status));

	/* Now query the scanner for the current image parameters */
	status = cmd_get_scanning_parameters (s,
			&s->params.format, &s->params.depth,
			&scan_pixels_per_line,
			&s->params.pixels_per_line, &s->params.lines);
	if (status != SANE_STATUS_GOOD) {
		DBG (1, "%s: Command cmd_get_scanning_parameters failed, %s\n",
		     __func__, sane_strstatus(status));
		return status;
	}

	/* Calculate how many bytes are really used per line */
	s->params.bytes_per_line = ceil (s->params.pixels_per_line * s->params.depth / 8.0);
	if (s->val[OPT_MODE].w == MODE_COLOR)
		s->params.bytes_per_line *= 3;

	/* Calculate how many bytes per line will be returned by the scanner.
	magicolor needed this because it uses padding so scan bytes per line != image bytes per line.
	 * The values needed for this are returned by get_scanning_parameters */
	s->scan_bytes_per_line = 3 * ceil (scan_pixels_per_line); /* we always scan in colour 8 bit */
	s->data_len = s->scan_bytes_per_line * floor (s->height * dpi / optres + 0.5); /* NB this is the length for a full scan */
	DBG (5, "Check: scan_bytes_per_line = %d  s->params.bytes_per_line = %d \n", s->scan_bytes_per_line, s->params.bytes_per_line);

/* k_setup_block_mode at the start of each page for adf to work */
	status = k_setup_block_mode (s);
	if (status != SANE_STATUS_GOOD)
		DBG (1, "%s: Command k_setup_block_mode failed, %s\n",
		     __func__, sane_strstatus(status));

	DBG (18, "%s: bytes_read_in_line: %d\n", __func__, s->bytes_read_in_line);
	return status;
}

static SANE_Status
k_check_adf(KodakAio_Scanner * s)
{
/* 20/2/12  detect paper in the adf? acknowledge esc S S 00 01.. ?*/

	if (! s->adf_loaded) {
		DBG(5, "%s: NO DOCS\n", __func__);
		return SANE_STATUS_NO_DOCS;
	}
	else {

	/* TODO: Check for jam in ADF */
		DBG(5, "%s: DOCS IN ADF\n", __func__);
		return SANE_STATUS_GOOD;
	}
}

static SANE_Status
k_scan_finish(KodakAio_Scanner * s)
{
	SANE_Status status = SANE_STATUS_GOOD;
	DBG(10, "%s called\n", __func__);

	/* If we have not yet read all data, cancel the scan */
	if (s->buf && !s->eof)
		status = cmd_cancel_scan (s);

	if (s->line_buffer)
		free (s->line_buffer);
	s->line_buffer = NULL;
	free(s->buf);
	s->buf = s->end = s->ptr = NULL;

	return status;
}

static void
k_copy_image_data(KodakAio_Scanner * s, SANE_Byte * data, SANE_Int max_length,
		   SANE_Int * length)
/* copies the read data from s->line_buffer to the position in data pointer to by s->ptr 
uncompressed data is RRRR...GGGG...BBBB  per line */
{
		SANE_Int bytes_available;
		SANE_Int threshold;

		DBG (min(18,DBG_READ), "%s: bytes_read  in line: %d\n", __func__, s->bytes_read_in_line);
		*length = 0;

		threshold = 255 - (int) (SANE_UNFIX(s->val[OPT_THRESHOLD].w) * 255.0 / 100.0 + 0.5); /* 255 - for the grey scale version */
		DBG (20, "%s: threshold: %d\n", __func__, threshold);

		while ((max_length >= s->params.bytes_per_line) && (s->ptr < s->end)) {
			SANE_Int bytes_to_copy = s->scan_bytes_per_line - s->bytes_read_in_line;
			/* First, fill the line buffer for the current line: */
			bytes_available = (s->end - s->ptr);
			/* Don't copy more than we have buffer and available */
			if (bytes_to_copy > bytes_available)
				bytes_to_copy = bytes_available;

			if (bytes_to_copy > 0) {
				memcpy (s->line_buffer + s->bytes_read_in_line, s->ptr, bytes_to_copy);
				s->ptr += bytes_to_copy;
				s->bytes_read_in_line += bytes_to_copy;
			}

			/* We have filled as much as possible of the current line
			 * with data from the scanner. If we have a complete line,
			 * copy it over. 
			line points to the current byte in the input s->line_buffer
			data points to the output buffer*/
			if ((s->bytes_read_in_line >= s->scan_bytes_per_line) &&
			    (s->params.bytes_per_line <= max_length))
			{
				SANE_Int i;
				SANE_Byte *line = s->line_buffer;
				*length += s->params.bytes_per_line;

				for (i=0; i< s->params.pixels_per_line; ++i) {
				/* different behaviour for each mode */

					if (s->val[OPT_MODE].w == MODE_COLOR){
					/*interlace was subtracting from 255 until 6/9/14 */
					 	*data++ = 255-line[0]; /*red */
					 	*data++ = 255-line[s->params.pixels_per_line];  /*green */
					 	*data++ = 255-line[2 * s->params.pixels_per_line];  /*blue */
					}

					else if (s->val[OPT_MODE].w == MODE_LINEART) { /* gives 1 bit output  */
        					/*output image location*/
        					int offset = i % 8;
        					unsigned char mask = 0x80 >> offset;
						/*set if any colour is over the threshold  */
						if (line[0] < threshold || line[s->params.pixels_per_line] < threshold || line[2 * s->params.pixels_per_line] < threshold)
          						*data &= ~mask;     /* white clear the bit in mask */
        					else
          						*data |= mask;      /* black set the bit in mask */
                  
        					if (offset == 7 || i == s->params.pixels_per_line-1) 
            						data++; /* move on a byte if the byte is full or the line is complete */
					}

					else { /* greyscale - Average the 3 colours */
						*data++ = (255-line[0]
						+255-line[s->params.pixels_per_line]
						+255-line[2 * s->params.pixels_per_line])
						/ 3;
					}
						line++;
				}
/*debug file The same for color or grey because the scan is colour */
				if (RawScan != NULL) {
					for (i=0; i< s->scan_bytes_per_line; ++i) fputc(s->line_buffer[i],RawScan); 
				}
				max_length -= s->params.bytes_per_line;
				s->bytes_read_in_line -= s->scan_bytes_per_line;
			}
		}
}

static SANE_Status
k_init_parametersta(KodakAio_Scanner * s)
{
	int dpi, optres;
	/* struct mode_param *mparam; */

	DBG(10, "%s\n", __func__);

	memset(&s->params, 0, sizeof(SANE_Parameters));

	dpi = s->val[OPT_RESOLUTION].w;
	optres = s->hw->cap->optical_res;

	 /* mparam = &mode_params[s->val[OPT_MODE].w];does this get used? */

	if (SANE_UNFIX(s->val[OPT_BR_Y].w) == 0 ||
		SANE_UNFIX(s->val[OPT_BR_X].w) == 0)
		return SANE_STATUS_INVAL;

	/* TODO: Use OPT_RESOLUTION or fixed 600dpi for left/top/width/height? */
	s->left = ((SANE_UNFIX(s->val[OPT_TL_X].w) / MM_PER_INCH) * optres) + 0.5;

	s->top = ((SANE_UNFIX(s->val[OPT_TL_Y].w) / MM_PER_INCH) * optres) + 0.5;

	/* width in pixels */
	s->width =
		((SANE_UNFIX(s->val[OPT_BR_X].w -
			   s->val[OPT_TL_X].w) / MM_PER_INCH) * optres) + 0.5;

	s->height = ((SANE_UNFIX(s->val[OPT_BR_Y].w -
			   s->val[OPT_TL_Y].w) / MM_PER_INCH) * optres) + 0.5;

	DBG(20, "%s: s->width = %d, s->height = %d optres units\n",
		__func__, s->width, s->height);

	s->params.pixels_per_line = s->width * dpi / optres + 0.5;

	/* ADF used without padding? added 30/12/12 */
	if (strcmp(source_list[s->val[OPT_SOURCE].w], ADF_STR) == 0 && !s->val[OPT_PADDING].w)
		s->params.lines = -1;
	else
		s->params.lines = s->height * dpi / optres + 0.5;


	DBG(20, "%s: resolution = %d, preview = %d\n",
		__func__, dpi, s->val[OPT_PREVIEW].w);

	DBG(20, "%s: %p %p tlx %f tly %f brx %f bry %f [mm]\n",
	    __func__, (void *) s, (void *) s->val,
	    SANE_UNFIX(s->val[OPT_TL_X].w), SANE_UNFIX(s->val[OPT_TL_Y].w),
	    SANE_UNFIX(s->val[OPT_BR_X].w), SANE_UNFIX(s->val[OPT_BR_Y].w));

	/*
	 * The default color depth is stored in mode_params.depth:‭
	 */
	if (mode_params[s->val[OPT_MODE].w].depth == 1)
		s->params.depth = 1;
	else {
		s->params.depth = s->val[OPT_BIT_DEPTH].w;
	}
	DBG(20, "%s: bit depth = s->params.depth = %d\n", __func__,s->params.depth);
	s->params.last_frame = SANE_TRUE;
	s->params.bytes_per_line = 3 * ceil (s->params.depth * s->params.pixels_per_line / 8.0);

/* kodak only scans in color and conversion to grey or lineart is done in the driver 
		s->params.format = SANE_FRAME_RGB; */
	DBG(20, "%s: s->val[OPT_MODE].w = %d (color is %d)\n", __func__,s->val[OPT_MODE].w, MODE_COLOR);
	if (s->val[OPT_MODE].w == MODE_COLOR) s->params.format = SANE_FRAME_RGB;
	else if (s->val[OPT_MODE].w == MODE_LINEART) s->params.format = SANE_FRAME_GRAY;
	else s->params.format = SANE_FRAME_GRAY;

	DBG(20, "%s: format=%d, bytes_per_line=%d, lines=%d\n", __func__, s->params.format, s->params.bytes_per_line, s->params.lines);
	return (s->params.lines >= -1) ? SANE_STATUS_GOOD : SANE_STATUS_INVAL;
}

static SANE_Status
k_start_scan(KodakAio_Scanner * s)
{
	SANE_Status status;

	status = cmd_start_scan (s, s->data_len);
	if (status != SANE_STATUS_GOOD ) {
		DBG (1, "%s: starting the scan failed (%s)\n", __func__, sane_strstatus(status));
	}
	return status;
}



static SANE_Status
k_read(struct KodakAio_Scanner *s)
{
	unsigned char rx[8];

/* monitors progress of blocks and calls cmd_read_data to get each block 
you don't know how many blocks there will be in advance because their size may be determined by the scanner*/
	SANE_Status status = SANE_STATUS_GOOD;
	size_t buf_len = 0;

	/* have we passed everything we read to sane? */
	if (s->ptr == s->end) {
		if (s->eof)
			return SANE_STATUS_EOF;

		s->counter++;
		if (s->bytes_unread >= s->block_len)
			buf_len = s->block_len;
		else
			buf_len = s->bytes_unread;
		DBG(min(20,DBG_READ), "%s: block %d, size %lu\n", __func__,
			s->counter, (unsigned long) buf_len);

		/* receive image data + error code */
		status = cmd_read_data (s, s->buf, &buf_len);
		if (status != SANE_STATUS_GOOD && status != SANE_STATUS_EOF) { /* was just GOOD 20/2/12 */
			DBG (1, "%s: Receiving image data failed (%s)\n",
					__func__, sane_strstatus(status));
			cmd_cancel_scan(s);
			return status;
		}

		DBG(min(14,DBG_READ), "%s: success %lu bytes of block %d, %d remain\n", __func__, (unsigned long) buf_len, s->counter, s->bytes_unread);

		if (s->bytes_unread > 0) {
			if (s->canceling) {
				cmd_cancel_scan(s);
				return SANE_STATUS_CANCELLED;
			}
			if (status == SANE_STATUS_EOF) {
				/* page ended prematurely.  */
			}
		} 
		else { /* s->bytes_unread <=0 This is the end of a page */
			s->eof = SANE_TRUE;
			DBG(min(10,DBG_READ), "%s: set EOF after %d blocks\n=============\n", __func__, s->counter);
/* look for the terminating ack if required */
			if (!s->ack) {
				if (kodakaio_expect_ack(s, rx) == SANE_STATUS_GOOD) {
					s->ack = SANE_TRUE;
				}
				else {
					DBG(min(1,DBG_READ), "%s: Did not get expected ack at end of page\n", __func__);
					return SANE_STATUS_IO_ERROR;
				}
			}
		}
		s->end = s->buf + buf_len;
		s->ptr = s->buf;
	}
	else {
		DBG(min(20,DBG_READ), "%s: data left in buffer\n", __func__);
	}
	return status;
}

/*
 *   SANE API implementation (high-level functions) 
*/

static struct KodakaioCap *
get_device_from_identification (const char *ident, const char *vid, const char *pid)
{
	int n;
	SANE_Word pidnum, vidnum;

	if(sscanf(vid, "%x", &vidnum) == EOF) {
    		DBG(5, "could not convert hex vid <%s>\n", vid);
    		return NULL;
	}
	if(sscanf(pid, "%x", &pidnum) == EOF) {
    		DBG(5, "could not convert hex pid <%s>\n", pid);
    		return NULL;
	}
	for (n = 0; n < NELEMS (kodakaio_cap); n++) {
		
		if (strcmp (kodakaio_cap[n].model, ident)==0) {
			DBG(20, "matched <%s> & <%s>\n", kodakaio_cap[n].model, ident);
			return &kodakaio_cap[n];
		}
		else 
		if (kodakaio_cap[n].id == pidnum && 0x040A == vidnum) {
			DBG(20, "matched <%s> & <%s:%s>\n", kodakaio_cap[n].model, vid, pid);
			return &kodakaio_cap[n];
		}
		else {
			DBG(20, "not found <%s> & <%s>\n", kodakaio_cap[n].model, pid);
		}
	}
	return NULL;
}

/*
 * close_scanner()
 *
 * Close the open scanner. Depending on the connection method, a different
 * close function is called.
 */
static void
close_scanner(KodakAio_Scanner *s)
{
	DBG(7, "%s: fd = %d\n", __func__, s->fd);

	if (s->fd == -1)
		return;

	k_scan_finish(s);
	if (s->hw->connection == SANE_KODAKAIO_NET) {
		sanei_kodakaio_net_close(s);
		sanei_tcp_close(s->fd);
	} else if (s->hw->connection == SANE_KODAKAIO_USB) {
		sanei_usb_close(s->fd);
	}

	s->fd = -1;
}

static SANE_Bool
split_scanner_name (const char *name, char * IP, unsigned int *model)
{
	const char *device = name;
	const char *qm;
	*model = 0;
	/* cut off leading net: */
	if (strncmp(device, "net:", 4) == 0)
		device = &device[4];

	qm = strchr(device, '?');
	if (qm != NULL) {
		size_t len = qm-device;
		strncpy (IP, device, len);
		IP[len] = '\0';
		qm++;
		if (strncmp(qm, "model=", 6) == 0) {
			qm += 6;
			if (!sscanf(qm, "0x%x", model))
				sscanf(qm, "%x", model);
		}
	} else {
		strcpy (IP, device);
	}
	return SANE_TRUE;
}

/*
 * open_scanner()
 *
 * Open the scanner device. Depending on the connection method,
 * different open functions are called.
 */

static SANE_Status
open_scanner(KodakAio_Scanner *s)
{
	SANE_Status status = 0;

	DBG(7, "%s: %s\n", __func__, s->hw->sane.name);

	if (s->fd != -1) {
		DBG(10, "scanner is already open: fd = %d\n", s->fd);
		return SANE_STATUS_GOOD;	/* no need to open the scanner */
	}

	if (s->hw->connection == SANE_KODAKAIO_NET) {
		/* device name has the form net:ipaddr?model=... */
		char IP[1024];
		unsigned int model = 0;
		if (!split_scanner_name (s->hw->sane.name, IP, &model))
			return SANE_STATUS_INVAL;
			DBG(10, "split_scanner_name OK model=0x%x\n",model);
/* normal with IP */
		status = sanei_tcp_open(IP, 9101, &s->fd);  /* (host,port,file pointer) */

		if (status != SANE_STATUS_GOOD ) DBG(1, "Is network scanner switched on?\n");

		if (model>0)
			k_set_device (s, model);
		if (status == SANE_STATUS_GOOD) {
			status = sanei_kodakaio_net_open (s);
		}
else			DBG(1, "status was not good at net open\n");


	} else if (s->hw->connection == SANE_KODAKAIO_USB) {
			DBG(7, "trying to open usb\n");
		status = sanei_usb_open(s->hw->sane.name, &s->fd);
		if (s->hw->cap->out_ep>0)
			sanei_usb_set_endpoint (s->fd,
				USB_DIR_OUT | USB_ENDPOINT_TYPE_BULK, s->hw->cap->out_ep);
		if (s->hw->cap->in_ep>0)
			sanei_usb_set_endpoint (s->fd,
				USB_DIR_IN | USB_ENDPOINT_TYPE_BULK, s->hw->cap->in_ep);
	}

	if (status == SANE_STATUS_ACCESS_DENIED) {
		DBG(1, "please check that you have permissions on the device.\n");
		DBG(1, "if this is a multi-function device with a printer,\n");
		DBG(1, "disable any conflicting driver (like usblp).\n");
	}

	if (status != SANE_STATUS_GOOD)
		DBG(1, "%s open failed: %s\n", s->hw->sane.name,
			sane_strstatus(status));
	else
		DBG(3, "scanner opened\n");
/* add check here of usb properties? */
/*sanei_usb_get_descriptor( SANE_Int dn, struct sanei_usb_dev_descriptor *desc );*/


	return status;
}

static SANE_Status
detect_usb(struct KodakAio_Scanner *s)
{
	SANE_Status status;
	SANE_Word vendor, product;
	int i, numIds;
	SANE_Bool is_valid;

	/* if the sanei_usb_get_vendor_product call is not supported,
	 * then we just ignore this and rely on the user to config
	 * the correct device.
	 */

	status = sanei_usb_get_vendor_product(s->fd, &vendor, &product);
	if (status != SANE_STATUS_GOOD) {
		DBG(1, "the device cannot be verified - will continue\n");
		return SANE_STATUS_GOOD;
	}

	/* check the vendor ID to see if we are dealing with a kodak device */
	if (vendor != SANE_KODAKAIO_VENDOR_ID) {
		/* this is not a supported vendor ID */
		DBG(1, "not a Kodak Aio device at %s (vendor id=0x%x)\n",
		    s->hw->sane.name, vendor);
		return SANE_STATUS_INVAL;
	}

	numIds = kodakaio_getNumberOfUSBProductIds();
	is_valid = SANE_FALSE;
	i = 0;

	/* check all known product IDs to verify that we know
	 *	   about the device */
	while (i != numIds && !is_valid) {
	/*	if (product == kodakaio_usb_product_ids[i]) */
		if (product == kodakaio_cap[i].id) 
			is_valid = SANE_TRUE;
		i++;
	}

	if (is_valid == SANE_FALSE) {
		DBG(1, "the device at %s is not a supported (product id=0x%x)\n",
		    s->hw->sane.name, product);
		return SANE_STATUS_INVAL;
	}

	DBG(2, "found valid usb Kodak Aio scanner: 0x%x/0x%x (vendorID/productID)\n",
	    vendor, product);
	k_set_device(s, product); /* added 21/12/11 to try and get a name for the device */

	return SANE_STATUS_GOOD;
}

/*
 * used by attach* and sane_get_devices
 * a ptr to a single-linked list of Kodak_Device structs
 * a ptr to a null term array of ptrs to SANE_Device structs
 */
static int num_devices;		/* number of scanners attached to backend */
static Kodak_Device *first_dev;	/* first scanner in list */
static const SANE_Device **devlist = NULL;

static struct KodakAio_Scanner *
scanner_create(struct Kodak_Device *dev, SANE_Status *status)
{
	struct KodakAio_Scanner *s;

	s = malloc(sizeof(struct KodakAio_Scanner));
	if (s == NULL) {
		*status = SANE_STATUS_NO_MEM;
		return NULL;
	}

	memset(s, 0x00, sizeof(struct KodakAio_Scanner));

	s->fd = -1;
	s->hw = dev;

	return s;
}

static struct KodakAio_Scanner *
device_detect(const char *name, int type, SANE_Status *status)
{
	struct KodakAio_Scanner *s;
	struct Kodak_Device *dev;

	/* try to find the device in our list */
	for (dev = first_dev; dev; dev = dev->next) {
		if (strcmp(dev->sane.name, name) == 0) {
			dev->missing = 0;
			DBG (10, "%s: Device %s already attached!\n", __func__,
			     name);
			return scanner_create(dev, status);
		}
	}

	if (type == SANE_KODAKAIO_NODEV) {
		*status = SANE_STATUS_INVAL;
		return NULL;
	}

	/* alloc and clear our device structure */
	dev = malloc(sizeof(*dev));
	if (!dev) {
		*status = SANE_STATUS_NO_MEM;
		return NULL;
	}
	memset(dev, 0x00, sizeof(struct Kodak_Device));

	s = scanner_create(dev, status);
	if (s == NULL)
		return NULL;

	k_dev_init(dev, name, type);

	*status = open_scanner(s);
	if (*status != SANE_STATUS_GOOD) {
		free(s);
		free(dev);
		return NULL;
	}

	/* from now on, close_scanner() must be called */

	/* USB requires special care */
	if (dev->connection == SANE_KODAKAIO_USB) {
		*status = detect_usb(s);
	}

	if (*status != SANE_STATUS_GOOD)
		goto close;

	/* set name and model (if not already set) */
	if (dev->model == NULL)
		k_set_model(s, "generic", 7);

	dev->name = strdup(name);
	dev->sane.name = dev->name;

	/* do we need to discover capabilities here? */
	*status = k_discover_capabilities(s);
	if (*status != SANE_STATUS_GOOD)
		goto close;

	if (source_list[0] == NULL || dev->cap->dpi_range.min == 0) {
		DBG(1, "something is wrong in the discovery process, aborting.\n");
		*status = SANE_STATUS_IO_ERROR;
		goto close;
	}

	/* add this scanner to the device list */
	num_devices++;
	dev->missing = 0;
	dev->next = first_dev;
	first_dev = dev;

	return s;

	close:
	close_scanner(s);
	free(dev);
	free(s);
	return NULL;
}



#if WITH_AVAHI
/* ProcessAvahiDevice is called to process each discovered device in turn */
void 
ProcessAvahiDevice(const char *device_id, const char *vid, const char *pid, const char *ip_addr)
{
	struct KodakaioCap *cap;

	DBG(min(10,DBG_AUTO),"device_id = <%s> vid:pid = <%s:%s>\n", device_id,vid,pid);

/* check if it is a model likely to be supported: "KODAK ESP" or "KODAK HERO" 

	DBG(min(10,DBG_AUTO),"look up model <%s>\n", device_model); */
	cap = get_device_from_identification("", vid, pid);
	if (cap == NULL) {
		return;
	} 

	DBG(min(10,DBG_AUTO), "%s: Found autodiscovered device: %s (type 0x%x)\n", __func__, cap->model, cap->id);
		attach_one_net (ip_addr, cap->id);
}


static void resolve_callback(
    AvahiServiceResolver *r,
    AVAHI_GCC_UNUSED AvahiIfIndex interface,
    AVAHI_GCC_UNUSED AvahiProtocol protocol,
    AvahiResolverEvent event,
    const char *name,
    const char *type,
    const char *domain,
    const char *host_name,
    const AvahiAddress *address,
    uint16_t port,
    AvahiStringList *txt,
    AvahiLookupResultFlags flags,
    AVAHI_GCC_UNUSED void* userdata) {

	AvahiStringList *vid_pair_list = NULL, *pid_pair_list = NULL;
	char *pidkey, *pidvalue;
	char *vidkey, *vidvalue;
	size_t valuesize;
	NOT_USED (flags);

    assert(r);

    /* Called whenever a service has been resolved successfully or timed out */

    switch (event) {
        case AVAHI_RESOLVER_FAILURE:
            DBG(min(1,DBG_AUTO), "(Resolver) Failed to resolve service '%s' of type '%s' in domain '%s': %s\n", name, type, domain, avahi_strerror(avahi_client_errno(avahi_service_resolver_get_client(r))));
            break;

        case AVAHI_RESOLVER_FOUND: {
            char a[AVAHI_ADDRESS_STR_MAX];

            avahi_address_snprint(a, sizeof(a), address);

/* Output short for Kodak ESP */
	DBG(min(10,DBG_AUTO), "%s:%u  %s\n", a,port,host_name);

	vid_pair_list = avahi_string_list_find(txt, "vid");
	if(vid_pair_list != NULL) {
		avahi_string_list_get_pair(vid_pair_list, &vidkey, &vidvalue, &valuesize);
		DBG(min(10,DBG_AUTO), "%s=%s  ", vidkey, vidvalue);
	}
	else	DBG(min(10,DBG_AUTO), "failed to find key vid\n");

	pid_pair_list = avahi_string_list_find(txt, "pid");
	if(pid_pair_list != NULL) {
		avahi_string_list_get_pair(pid_pair_list, &pidkey, &pidvalue, &valuesize);
		DBG(min(10,DBG_AUTO), "%s=%s\n", pidkey, pidvalue);
	}
	else	DBG(min(10,DBG_AUTO), "failed to find key pid\n");

	if(pid_pair_list != NULL && vid_pair_list != NULL) {
		ProcessAvahiDevice(name, vidvalue, pidvalue, a);
	}
	else DBG(min(10,DBG_AUTO), "didn't call ProcessAvahiDevice\n");

	if(vid_pair_list != NULL) {
		avahi_free(vidkey); 
		avahi_free(vidvalue);
		DBG(min(15,DBG_AUTO), "vidkey and vidvalue freed\n");
	}
	if(pid_pair_list != NULL) {
		avahi_free(pidkey); 
		avahi_free(pidvalue);
		DBG(min(15,DBG_AUTO), "pidkey and pidvalue freed\n");
	}
        }
    }
    DBG(min(10,DBG_AUTO), "ending resolve_callback\n");
    avahi_service_resolver_free(r);
}

static void browse_callback(
    AvahiServiceBrowser *b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const char *name,
    const char *type,
    const char *domain,
    AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
    void* userdata) {

    AvahiClient *c = userdata;
    assert(b);

    /* Called whenever a new services becomes available on the LAN or is removed from the LAN */
    switch (event) {
        case AVAHI_BROWSER_FAILURE:

            DBG(min(1,DBG_AUTO), "(Browser) %s\n", avahi_strerror(avahi_client_errno(avahi_service_browser_get_client(b))));
            avahi_simple_poll_quit(simple_poll);
            return;

        case AVAHI_BROWSER_NEW:
            DBG(min(5,DBG_AUTO), "(Browser) NEW: service '%s' of type '%s' in domain '%s'\n", name, type, domain);

            /* We ignore the returned resolver object. In the callback
               function we free it. If the server is terminated before
               the callback function is called the server will free
               the resolver for us. */

            if (!(avahi_service_resolver_new(c, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, 0, resolve_callback, c)))
                DBG(min(1,DBG_AUTO), "Failed to resolve service '%s': %s\n", name, avahi_strerror(avahi_client_errno(c)));

            break;

        case AVAHI_BROWSER_REMOVE:
            DBG(min(1,DBG_AUTO), "(Browser) REMOVE: service '%s' of type '%s' in domain '%s'\n", name, type, domain);
            break;

        case AVAHI_BROWSER_ALL_FOR_NOW:
        case AVAHI_BROWSER_CACHE_EXHAUSTED:
            DBG(min(5,DBG_AUTO), "(Browser) %s\n", event == AVAHI_BROWSER_CACHE_EXHAUSTED ? "CACHE_EXHAUSTED" : "ALL_FOR_NOW");
            break;
    }
}

static void client_callback(AvahiClient *c, AvahiClientState state, AVAHI_GCC_UNUSED void * userdata) {
    assert(c);

    /* Called whenever the client or server state changes */

    if (state == AVAHI_CLIENT_FAILURE) {
        DBG(min(1,DBG_AUTO), "Server connection failure: %s\n", avahi_strerror(avahi_client_errno(c)));
        avahi_simple_poll_quit(simple_poll);
    }
}


static int
kodak_network_discovery(const char*host)
/* If host = NULL do autodiscovery. If host != NULL try to verify the model 
First version only does autodiscovery */
{
    AvahiClient *client = NULL;
    AvahiServiceBrowser *sb = NULL;
    int error;
    int i, ret = 1;
	NOT_USED(host);

	DBG(2, "%s: called\n", __func__);

    /* Allocate main loop object */
    if (!(simple_poll = avahi_simple_poll_new())) {
        DBG(min(1,DBG_AUTO), "Failed to create simple poll object.\n");
        goto fail;
    }

    /* Allocate a new client */
    client = avahi_client_new(avahi_simple_poll_get(simple_poll), 0, client_callback, NULL, &error);

    /* Check wether creating the client object succeeded */
    if (!client) {
        DBG(min(1,DBG_AUTO), "Failed to create client: %s\n", avahi_strerror(error));
        goto fail;
    }

    /* Create the service browser */
    if (!(sb = avahi_service_browser_new(client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, "_scanner._tcp", NULL, 0, browse_callback, client))) {
        DBG(min(1,DBG_AUTO), "Failed to create service browser: %s\n", avahi_strerror(avahi_client_errno(client)));
        goto fail;
    }

    /* Run the main loop */
	for(i=1;i<K_SNMP_Timeout/POLL_ITN_MS;++i) {
    		avahi_simple_poll_iterate(simple_poll,POLL_ITN_MS);
	}
    ret = 0;

fail:


    /* Cleanup things  */
        DBG(min(10,DBG_AUTO), "Cleaning up avahi.\n");
    if (sb)
        avahi_service_browser_free(sb);

    if (client)
        avahi_client_free(client);

    if (simple_poll)
        avahi_simple_poll_free(simple_poll);

    return ret;
}

#endif


static SANE_Status
attach(const char *name, int type)
{
	SANE_Status status;
	KodakAio_Scanner *s;

	DBG(7, "%s: devname = %s, type = %d\n", __func__, name, type);

	s = device_detect(name, type, &status);
	if(s == NULL)
		return status;

	close_scanner(s);
	free(s);
	return status;
}

SANE_Status
attach_one_usb(const char *dev)
{
	DBG(7, "%s: dev = %s\n", __func__, dev);
	return attach(dev, SANE_KODAKAIO_USB);
}

static SANE_Status
attach_one_net(const char *dev, unsigned int model)
{
	char name[1024];

	DBG(7, "%s: dev = %s\n", __func__, dev);
	if (model > 0) {
		snprintf(name, 1024, "net:%s?model=0x%x", dev, model);
	} else {
		snprintf(name, 1024, "net:%s", dev);
	}

	return attach(name, SANE_KODAKAIO_NET);
}

static SANE_Status
attach_one_config(SANEI_Config __sane_unused__ *config, const char *line)
{
	int vendor, product, timeout;

	int len = strlen(line);

	DBG(7, "%s: len = %d, line = %s\n", __func__, len, line);

	if (sscanf(line, "usb %i %i", &vendor, &product) == 2) {
		/* add the vendor and product IDs to the list of
		 *		   known devices before we call the attach function */

		int numIds = kodakaio_getNumberOfUSBProductIds();

		if (vendor != SANE_KODAKAIO_VENDOR_ID) {
			DBG(7, "Wrong vendor: numIds = %d, vendor = %d\n", numIds, vendor);
			return SANE_STATUS_INVAL; /* this is not a Kodak device */
		}
		/* kodakaio_usb_product_ids[numIds - 1] = product; */
		kodakaio_cap[numIds - 1].id = product;

		sanei_usb_attach_matching_devices(line, attach_one_usb);

	} else if (strncmp(line, "usb", 3) == 0 && len == 3) {
		int i, numIds;
		/* auto detect ?  */
		numIds = kodakaio_getNumberOfUSBProductIds();

		for (i = 0; i < numIds; i++) {
/*			sanei_usb_find_devices(SANE_KODAKAIO_VENDOR_ID,
					       kodakaio_usb_product_ids[i], attach_one_usb); */
			sanei_usb_find_devices(SANE_KODAKAIO_VENDOR_ID,
					       kodakaio_cap[i].id, attach_one_usb);
		}

	} else if (strncmp(line, "net", 3) == 0) {

		/* remove the "net" sub string */
		const char *name = sanei_config_skip_whitespace(line + 3);
		char IP[1024];
		unsigned int model = 0;

		if (strncmp(name, "autodiscovery", 13) == 0) {

#if WITH_AVAHI
			DBG (30, "%s: Initiating network autodiscovery via avahi\n", __func__);
			kodak_network_discovery(NULL);
#else
			DBG (20, "%s: Network autodiscovery not done because not configured with avahi.\n", __func__);
#endif

		} else if (sscanf(name, "%s %x", IP, &model) == 2) {
			DBG(30, "%s: Using network device on IP %s, forcing model 0x%x\n", __func__, IP, model);
			attach_one_net(IP, model);
		} else {
				DBG(1, "%s: net entry %s may be a host name?\n", __func__, name);
				attach_one_net(name, 0);
			}

	} else if (sscanf(line, "snmp-timeout %i\n", &timeout)) {
		/* Timeout for auto network discovery */
		DBG(50, "%s: network auto-discovery timeout set to %d\n", __func__, timeout);
		K_SNMP_Timeout = timeout;

	} else if (sscanf(line, "scan-data-timeout %i\n", &timeout)) {
		/* Timeout for scan data requests */
		DBG(50, "%s: Scan data timeout set to %d\n", __func__, timeout);
		K_Scan_Data_Timeout = timeout;

	} else if (sscanf(line, "request-timeout %i\n", &timeout)) {
		/* Timeout for all other read requests */
		DBG(50, "%s: Request timeout set to %d\n", __func__, timeout);
		K_Request_Timeout = timeout;
	
	} else {
		/* TODO: Warning about unparsable line! */
	}

	return SANE_STATUS_GOOD;
}

static void
free_devices(void)
{
	Kodak_Device *dev, *next;

	DBG(5, "%s\n", __func__);

	for (dev = first_dev; dev; dev = next) {
		next = dev->next;
		free(dev->name);
		free(dev->model);
		free(dev);
	}

	if (devlist)
		free(devlist);
	devlist = NULL;
	first_dev = NULL;
}

SANE_Status
sane_init(SANE_Int *version_code, SANE_Auth_Callback __sane_unused__ authorize)
{
	DBG_INIT();
	DBG(1, "========================================== \n");
	DBG(2, "%s: " PACKAGE " " VERSION "\n", __func__);

	DBG(1, "kodakaio backend, version %i.%i.%i\n",
	    KODAKAIO_VERSION, KODAKAIO_REVISION, KODAKAIO_BUILD);
	DBG(2, "%s: called\n", __func__);
	if (version_code != NULL)
		*version_code = SANE_VERSION_CODE(SANE_CURRENT_MAJOR, V_MINOR,
						  KODAKAIO_BUILD);
	sanei_usb_init();

#if WITH_AVAHI
	DBG(min(3,DBG_AUTO), "avahi detected\n");
#else
	DBG(min(3,DBG_AUTO), "avahi not detected\n");
#endif
	return SANE_STATUS_GOOD;
}

/* Clean up the list of attached scanners. */
void
sane_exit(void)
{
	DBG(5, "%s\n", __func__);
	free_devices();
}

SANE_Status
sane_get_devices(const SANE_Device ***device_list, SANE_Bool __sane_unused__ local_only)
{
	Kodak_Device *dev, *s, *prev=0;
	int i;

	DBG(2, "%s: called\n", __func__);

	sanei_usb_init();

	/* mark all existing scanners as missing, attach_one will remove mark */
	for (s = first_dev; s; s = s->next) {
		s->missing = 1;
	}

	/* Read the config, mark each device as found, possibly add new devs */
	sanei_configure_attach(KODAKAIO_CONFIG_FILE, NULL,
			       attach_one_config);

	/*delete missing scanners from list*/
	for (s = first_dev; s;) {
		if (s->missing) {
			DBG (5, "%s: missing scanner %s\n", __func__, s->name);

			/*splice s out of list by changing pointer in prev to next*/
			if (prev) {
				prev->next = s->next;
				free (s);
				s = prev->next;
				num_devices--;
			} else {
				/*remove s from head of list */
				first_dev = s->next;
				free(s);
				s = first_dev;
				prev=NULL;
				num_devices--;
			}
		} else {
			prev = s;
			s = prev->next;
		}
	}

	DBG (15, "%s: found %d scanner(s)\n", __func__, num_devices);
	for (s = first_dev; s; s=s->next) {
		DBG (15, "%s: found scanner %s\n", __func__, s->name);
	}

	if (devlist)
		free (devlist);

	devlist = malloc((num_devices + 1) * sizeof(devlist[0]));
	if (!devlist) {
		DBG(1, "out of memory (line %d)\n", __LINE__);
		return SANE_STATUS_NO_MEM;
	}

	DBG(5, "%s - results:\n", __func__);

	for (i = 0, dev = first_dev; i < num_devices && dev; dev = dev->next, i++) {
		DBG(5, " %d (%d): %s\n", i, dev->connection, dev->model);
		devlist[i] = &dev->sane;
	}

	devlist[i] = NULL;

	if(device_list){
		*device_list = devlist;
	}

	return SANE_STATUS_GOOD;
}

static SANE_Status
init_options(KodakAio_Scanner *s)
{
	int i;
	SANE_Word *res_list;
	DBG(5, "%s: called\n", __func__);

	for (i = 0; i < NUM_OPTIONS; i++) {
		s->opt[i].size = sizeof(SANE_Word);
		s->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	}

	s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
	s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
	s->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
	s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
	s->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

	/* "Scan Mode" group: */

	s->opt[OPT_MODE_GROUP].name = SANE_NAME_STANDARD;
	s->opt[OPT_MODE_GROUP].title = SANE_TITLE_STANDARD;
	s->opt[OPT_MODE_GROUP].desc = SANE_DESC_STANDARD;
	s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
	s->opt[OPT_MODE_GROUP].cap = 0;

	/* scan mode */
	s->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
	s->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
	s->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
	s->opt[OPT_MODE].type = SANE_TYPE_STRING;
	s->opt[OPT_MODE].size = max_string_size(mode_list);
	s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_MODE].constraint.string_list = mode_list;
	s->val[OPT_MODE].w = MODE_COLOR;	/* default */
	DBG(20, "%s: mode_list has first entry %s, default mode is %s\n", __func__, mode_list[0],mode_list[s->val[OPT_MODE].w]);

	/* theshold the sane std says should be SANE_TYPE_FIXED 0..100 but all other backends seem to use INT 0..255 */
	s->opt[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
	s->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
	s->opt[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
	s->opt[OPT_THRESHOLD].type = SANE_TYPE_FIXED;
	s->opt[OPT_THRESHOLD].unit = SANE_UNIT_PERCENT;
	s->opt[OPT_THRESHOLD].size = sizeof(SANE_Word);
	s->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_THRESHOLD].constraint.range = &percent_range_fixed;
	s->val[OPT_THRESHOLD].w = SANE_FIX(50.0);
	DBG(20, "%s: threshold initialised to fixed %f\n", __func__, SANE_UNFIX(s->val[OPT_THRESHOLD].w));

	/* theshold the sane std says should be SANE_TYPE_FIXED 0..100 but all other backends seem to use INT 0..255
	s->opt[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
	s->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
	s->opt[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
	s->opt[OPT_THRESHOLD].type = SANE_TYPE_INT;
	s->opt[OPT_THRESHOLD].unit = SANE_UNIT_PERCENT;
	s->opt[OPT_THRESHOLD].size = sizeof(SANE_Word);
	s->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_THRESHOLD].constraint.range = &percent_range_int;
	s->val[OPT_THRESHOLD].w = 51;
	DBG(20, "%s: threshold initialised to int %d\n", __func__, s->val[OPT_THRESHOLD].w); */

	/* bit depth */
	s->opt[OPT_BIT_DEPTH].name = SANE_NAME_BIT_DEPTH;
	s->opt[OPT_BIT_DEPTH].title = SANE_TITLE_BIT_DEPTH;
	s->opt[OPT_BIT_DEPTH].desc = SANE_DESC_BIT_DEPTH;
	s->opt[OPT_BIT_DEPTH].type = SANE_TYPE_INT;
	s->opt[OPT_BIT_DEPTH].unit = SANE_UNIT_NONE;
	s->opt[OPT_BIT_DEPTH].constraint_type = SANE_CONSTRAINT_WORD_LIST;
	s->opt[OPT_BIT_DEPTH].constraint.word_list = s->hw->cap->depth_list;
	s->opt[OPT_BIT_DEPTH].cap |= SANE_CAP_INACTIVE;
	s->val[OPT_BIT_DEPTH].w = s->hw->cap->depth_list[1];	/* the first "real" element is the default */
	
	DBG(20, "%s: depth list has depth_list[0] = %d entries\n", __func__, s->hw->cap->depth_list[0]);
	if (s->hw->cap->depth_list[0] == 1) {	/* only one element in the list -> hide the option */
		s->opt[OPT_BIT_DEPTH].cap |= SANE_CAP_INACTIVE;
		DBG(20, "%s: Only one depth in list so inactive option\n", __func__);
	}

	/* resolution */
	s->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
	s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
	s->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
	s->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
	s->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
	s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
	res_list = malloc((s->hw->cap->res_list_size + 1) * sizeof(SANE_Word));
	if (res_list == NULL) {
		return SANE_STATUS_NO_MEM;
	}
	*(res_list) = s->hw->cap->res_list_size;
	memcpy(&(res_list[1]), s->hw->cap->res_list, s->hw->cap->res_list_size * sizeof(SANE_Word));
	s->opt[OPT_RESOLUTION].constraint.word_list = res_list;
	s->val[OPT_RESOLUTION].w = s->hw->cap->dpi_range.min;


	/* trial option for debugging 
	s->opt[OPT_TRIALOPT].name = "trialoption";
	s->opt[OPT_TRIALOPT].title = "trialoption";
	s->opt[OPT_TRIALOPT].desc = "trialoption";
	s->opt[OPT_TRIALOPT].type = SANE_TYPE_INT;
	s->opt[OPT_TRIALOPT].unit = SANE_UNIT_NONE;
	s->opt[OPT_TRIALOPT].size = sizeof(SANE_Word);
	s->opt[OPT_TRIALOPT].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_TRIALOPT].constraint.range = &percent_range_int;
	s->val[OPT_TRIALOPT].w = 1; */

	/* preview */
	s->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
	s->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
	s->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
	s->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;
	s->val[OPT_PREVIEW].w = SANE_FALSE;

	for(i=0;source_list[i]!=NULL;++i)
		DBG(18, "source_list: %s\n",source_list[i]);
		
	/* source */
	s->opt[OPT_SOURCE].name = SANE_NAME_SCAN_SOURCE;
	s->opt[OPT_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
	s->opt[OPT_SOURCE].desc = SANE_DESC_SCAN_SOURCE;
	s->opt[OPT_SOURCE].type = SANE_TYPE_STRING;
	s->opt[OPT_SOURCE].size = max_string_size(source_list);
	s->opt[OPT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_SOURCE].constraint.string_list = source_list;
	s->val[OPT_SOURCE].w = 0;	/* always use Flatbed as default */

	if ((!s->hw->cap->ADF)) {
		DBG(9, "device with no adf detected source option inactive\n");
		s->opt[OPT_SOURCE].cap |= SANE_CAP_INACTIVE;
	}

/* Are there any ESP scanners that are duplex? */	
	s->opt[OPT_ADF_MODE].name = "adf-mode";
	s->opt[OPT_ADF_MODE].title = SANE_I18N("ADF Mode");
	s->opt[OPT_ADF_MODE].desc =
	SANE_I18N("Selects the ADF mode (simplex/duplex)");
	s->opt[OPT_ADF_MODE].type = SANE_TYPE_STRING;
	s->opt[OPT_ADF_MODE].size = max_string_size(adf_mode_list);
	s->opt[OPT_ADF_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_ADF_MODE].constraint.string_list = adf_mode_list;
	s->val[OPT_ADF_MODE].w = 0;	/* simplex */
	if ((!s->hw->cap->ADF) || (!s->hw->cap->adf_duplex))
		s->opt[OPT_ADF_MODE].cap |= SANE_CAP_INACTIVE;


	/* "Geometry" group: */
	s->opt[OPT_GEOMETRY_GROUP].name = SANE_NAME_GEOMETRY;
	s->opt[OPT_GEOMETRY_GROUP].title = SANE_TITLE_GEOMETRY;
	s->opt[OPT_GEOMETRY_GROUP].desc = SANE_DESC_GEOMETRY;
	s->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
	s->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;

	/* top-left x */
	s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
	s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
	s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
	s->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
	s->opt[OPT_TL_X].unit = SANE_UNIT_MM;
	s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_TL_X].constraint.range = s->hw->x_range;
	s->val[OPT_TL_X].w = 0;

	/* top-left y */
	s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
	s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
	s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
	s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
	s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
	s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_TL_Y].constraint.range = s->hw->y_range;
	s->val[OPT_TL_Y].w = 0;

	/* bottom-right x */
	s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
	s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
	s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
	s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
	s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
	s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_BR_X].constraint.range = s->hw->x_range;
	s->val[OPT_BR_X].w = s->hw->x_range->max;

	/* bottom-right y */
	s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
	s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
	s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
	s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
	s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
	s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_BR_Y].constraint.range = s->hw->y_range;
	s->val[OPT_BR_Y].w = s->hw->y_range->max;

	/* padding short pages in adf */
	s->opt[OPT_PADDING].name = "adf-padding";
	s->opt[OPT_PADDING].title = "pad short adf pages";
	s->opt[OPT_PADDING].desc = "Selects whether to make short pages up to full length";
	s->opt[OPT_PADDING].type = SANE_TYPE_BOOL;
	s->val[OPT_PADDING].w = SANE_FALSE;
	if ((!s->hw->cap->ADF) || (strcmp(source_list[s->val[OPT_SOURCE].w], ADF_STR) != 0))
	{
		DBG(9, "adf not source so padding option off and inactive\n");
		s->opt[OPT_PADDING].cap |= SANE_CAP_INACTIVE;
	}

	return SANE_STATUS_GOOD;
}

SANE_Status
sane_open(SANE_String_Const name, SANE_Handle *handle)
{
	SANE_Status status;
	KodakAio_Scanner *s = NULL;

	int l = strlen(name);

	DBG(2, "%s: name = %s\n", __func__, name);

	/* probe if empty device name provided */
	if (l == 0) {

		status = sane_get_devices(NULL,0);
		if (status != SANE_STATUS_GOOD) {
			return status;
		}

		if (first_dev == NULL) {
			DBG(1, "no device detected\n");
			return SANE_STATUS_INVAL;
		}

		s = device_detect(first_dev->sane.name, first_dev->connection,
					&status);
		if (s == NULL) {
			DBG(1, "cannot open a perfectly valid device (%s),"
				" please report to the authors\n", name);
			return SANE_STATUS_INVAL;
		}

	} else {

		if (strncmp(name, "net:", 4) == 0) {
			s = device_detect(name, SANE_KODAKAIO_NET, &status);
			if (s == NULL)
				return status;
		} else if (strncmp(name, "libusb:", 7) == 0) {
			s = device_detect(name, SANE_KODAKAIO_USB, &status);
			if (s == NULL)
				return status;
		} else {

			/* as a last resort, check for a match
			 * in the device list. This should handle platforms without libusb.
			 */
			if (first_dev == NULL) {
				status = sane_get_devices(NULL,0);
				if (status != SANE_STATUS_GOOD) {
					return status;
				}
			}

			s = device_detect(name, SANE_KODAKAIO_NODEV, &status);
			if (s == NULL) {
				DBG(1, "invalid device name: %s\n", name);
				return SANE_STATUS_INVAL;
			}
		}
	}


	/* s is always valid here */

	DBG(10, "handle obtained\n");
	status = k_discover_capabilities(s); /* added 27/12/11 to fix source list problem 
maybe we should only be rebuilding the source list here? */
	if (status != SANE_STATUS_GOOD)
		return status;

	init_options(s);

	*handle = (SANE_Handle) s;

/* moving the open scanner section below to sane_start 27/12/14 
	status = open_scanner(s);
	if (status != SANE_STATUS_GOOD) {
		free(s);
		return status;
	}
*/
	return status;
}

void
sane_close(SANE_Handle handle)
{
	KodakAio_Scanner *s;

	/*
	 * XXX Test if there is still data pending from
	 * the scanner. If so, then do a cancel
	 */

	s = (KodakAio_Scanner *) handle;
	DBG(2, "%s: called\n", __func__);

/* moving the close scanner section below to sane_cancel 27/12/14 */
	if (s->fd != -1)
		close_scanner(s);
/* end of section */

	if(RawScan != NULL)
		fclose(RawScan);
	RawScan = NULL;
	free(s);
}

const SANE_Option_Descriptor *
sane_get_option_descriptor(SANE_Handle handle, SANE_Int option)
{
/* this may be a sane call, but it happens way too often to have DBG level 2 */
	KodakAio_Scanner *s = (KodakAio_Scanner *) handle;

	DBG(30, "%s: called for option %d\n", __func__, option);

	if (option < 0 || option >= NUM_OPTIONS)
		return NULL;

	return s->opt + option;
}

static const SANE_String_Const *
search_string_list(const SANE_String_Const *list, SANE_String value)
{
	while (*list != NULL && strcmp(value, *list) != 0)
		list++;

	return ((*list == NULL) ? NULL : list);
}

/*
    Activate, deactivate an option. Subroutines so we can add
    debugging info if we want. The change flag is set to TRUE
    if we changed an option. If we did not change an option,
    then the value of the changed flag is not modified.
*/

static void
activateOption(KodakAio_Scanner *s, SANE_Int option, SANE_Bool *change)
{
	if (!SANE_OPTION_IS_ACTIVE(s->opt[option].cap)) {
		s->opt[option].cap &= ~SANE_CAP_INACTIVE;
		*change = SANE_TRUE;
	}
}

static void
deactivateOption(KodakAio_Scanner *s, SANE_Int option, SANE_Bool *change)
{
	if (SANE_OPTION_IS_ACTIVE(s->opt[option].cap)) {
		s->opt[option].cap |= SANE_CAP_INACTIVE;
		*change = SANE_TRUE;
	}
}

static SANE_Status
getvalue(SANE_Handle handle, SANE_Int option, void *value)
{
	KodakAio_Scanner *s = (KodakAio_Scanner *) handle;
	SANE_Option_Descriptor *sopt = &(s->opt[option]);
	Option_Value *sval = &(s->val[option]);

	DBG(17, "%s: option = %d\n", __func__, option);

	switch (option) {

	case OPT_NUM_OPTS:
	case OPT_BIT_DEPTH:
	/* case OPT_TRIALOPT: */
	case OPT_RESOLUTION:
	case OPT_PREVIEW:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
		*((SANE_Word *) value) = sval->w;
		DBG(20, "%s: got option %d as %d\n", __func__, option, *((SANE_Word *) value));
		break;

	case OPT_THRESHOLD:
		*((SANE_Word *) value) = sval->w;
		DBG(20, "%s: got option %d as %f\n", __func__, option, SANE_UNFIX(*((SANE_Word *) value)));
		/*DBG(20, "%s: got option %d as %d\n", __func__, option, *((SANE_Word *) value));*/
		break;

	case OPT_MODE:
	case OPT_SOURCE:
	case OPT_ADF_MODE:
		strcpy((char *) value, sopt->constraint.string_list[sval->w]);
		break;
	case OPT_PADDING:
		*((SANE_Bool *) value) = sval->w;
		break;

	default:
		DBG(20, "%s: returning inval\n", __func__);
		return SANE_STATUS_INVAL;
	}

	DBG(20, "%s: returning good\n", __func__);
	return SANE_STATUS_GOOD;
}


/*
 * Handles setting the source (flatbed, or auto document feeder (ADF)).
 *
 */

static void
change_source(KodakAio_Scanner *s, SANE_Int optindex, char *value)
{
	int force_max = SANE_FALSE;
	SANE_Bool dummy;

	DBG(5, "%s: optindex = %d, source = '%s'\n", __func__, optindex,
	    value);

	if (s->val[OPT_SOURCE].w == optindex)
		return;

	s->val[OPT_SOURCE].w = optindex;

	if (s->val[OPT_TL_X].w == s->hw->x_range->min
	    && s->val[OPT_TL_Y].w == s->hw->y_range->min
	    && s->val[OPT_BR_X].w == s->hw->x_range->max
	    && s->val[OPT_BR_Y].w == s->hw->y_range->max) {
		force_max = SANE_TRUE;
	}

	if (strcmp(ADF_STR, value) == 0) {
		s->hw->x_range = &s->hw->cap->adf_x_range;
		s->hw->y_range = &s->hw->cap->adf_y_range;
		if (s->hw->cap->adf_duplex) {
			activateOption(s, OPT_ADF_MODE, &dummy);
		} else {
			deactivateOption(s, OPT_ADF_MODE, &dummy);
			s->val[OPT_ADF_MODE].w = 0;
		}
		activateOption(s, OPT_PADDING, &dummy);

		DBG(5, "adf activated flag = %d\n",s->hw->cap->adf_duplex);

	} else {
		/* ADF not active */
		s->hw->x_range = &s->hw->cap->fbf_x_range;
		s->hw->y_range = &s->hw->cap->fbf_y_range;

		deactivateOption(s, OPT_ADF_MODE, &dummy);
		deactivateOption(s, OPT_PADDING, &dummy);
	}

	s->opt[OPT_BR_X].constraint.range = s->hw->x_range;
	s->opt[OPT_BR_Y].constraint.range = s->hw->y_range;

	if (s->val[OPT_TL_X].w < s->hw->x_range->min || force_max)
		s->val[OPT_TL_X].w = s->hw->x_range->min;

	if (s->val[OPT_TL_Y].w < s->hw->y_range->min || force_max)
		s->val[OPT_TL_Y].w = s->hw->y_range->min;

	if (s->val[OPT_BR_X].w > s->hw->x_range->max || force_max)
		s->val[OPT_BR_X].w = s->hw->x_range->max;

	if (s->val[OPT_BR_Y].w > s->hw->y_range->max || force_max)
		s->val[OPT_BR_Y].w = s->hw->y_range->max;

}

static SANE_Status
setvalue(SANE_Handle handle, SANE_Int option, void *value, SANE_Int *info)
{
	KodakAio_Scanner *s = (KodakAio_Scanner *) handle;
	SANE_Option_Descriptor *sopt = &(s->opt[option]);
	Option_Value *sval = &(s->val[option]);

	SANE_Status status;
	const SANE_String_Const *optval = NULL;
	int optindex = 0;
	SANE_Bool reload = SANE_FALSE;

	DBG(17, "%s: option = %d, value = %p, as word: %d\n", __func__, option, value, *(SANE_Word *) value);

	status = sanei_constrain_value(sopt, value, info);
	if (status != SANE_STATUS_GOOD)
		return status;

	if (info && value && (*info & SANE_INFO_INEXACT)
	    && sopt->type == SANE_TYPE_INT)
		DBG(17, "%s: constrained val = %d\n", __func__,
		    *(SANE_Word *) value);

	if (sopt->constraint_type == SANE_CONSTRAINT_STRING_LIST) {
		optval = search_string_list(sopt->constraint.string_list,
					    (char *) value);
		if (optval == NULL)
			return SANE_STATUS_INVAL;
		optindex = optval - sopt->constraint.string_list;
	}

	switch (option) {

	case OPT_MODE:
	{
		sval->w = optindex;
		/* if binary, then disable the bit depth selection and enable threshold */
		if (optindex == MODE_LINEART) {
	DBG(17, "%s: binary mode setting depth to 1\n", __func__);
			s->val[OPT_BIT_DEPTH].w = 1;
			s->opt[OPT_BIT_DEPTH].cap |= SANE_CAP_INACTIVE;
			s->opt[OPT_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;
		} else {
			if (s->hw->cap->depth_list[0] == 1) { /* only one entry in the list ? */
	DBG(17, "%s: non-binary mode but only one depth available\n", __func__);
				s->val[OPT_BIT_DEPTH].w = s->hw->cap->depth_list[1];
				s->opt[OPT_BIT_DEPTH].cap |= SANE_CAP_INACTIVE;
				s->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
			} else { /* there is a list to choose from ? */
	DBG(17, "%s: non-binary mode and depth list available\n", __func__);
				s->opt[OPT_BIT_DEPTH].cap &= ~SANE_CAP_INACTIVE;
				s->val[OPT_BIT_DEPTH].w = mode_params[optindex].depth;
				s->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE; /* does not work in xsane ? */
			}
		}
		reload = SANE_TRUE;
		break;
	}

	case OPT_BIT_DEPTH:
		sval->w = *((SANE_Word *) value);
		mode_params[s->val[OPT_MODE].w].depth = sval->w;
		reload = SANE_TRUE;
		break;

	case OPT_THRESHOLD:
		sval->w = *((SANE_Word *) value);
		DBG(17, "setting threshold to %f\n", SANE_UNFIX(sval->w));
		/*DBG(17, "setting threshold to %d\n", sval->w);*/
		/*reload = SANE_TRUE; what does this do?*/
		break;

	case OPT_RESOLUTION:
		sval->w = *((SANE_Word *) value);
		DBG(17, "setting resolution to %d\n", sval->w);
		reload = SANE_TRUE;
		break;

	case OPT_BR_X:
	case OPT_BR_Y:
		sval->w = *((SANE_Word *) value);
		if (SANE_UNFIX(sval->w) == 0) {
			DBG(17, "invalid br-x or br-y\n");
			return SANE_STATUS_INVAL;
		}
		/* passthru */
	case OPT_TL_X:
	case OPT_TL_Y:
		sval->w = *((SANE_Word *) value);
		DBG(17, "setting size to %f\n", SANE_UNFIX(sval->w));
		if (NULL != info)
			*info |= SANE_INFO_RELOAD_PARAMS;
		break;

	case OPT_SOURCE:
		change_source(s, optindex, (char *) value);
		reload = SANE_TRUE;
		break;

	case OPT_ADF_MODE:
		sval->w = optindex;	/* Simple lists */
		break;

	case OPT_PADDING:
		sval->w = *((SANE_Word *) value);
		break;

	/* case OPT_TRIALOPT: */
	case OPT_PREVIEW:	/* needed? */
		sval->w = *((SANE_Word *) value);
		break;

	default:
		return SANE_STATUS_INVAL;
	}

	if (reload && info != NULL)
		*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;

	DBG(17, "%s: end\n", __func__);

	return SANE_STATUS_GOOD;
}

SANE_Status
sane_control_option(SANE_Handle handle, SANE_Int option, SANE_Action action,
		    void *value, SANE_Int *info)
{
	KodakAio_Scanner *s = (KodakAio_Scanner *) handle;
	if (option < 0 || option >= NUM_OPTIONS)
	{
		DBG(1, "%s: option num = %d out of range (0..%d)\n", __func__, option, NUM_OPTIONS - 1);
		return SANE_STATUS_INVAL;
	}

	DBG(5, "%s: action = %x, option = %d %s\n", __func__, action, option, s->opt[option].name);

	if (info != NULL)
		*info = 0;

	switch (action) {
	case SANE_ACTION_GET_VALUE:
		return getvalue(handle, option, value);

	case SANE_ACTION_SET_VALUE:
		return setvalue(handle, option, value, info);

	default:
		return SANE_STATUS_INVAL;
	}

	return SANE_STATUS_INVAL;
}

SANE_Status
sane_get_parameters(SANE_Handle handle, SANE_Parameters *params)
{
	KodakAio_Scanner *s = (KodakAio_Scanner *) handle;

	DBG(2, "%s: called\n", __func__);

	if (params == NULL)
		DBG(1, "%s: params is NULL\n", __func__);

	/*
	 * If sane_start was already called, then just retrieve the parameters
	 * from the scanner data structure
	 */

	if (!s->eof && s->ptr != NULL) {
		DBG(5, "scan in progress, returning saved params structure\n");
	} else {
		/* otherwise initialize the params structure and gather the data */
		k_init_parametersta(s);
	}

	if (params != NULL)
		*params = s->params;

	print_params(s->params,20);

	return SANE_STATUS_GOOD;
}

/*
 * This function is part of the SANE API and gets called from the front end to
 * start the scan process.
 */

SANE_Status
sane_start(SANE_Handle handle)
{
	KodakAio_Scanner *s = (KodakAio_Scanner *) handle;
	SANE_Status status;

	DBG(2, "%s: called\n", __func__);
	if(! s->scanning) {
		/* calc scanning parameters */
		status = k_init_parametersta(s);
		if (status != SANE_STATUS_GOOD)
			return status;

	/* set scanning parameters; also query the current image
	 * parameters from the sanner and save
	 * them to s->params 
Only set scanning params the first time, or after a cancel 
try change 22/2/12 take lock scanner out of k_set_scanning_parameters */

/* moved open_scanner here 27/12/14 from sane_open */
	status = open_scanner(s);
	if (status != SANE_STATUS_GOOD) {
		free(s);
		return status;
	}
/* end of open scanner section */


		status = k_lock_scanner(s);
		if (status != SANE_STATUS_GOOD) {
			DBG(1, "could not lock scanner\n");
			return status;
		}

	}

	status = k_set_scanning_parameters(s);
	if (status != SANE_STATUS_GOOD)
		return status;

	print_params(s->params, 5);
	/* if we scan from ADF, check if it is loaded */
	if (strcmp(source_list[s->val[OPT_SOURCE].w], ADF_STR) == 0) {
		status = k_check_adf(s);
		if (status != SANE_STATUS_GOOD) {
/* returning SANE_STATUS_NO_DOCS seems not to cause simple-scan to end the adf scan, so we cancel */
			status = SANE_STATUS_CANCELLED;
			DBG(10, "%s: returning %s\n", __func__, sane_strstatus(status));
			return status;
		}
	}

	/* prepare buffer here so that a memory allocation failure
	 * will leave the scanner in a sane state.
	 */
	s->buf = realloc(s->buf, s->block_len);
	if (s->buf == NULL)
		return SANE_STATUS_NO_MEM;

	s->eof = SANE_FALSE; /* page not finished */
	s->ack = SANE_FALSE; /* page from scanner not finished */
	s->ptr = s->end = s->buf;
	s->canceling = SANE_FALSE;

	if (strlen(RawScanPath) > 0 && s->params.lines > 0)
		RawScan = fopen(RawScanPath, "wb");/* open the debug file if it has a name */
	if(RawScan) fprintf(RawScan, "P5\n%d %d\n%d\n",s->scan_bytes_per_line, s->params.lines, 255);

	/* start scanning */
	DBG(2, "%s: scanning...\n", __func__);
	status = k_start_scan(s);

	if (status != SANE_STATUS_GOOD) {
		DBG(1, "%s: start failed: %s\n", __func__,
		    sane_strstatus(status));

		return status;
	}

	return status;
}

/* this moves data from our buffers to SANE */

SANE_Status
sane_read(SANE_Handle handle, SANE_Byte *data, SANE_Int max_length,
	  SANE_Int *length)
{
	SANE_Status status;
	KodakAio_Scanner *s = (KodakAio_Scanner *) handle;

	if (s->buf == NULL || s->canceling)
		return SANE_STATUS_CANCELLED;

	*length = 0;
	DBG(18, "sane-read, bytes unread %d\n",s->bytes_unread);

	status = k_read(s);

	if (status == SANE_STATUS_CANCELLED) {
		k_scan_finish(s);
		return status;
	}

	k_copy_image_data(s, data, max_length, length);

	DBG(18, "%d lines read, status: %s\n",
		*length / s->params.bytes_per_line, sane_strstatus(status));

	/* continue reading if appropriate */
	if (status == SANE_STATUS_GOOD)
		return status;

/* not sure if we want to finish (unlock scanner) here or in sane_close */
	/* k_scan_finish(s); */
	return status;
}


void
sane_cancel(SANE_Handle handle)
{
	SANE_Status status;
	KodakAio_Scanner *s = (KodakAio_Scanner *) handle;
	DBG(2, "%s: called\n", __func__);

	status = cmd_cancel_scan(s);
	if (status != SANE_STATUS_GOOD)
		DBG(1, "%s: cmd_cancel_scan failed: %s\n", __func__,
		    sane_strstatus(status));

/* moved from close scanner section 27/12/14 */
	if (s->fd != -1)
		close_scanner(s);
/* end of section */

}

/*
 * SANE_Status sane_set_io_mode()
 *
 * not supported - for asynchronous I/O
 */

SANE_Status
sane_set_io_mode(SANE_Handle __sane_unused__ handle,
	SANE_Bool __sane_unused__ non_blocking)
{
	return SANE_STATUS_UNSUPPORTED;
}

/*
 * SANE_Status sane_get_select_fd()
 *
 * not supported - for asynchronous I/O
 */

SANE_Status
sane_get_select_fd(SANE_Handle __sane_unused__ handle,
	SANE_Int __sane_unused__ *fd)
{
	return SANE_STATUS_UNSUPPORTED;
}
