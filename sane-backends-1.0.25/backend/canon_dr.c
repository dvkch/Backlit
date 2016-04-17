/* sane - Scanner Access Now Easy.

   This file is part of the SANE package, and implements a SANE backend
   for various Canon DR-series scanners.

   Copyright (C) 2008-2010 m. allan noah

   Yabarana Corp. www.yabarana.com provided significant funding
   EvriChart, Inc. www.evrichart.com provided funding and loaned equipment
   Canon, USA. www.usa.canon.com loaned equipment
   HPrint hprint.com.br provided funding and testing for DR-2510 support
   Stone-IT www.stone-it.com provided funding for DR-2010 and DR-2050 support

   --------------------------------------------------------------------------

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

   --------------------------------------------------------------------------

   The source code is divided in sections which you can easily find by
   searching for the tag "@@".

   Section 1 - Init & static stuff
   Section 2 - sane_init, _get_devices, _open & friends
   Section 3 - sane_*_option functions
   Section 4 - sane_start, _get_param, _read & friends
   Section 5 - calibration functions
   Section 6 - sane_close functions
   Section 7 - misc functions
   Section 8 - image processing functions

   Changes:
      v1 2008-10-29, MAN
         - initial version
      v2 2008-11-04, MAN
         - round scanlines to even bytes
	 - spin RS and usb_clear_halt code into new function
	 - update various scsi payloads
	 - calloc out block so it gets set to 0 initially
      v3 2008-11-07, MAN
         - back window uses id 1
         - add option and functions to read/send page counter
         - add rif option
      v4 2008-11-11, MAN
         - eject document when sane_read() returns EOF
      v5 2008-11-25, MAN
         - remove EOF ejection code
         - add SSM and GSM commands
         - add dropout, doublefeed, and jpeg compression options
         - disable adf backside
         - fix adf duplex
         - read two extra lines (ignore errors) at end of image
         - only send scan command at beginning of batch
	 - fix bug in hexdump with 0 length string
         - DR-7580 support
      v6 2008-11-29, MAN
         - fix adf simplex
         - rename ssm_duplex to ssm_buffer
         - add --buffer option
         - reduce inter-page commands when buffering is enabled
         - improve sense_handler output
         - enable counter option
         - drop unused code
      v7 2008-11-29, MAN
         - jpeg support (size rounding and header overwrite)
         - call object_position(load) between pages even if buffering is on
         - use request sense info bytes on short scsi reads
         - byte swap color BGR to RGB
         - round image width down, not up
         - round image height down to even # of lines
         - always transfer even # of lines per block
         - scsi and jpeg don't require reading extra lines to reach EOF
         - rename buffer option to buffermode to avoid conflict with scanimage
         - send ssm_do and ssm_df during sane_start
         - improve sense_handler output
      v8 2008-12-07, MAN
         - rename read/send_counter to read/send_panel
         - enable control panel during init
         - add options for all buttons
         - call TUR twice in wait_scanner(), even if first succeeds
         - disable rif
         - enable brightness/contrast/threshold options
      v9 2008-12-07, MAN
         - add rollerdeskew and stapledetect options
         - add rollerdeskew and stapledetect bits to ssm_df()
      v10 2008-12-10, MAN
         - add all documented request sense codes to sense_handler()
         - fix color jpeg (remove unneeded BGR to RGB swapping code)
         - add macros for LUT data
      v11 2009-01-10, MAN
         - send_panel() can disable too
         - add cancel() to send d8 command
         - call cancel() only after final read from scanner
         - stop button reqests cancel
      v12 2009-01-21, MAN
         - dont export private symbols
      v13 2009-03-06, MAN
         - new vendor ID for recent machines
         - add usb ids for several new machines
      v14 2009-03-07, MAN
         - remove HARD_SELECT from counter (Legitimate, but API violation)
         - attach to CR-series scanners as well
      v15 2009-03-15, MAN
         - add byte-oriented duplex interlace code
         - add RRGGBB color interlace code
         - add basic support for DR-2580C
      v16 2009-03-20, MAN
         - add more unknown setwindow bits
         - add support for 16 byte status packets
         - clean do_usb_cmd error handling (call reset more often)
         - add basic support for DR-2050C, DR-2080C, DR-2510C
      v17 2009-03-20, MAN
         - set status packet size from config file
      v18 2009-03-21, MAN
         - rewrite config file parsing to reset options after each scanner
         - add config options for vendor, model, version
         - dont call inquiry if those 3 options are set
         - remove default config file from code
         - add initial gray deinterlacing code for DR-2510C
         - rename do_usb_reset to do_usb_clear
      v19 2009-03-22, MAN
         - pad gray deinterlacing area for DR-2510C
         - override tl_x and br_x for fixed width scanners
      v20 2009-03-23, MAN
         - improved macros for inquiry and set window
         - shorten inquiry vpd length to match windows driver
         - remove status-length config option
         - add padded-read config option
         - rewrite do_usb_cmd to pad reads and calloc/copy buffers
      v21 2009-03-24, MAN
         - correct rgb padding macro
         - skip send_panel and ssm_df commands for DR-20xx scanners
      v22 2009-03-25, MAN
         - add deinterlacing code for DR-2510C in duplex and color 
      v23 2009-03-27, MAN
         - rewrite all image data processing code
         - handle more image interlacing formats
         - re-enable binary mode on some scanners
         - limit some machines to full-width scanning
      v24 2009-04-02, MAN
         - fix DR-2510C duplex deinterlacing code
         - rewrite sane_read helpers to read until EOF
         - update sane_start for scanners that dont use object_position
         - dont call sanei_usb_clear_halt() if device is not open
         - increase default buffer size to 4 megs
         - set buffermode on by default
         - hide modes and resolutions that DR-2510C lies about
         - read_panel() logs front-end access to sensors instead of timing
         - rewrite do_usb_cmd() to use remainder from RS info
      v25 2009-04-12, MAN
         - disable SANE_FRAME_JPEG
      v26 2009-04-14, MAN (SANE 1.0.20)
         - return cmd status for reads on sensors
         - allow rs to adjust read length for all bad status responses
      v27 2009-05-08, MAN
         - bug fix in read_panel()
         - initialize vars in do_usb_cmd()
         - set buffermode off by default
         - clear page counter during init and sane_start()
         - eject previous page during init and sane_start()
         - improved SSM_BUFF macros
         - moved set_window() to after ssm-*()
         - add coarse calibration (AFE offset/gain & per-channel exposure)
         - add fine calibration (per-cell offset/gain)
         - free image and fine cal buffers in sane_close()
         - compare page counter of small scanners only in non-buffered mode
         - add back-side gray mirroring code for DR-2580C
      v28 2009-05-20, MAN
         - use average instead of min/max for fine offset and gain
         - rewrite supported resolution list as x and y arrays
         - merge x and y resolution options into single option
         - move scan params into two new structs, s->u and s->s
         - sane_get_parameters() just returns values from s->u
         - dont call wait_scanner() in object_position()
         - dont call ssm_*() from option handler
         - refactor sane_start()
         - read_from_buffer() can workaround missing res, modes and cropping
         - set most DR-2xxx machines to use the read_from_buffer workarounds
         - set default threshold to 90
         - add option for button #3 of some machines
         - don't eject paper during init
         - add DR-2010 quirks
         - switch counter to HARD_SELECT, not SOFT
      v29 2009-06-01, MAN
         - split coarse and fine cal to run independently
         - add side option
         - reset scan params to user request if calibration fails
         - better handling of sane_cancel
         - better handling of errors during sane_start and sane_read
      v30 2009-06-17, MAN
         - add fine cal support for machines with internal buffer (2050/2080)
         - support fixed-width machines that require even bytes per scanline
         - pad end of scan with gray if scanner stops prematurely
         - better handling of errors during calibration
         - cleanup canceling debug messages
         - remove old cancel() prototype
         - small sleep before clearing usb halt condition
      v31 2009-06-29, MAN
         - reduce default buffer size to 2 megs
      v32 2009-07-21, MAN
         - crop/resample image data before buffering, not after
         - shink image buffers to size of output image, not input
         - correct some debug message
         - better handling of EOF
         - add intermediate param struct to existing user and scan versions
      v33 2009-07-23, MAN
         - add software brightness/contrast for dumb scanners
         - add blocking mode to allow full-page manipulation options to run
         - add swdespeck option and support code
         - add swdeskew and swcrop options (disabled)
      v34 2009-07-28, MAN
         - add simplified Hough transform based deskewing code
         - add extremity detecting cropping code
         - use per-model background color to fill corners after deskew
         - request and chop extra scanlines instead of rounding down
         - remove padding dumb scanners add to top of front side
         - sane_get_params uses intermediate struct instead of user struct
         - if scanner stops, clone the last line until the end of buffer
         - reset some intermediate params between duplex sides
      v35 2010-02-09, MAN (SANE 1.0.21)
         - cleanup #includes and copyright
         - add SANE_I18N to static strings
         - don't fail if scsi buffer is too small
      v36 2011-01-03, MAN
         - initial support for DR-3080 and DR-5060
         - add code to clamp scan width to an arbitrary byte width boundary
         - add code to prevent setting of brightness/threshold/contrast
         - don't send dropout color command on non-color scanners
         - initial support for DR-7090C
         - update credits
      v37 2011-01-26, MAN (SANE 1.0.22)
         - don't center window when using flatbed
         - improve request sense error messages
         - enable flatbed for all known models
      v38 2011-07-06, MAN
	 - initial support for DR-5020
	 - use ppl_mod instead of Bpl_mod, apply to all modes
	 - invert logic of read_panel tracking
	 - add ability to disable read_panel()
	 - automatically disable read/send_panel if unsupported
      v39 2011-11-01, MAN
         - DR-2580C pads the backside of duplex scans
      v40 2012-11-01, MAN
         - initial DR-9050C, DR-7550C, DR-6050C and DR-3010C support
      v41 2013-07-31, MAN (SANE 1.0.24)
         - initial P-208 and P-215 support
         - bug fix for calibration of scanners with duplex_offset
         - allow duplex_offset to be controlled from config file
      v42 2013-12-09, MAN
         - initial DR-G1100 support
         - add support for paper sensors (P-215 & P-208)
         - add initial support for card reader (P-215)
         - removed unused var from do_scsi_cmd()
      v43 2014-03-13, MAN
         - initial DR-M140 support
         - add extra_status config and code
         - split status code into do_usb_status
         - fix copy_line margin offset
         - add new color interlacing modes and code
         - comment out ssm2
         - add timestamp to do_usb_cmd
      v44 2014-03-26, MAN
         - buffermode support for machines with ssm2 command
         - DR-M140 needs always_op=0
      v45 2014-03-29, MAN
         - dropout support for machines with ssm2 command
         - doublefeed support for machines with ssm2 command
      v46 2014-04-09, MAN
         - split debug level 30 into two levels
         - simplify jpeg ifdefs
         - add support for DR-M160
      v47 2014-07-07, MAN
         - initial DR-G1130 support
      v48 2014-08-06, MAN
         - set another unknown byte in buffermode for ssm2
         - add another gettimeofday call at end of do_usb_cmd
         - don't print 0 length line in hexdump
      v49 2015-03-18, MAN
         - initial support for DR-C125
      v50 2015-08-23, MAN
         - DR-C125 adds duplex padding on back side
         - initial support for DR-C225
      v51 2015-08-25, MAN
         - DR-C125 does not invert_tly, does need sw_lut

   SANE FLOW DIAGRAM

   - sane_init() : initialize backend
   . - sane_get_devices() : query list of scanner devices
   . - sane_open() : open a particular scanner device
   . . - sane_set_io_mode : set blocking mode
   . . - sane_get_select_fd : get scanner fd
   . .
   . . - sane_get_option_descriptor() : get option information
   . . - sane_control_option() : change option values
   . . - sane_get_parameters() : returns estimated scan parameters
   . . - (repeat previous 3 functions)
   . .
   . . - sane_start() : start image acquisition
   . .   - sane_get_parameters() : returns actual scan parameters
   . .   - sane_read() : read image data (from pipe)
   . . (sane_read called multiple times; after sane_read returns EOF, 
   . . loop may continue with sane_start which may return a 2nd page
   . . when doing duplex scans, or load the next page from the ADF)
   . .
   . . - sane_cancel() : cancel operation
   . - sane_close() : close opened scanner device
   - sane_exit() : terminate use of backend

*/

/*
 * @@ Section 1 - Init
 */

#include "../include/sane/config.h"

#include <string.h> /*memcpy...*/
#include <ctype.h> /*isspace*/
#include <math.h> /*tan*/
#include <unistd.h> /*usleep*/
#include <sys/time.h> /*gettimeofday*/

#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_scsi.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_config.h"

#include "canon_dr-cmd.h"
#include "canon_dr.h"

#define DEBUG 1
#define BUILD 51

/* values for SANE_DEBUG_CANON_DR env var:
 - errors           5
 - function trace  10
 - function detail 15
 - get/setopt cmds 20
 - scsi/usb trace  25 
 - scsi/usb writes 30
 - scsi/usb reads  31
 - useless noise   35
*/

/* ------------------------------------------------------------------------- */
/* if JPEG support is not enabled in sane.h, we setup our own defines */
#ifndef SANE_FRAME_JPEG
#define SANE_FRAME_JPEG 0x0B
#define SANE_JPEG_DISABLED 1
#endif
/* ------------------------------------------------------------------------- */
#define STRING_FLATBED SANE_I18N("Flatbed")
#define STRING_ADFFRONT SANE_I18N("ADF Front")
#define STRING_ADFBACK SANE_I18N("ADF Back")
#define STRING_ADFDUPLEX SANE_I18N("ADF Duplex")
#define STRING_CARDFRONT SANE_I18N("Card Front")
#define STRING_CARDBACK SANE_I18N("Card Back")
#define STRING_CARDDUPLEX SANE_I18N("Card Duplex")

#define STRING_LINEART SANE_VALUE_SCAN_MODE_LINEART
#define STRING_HALFTONE SANE_VALUE_SCAN_MODE_HALFTONE
#define STRING_GRAYSCALE SANE_VALUE_SCAN_MODE_GRAY
#define STRING_COLOR SANE_VALUE_SCAN_MODE_COLOR

#define STRING_RED SANE_I18N("Red")
#define STRING_GREEN SANE_I18N("Green")
#define STRING_BLUE SANE_I18N("Blue")
#define STRING_EN_RED SANE_I18N("Enhance Red")
#define STRING_EN_GREEN SANE_I18N("Enhance Green")
#define STRING_EN_BLUE SANE_I18N("Enhance Blue")

#define STRING_NONE SANE_I18N("None")
#define STRING_JPEG SANE_I18N("JPEG")

/* Also set via config file. */
static int global_buffer_size;
static int global_buffer_size_default = 2 * 1024 * 1024;
static int global_padded_read;
static int global_padded_read_default = 0;
static int global_extra_status;
static int global_extra_status_default = 0;
static int global_duplex_offset;
static int global_duplex_offset_default = 0;
static char global_vendor_name[9];
static char global_model_name[17];
static char global_version_name[5];

/*
 * used by attach* and sane_get_devices
 * a ptr to a null term array of ptrs to SANE_Device structs
 * a ptr to a single-linked list of scanner structs
 */
static const SANE_Device **sane_devArray = NULL;
static struct scanner *scanner_devList = NULL;

/*
 * @@ Section 2 - SANE & scanner init code
 */

/*
 * Called by SANE initially.
 * 
 * From the SANE spec:
 * This function must be called before any other SANE function can be
 * called. The behavior of a SANE backend is undefined if this
 * function is not called first. The version code of the backend is
 * returned in the value pointed to by version_code. If that pointer
 * is NULL, no version code is returned. Argument authorize is either
 * a pointer to a function that is invoked when the backend requires
 * authentication for a specific resource or NULL if the frontend does
 * not support authentication.
 */
SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  authorize = authorize;        /* get rid of compiler warning */

  DBG_INIT ();
  DBG (10, "sane_init: start\n");

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, BUILD);

  DBG (5, "sane_init: canon_dr backend %d.%d.%d, from %s\n",
    SANE_CURRENT_MAJOR, V_MINOR, BUILD, PACKAGE_STRING);

  DBG (10, "sane_init: finish\n");

  return SANE_STATUS_GOOD;
}

/*
 * Called by SANE to find out about supported devices.
 * 
 * From the SANE spec:
 * This function can be used to query the list of devices that are
 * available. If the function executes successfully, it stores a
 * pointer to a NULL terminated array of pointers to SANE_Device
 * structures in *device_list. The returned list is guaranteed to
 * remain unchanged and valid until (a) another call to this function
 * is performed or (b) a call to sane_exit() is performed. This
 * function can be called repeatedly to detect when new devices become
 * available. If argument local_only is true, only local devices are
 * returned (devices directly attached to the machine that SANE is
 * running on). If it is false, the device list includes all remote
 * devices that are accessible to the SANE library.
 * 
 * SANE does not require that this function is called before a
 * sane_open() call is performed. A device name may be specified
 * explicitly by a user which would make it unnecessary and
 * undesirable to call this function first.
 */
/*
 * Read the config file, find scanners with help from sanei_*
 * and store in global device structs
 */
SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  SANE_Status ret = SANE_STATUS_GOOD;
  struct scanner * s;
  struct scanner * prev = NULL;
  char line[PATH_MAX];
  const char *lp;
  FILE *fp;
  int num_devices=0;
  int i=0;

  local_only = local_only;        /* get rid of compiler warning */

  DBG (10, "sane_get_devices: start\n");

  /* mark all existing scanners as missing, attach_one will remove mark */
  for (s = scanner_devList; s; s = s->next) {
    s->missing = 1;
  }

  sanei_usb_init();

  /* reset globals before reading the file */
  default_globals();

  fp = sanei_config_open (CANON_DR_CONFIG_FILE);

  if (fp) {

      DBG (15, "sane_get_devices: reading config file %s\n",
        CANON_DR_CONFIG_FILE);

      while (sanei_config_read (line, PATH_MAX, fp)) {
    
          lp = line;
    
          /* ignore comments */
          if (*lp == '#')
            continue;
    
          /* skip empty lines */
          if (*lp == 0)
            continue;
    
          if (!strncmp ("option", lp, 6) && isspace (lp[6])) {
    
              lp += 6;
              lp = sanei_config_skip_whitespace (lp);
    
              /* BUFFERSIZE: > 4K */
              if (!strncmp (lp, "buffer-size", 11) && isspace (lp[11])) {
    
                  int buf;
                  lp += 11;
                  lp = sanei_config_skip_whitespace (lp);
                  buf = atoi (lp);
    
                  if (buf < 4096) {
                    DBG (5, "sane_get_devices: config option \"buffer-size\" "
                      "(%d) is < 4096, ignoring!\n", buf);
                    continue;
                  }
    
                  if (buf > global_buffer_size_default) {
                    DBG (5, "sane_get_devices: config option \"buffer-size\" "
                      "(%d) is > %d, scanning problems may result\n", buf,
                      global_buffer_size_default);
                  }
    
                  DBG (15, "sane_get_devices: setting \"buffer-size\" to %d\n",
                    buf);

                  global_buffer_size = buf;
              }

              /* PADDED READ: we clamp to 0 or 1 */
              else if (!strncmp (lp, "padded-read", 11) && isspace (lp[11])) {
    
                  int buf;
                  lp += 11;
                  lp = sanei_config_skip_whitespace (lp);
                  buf = atoi (lp);
    
                  if (buf < 0) {
                    DBG (5, "sane_get_devices: config option \"padded-read\" "
                      "(%d) is < 0, ignoring!\n", buf);
                    continue;
                  }
    
                  if (buf > 1) {
                    DBG (5, "sane_get_devices: config option \"padded-read\" "
                      "(%d) is > 1, ignoring!\n", buf);
                    continue;
                  }
    
                  DBG (15, "sane_get_devices: setting \"padded-read\" to %d\n",
                    buf);

                  global_padded_read = buf;
              }

              /* EXTRA STATUS: we clamp to 0 or 1 */
              else if (!strncmp (lp, "extra-status", 12) && isspace (lp[12])) {
    
                  int buf;
                  lp += 12;
                  lp = sanei_config_skip_whitespace (lp);
                  buf = atoi (lp);
    
                  if (buf < 0) {
                    DBG (5, "sane_get_devices: config option \"extra-status\" "
                      "(%d) is < 0, ignoring!\n", buf);
                    continue;
                  }
    
                  if (buf > 1) {
                    DBG (5, "sane_get_devices: config option \"extra-status\" "
                      "(%d) is > 1, ignoring!\n", buf);
                    continue;
                  }
    
                  DBG (15, "sane_get_devices: setting \"extra-status\" to %d\n",
                    buf);

                  global_extra_status = buf;
              }

              /* DUPLEXOFFSET: < 1200 */
              else if (!strncmp (lp, "duplex-offset", 13) && isspace (lp[13])) {
    
                  int buf;
                  lp += 13;
                  lp = sanei_config_skip_whitespace (lp);
                  buf = atoi (lp);
    
                  if (buf > 1200) {
                    DBG (5, "sane_get_devices: config option \"duplex-offset\" "
                      "(%d) is > 1200, ignoring!\n", buf);
                    continue;
                  }
    
                  if (buf < 0) {
                    DBG (5, "sane_get_devices: config option \"duplex-offset\" "
                      "(%d) is < 0, ignoring!\n", buf);
                    continue;
                  }

                  DBG (15, "sane_get_devices: setting \"duplex-offset\" to %d\n",
                    buf);

                  global_duplex_offset = buf;
              }

              /* VENDOR: we ingest up to 8 bytes */
              else if (!strncmp (lp, "vendor-name", 11) && isspace (lp[11])) {

                  lp += 11;
                  lp = sanei_config_skip_whitespace (lp);
                  strncpy(global_vendor_name, lp, 8);
                  global_vendor_name[8] = 0;
    
                  DBG (15, "sane_get_devices: setting \"vendor-name\" to %s\n",
                    global_vendor_name);
              }

              /* MODEL: we ingest up to 16 bytes */
              else if (!strncmp (lp, "model-name", 10) && isspace (lp[10])) {

                  lp += 10;
                  lp = sanei_config_skip_whitespace (lp);
                  strncpy(global_model_name, lp, 16);
                  global_model_name[16] = 0;
    
                  DBG (15, "sane_get_devices: setting \"model-name\" to %s\n",
                    global_model_name);
              }

              /* VERSION: we ingest up to 4 bytes */
              else if (!strncmp (lp, "version-name", 12) && isspace (lp[12])) {

                  lp += 12;
                  lp = sanei_config_skip_whitespace (lp);
                  strncpy(global_version_name, lp, 4);
                  global_version_name[4] = 0;
    
                  DBG (15, "sane_get_devices: setting \"version-name\" to %s\n",
                    global_version_name);
              }

              else {
                  DBG (5, "sane_get_devices: config option \"%s\" unrecognized "
                  "- ignored.\n", lp);
              }
          }
          else if ((strncmp ("usb", lp, 3) == 0) && isspace (lp[3])) {
              DBG (15, "sane_get_devices: looking for '%s'\n", lp);
              sanei_usb_attach_matching_devices(lp, attach_one_usb);

              /* re-default these after reading the usb line */
              default_globals();
          }
          else if ((strncmp ("scsi", lp, 4) == 0) && isspace (lp[4])) {
              DBG (15, "sane_get_devices: looking for '%s'\n", lp);
              sanei_config_attach_matching_devices (lp, attach_one_scsi);

              /* re-default these after reading the scsi line */
              default_globals();
          }
          else{
              DBG (5, "sane_get_devices: config line \"%s\" unrecognized - "
              "ignored.\n", lp);
          }
      }
      fclose (fp);
  }

  else {
      DBG (5, "sane_get_devices: missing required config file '%s'!\n",
        CANON_DR_CONFIG_FILE);
  }

  /*delete missing scanners from list*/
  for (s = scanner_devList; s;) {
    if(s->missing){
      DBG (5, "sane_get_devices: missing scanner %s\n",s->device_name);

      /*splice s out of list by changing pointer in prev to next*/
      if(prev){
        prev->next = s->next;
        free(s);
        s=prev->next;
      }
      /*remove s from head of list, using prev to cache it*/
      else{
        prev = s;
        s = s->next;
        free(prev);
	prev=NULL;

	/*reset head to next s*/
	scanner_devList = s;
      }
    }
    else{
      prev = s;
      s=prev->next;
    }
  }

  for (s = scanner_devList; s; s=s->next) {
    DBG (15, "sane_get_devices: found scanner %s\n",s->device_name);
    num_devices++;
  }

  DBG (15, "sane_get_devices: found %d scanner(s)\n",num_devices);

  if (sane_devArray)
    free (sane_devArray);

  sane_devArray = calloc (num_devices + 1, sizeof (SANE_Device*));
  if (!sane_devArray)
    return SANE_STATUS_NO_MEM;

  for (s = scanner_devList; s; s=s->next) {
    sane_devArray[i++] = (SANE_Device *)&s->sane;
  }
  sane_devArray[i] = 0;

  if(device_list){
      *device_list = sane_devArray;
  }

  DBG (10, "sane_get_devices: finish\n");

  return ret;
}

/* callbacks used by sane_get_devices */
static SANE_Status
attach_one_scsi (const char *device_name)
{
  return attach_one(device_name,CONNECTION_SCSI);
}

static SANE_Status
attach_one_usb (const char *device_name)
{
  return attach_one(device_name,CONNECTION_USB);
}

/* build the scanner struct and link to global list 
 * unless struct is already loaded, then pretend 
 */
static SANE_Status
attach_one (const char *device_name, int connType)
{
  struct scanner *s;
  int ret;

  DBG (10, "attach_one: start\n");
  DBG (15, "attach_one: looking for '%s'\n", device_name);

  for (s = scanner_devList; s; s = s->next) {
    if (strcmp (s->device_name, device_name) == 0){
      DBG (10, "attach_one: already attached!\n");
      s->missing = 0;
      return SANE_STATUS_GOOD;
    }
  }

  /* build a scanner struct to hold it */
  if ((s = calloc (sizeof (*s), 1)) == NULL)
    return SANE_STATUS_NO_MEM;

  /* config file settings */
  s->buffer_size = global_buffer_size;
  s->padded_read = global_padded_read;
  s->extra_status = global_extra_status;
  s->duplex_offset = global_duplex_offset;

  /* copy the device name */
  strcpy (s->device_name, device_name);

  /* connect the fd */
  s->connection = connType;
  s->fd = -1;
  ret = connect_fd(s);
  if(ret != SANE_STATUS_GOOD){
    free (s);
    return ret;
  }

  /* query the device to load its vendor/model/version, */
  /* if config file doesn't give all three */
  if ( !strlen(global_vendor_name)
    || !strlen(global_model_name)
    || !strlen(global_version_name)
  ){
    ret = init_inquire (s);
    if (ret != SANE_STATUS_GOOD) {
      disconnect_fd(s);
      free (s);
      DBG (5, "attach_one: inquiry failed\n");
      return ret;
    }
  }

  /* override any inquiry settings with those from config file */
  if(strlen(global_vendor_name))
    strcpy(s->vendor_name, global_vendor_name);
  if(strlen(global_model_name))
    strcpy(s->model_name, global_model_name);
  if(strlen(global_version_name))
    strcpy(s->version_name, global_version_name);

  /* load detailed specs/capabilities from the device */
  /* if a model cannot support inquiry vpd, this function will die */
  ret = init_vpd (s);
  if (ret != SANE_STATUS_GOOD) {
    disconnect_fd(s);
    free (s);
    DBG (5, "attach_one: vpd failed\n");
    return ret;
  }

  /* clean up the scanner struct based on model */
  /* this is the big piece of model specific code */
  ret = init_model (s);
  if (ret != SANE_STATUS_GOOD) {
    disconnect_fd(s);
    free (s);
    DBG (5, "attach_one: model failed\n");
    return ret;
  }

  /* enable/read the buttons */
  ret = init_panel (s);
  if (ret != SANE_STATUS_GOOD) {
    disconnect_fd(s);
    free (s);
    DBG (5, "attach_one: model failed\n");
    return ret;
  }

  /* sets SANE option 'values' to good defaults */
  ret = init_user (s);
  if (ret != SANE_STATUS_GOOD) {
    disconnect_fd(s);
    free (s);
    DBG (5, "attach_one: user failed\n");
    return ret;
  }

  ret = init_options (s);
  if (ret != SANE_STATUS_GOOD) {
    disconnect_fd(s);
    free (s);
    DBG (5, "attach_one: options failed\n");
    return ret;
  }

  /* load strings into sane_device struct */
  s->sane.name = s->device_name;
  s->sane.vendor = s->vendor_name;
  s->sane.model = s->model_name;
  s->sane.type = "scanner";

  /* change name in sane_device struct if scanner has serial number
  ret = init_serial (s);
  if (ret == SANE_STATUS_GOOD) {
    s->sane.name = s->serial_name;
  }
  else{
    DBG (5, "attach_one: serial number unsupported?\n");
  }
  */

  /* we close the connection, so that another backend can talk to scanner */
  disconnect_fd(s);

  /* store this scanner in global vars */
  s->next = scanner_devList;
  scanner_devList = s;

  DBG (10, "attach_one: finish\n");

  return SANE_STATUS_GOOD;
}

/*
 * connect the fd in the scanner struct
 */
static SANE_Status
connect_fd (struct scanner *s)
{
  SANE_Status ret;
  int buffer_size = s->buffer_size;

  DBG (10, "connect_fd: start\n");

  if(s->fd > -1){
    DBG (5, "connect_fd: already open\n");
    ret = SANE_STATUS_GOOD;
  }
  else if (s->connection == CONNECTION_USB) {
    DBG (15, "connect_fd: opening USB device (%s)\n", s->device_name);
    ret = sanei_usb_open (s->device_name, &(s->fd));
    if(!ret){
      ret = sanei_usb_clear_halt(s->fd);
    }
  }
  else {
    DBG (15, "connect_fd: opening SCSI device (%s)\n", s->device_name);
    ret = sanei_scsi_open_extended (s->device_name, &(s->fd), sense_handler, s,
      &s->buffer_size);
    if(!ret && buffer_size != s->buffer_size){
      DBG (5, "connect_fd: cannot get requested buffer size (%d/%d)\n",
        buffer_size, s->buffer_size);
    }
  }

  if(ret == SANE_STATUS_GOOD){

    /* first generation usb scanners can get flaky if not closed 
     * properly after last use. very first commands sent to device 
     * must be prepared to correct this- see wait_scanner() */
    ret = wait_scanner(s);
    if (ret != SANE_STATUS_GOOD) {
      DBG (5, "connect_fd: could not wait_scanner\n");
      disconnect_fd(s);
    }

  }
  else{
    DBG (5, "connect_fd: could not open device: %d\n", ret);
  }

  DBG (10, "connect_fd: finish\n");

  return ret;
}

/*
 * This routine will check if a certain device is a Canon scanner
 * It also copies interesting data from INQUIRY into the handle structure
 */
static SANE_Status
init_inquire (struct scanner *s)
{
  int i;
  SANE_Status ret;

  unsigned char cmd[INQUIRY_len];
  size_t cmdLen = INQUIRY_len;

  unsigned char in[INQUIRY_std_len];
  size_t inLen = INQUIRY_std_len;

  DBG (10, "init_inquire: start\n");

  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd, INQUIRY_code);
  set_IN_return_size (cmd, inLen);
  set_IN_evpd (cmd, 0);
  set_IN_page_code (cmd, 0);
 
  ret = do_cmd (
    s, 1, 0, 
    cmd, cmdLen,
    NULL, 0,
    in, &inLen
  );

  if (ret != SANE_STATUS_GOOD){
    DBG (10, "init_inquire: failed: %d\n", ret);
    return ret;
  }

  if (get_IN_periph_devtype (in) != IN_periph_devtype_scanner){
    DBG (5, "The device at '%s' is not a scanner.\n", s->device_name);
    return SANE_STATUS_INVAL;
  }

  get_IN_vendor (in, s->vendor_name);
  get_IN_product (in, s->model_name);
  get_IN_version (in, s->version_name);

  s->vendor_name[8] = 0;
  s->model_name[16] = 0;
  s->version_name[4] = 0;

  /* gobble trailing spaces */
  for (i = 7; s->vendor_name[i] == ' ' && i >= 0; i--)
    s->vendor_name[i] = 0;
  for (i = 15; s->model_name[i] == ' ' && i >= 0; i--)
    s->model_name[i] = 0;
  for (i = 3; s->version_name[i] == ' ' && i >= 0; i--)
    s->version_name[i] = 0;

  /*check for vendor name*/
  if (strcmp ("CANON", s->vendor_name)) {
    DBG (5, "The device at '%s' is reported to be made by '%s'\n",
      s->device_name, s->vendor_name);
    DBG (5, "This backend only supports Canon products.\n");
    return SANE_STATUS_INVAL;
  }

  /*check for model name*/
  if (strncmp ("DR", s->model_name, 2)
   && strncmp ("CR", s->model_name, 2)
   && strncmp ("P-", s->model_name, 2)
  ) {
    DBG (5, "The device at '%s' is reported to be a '%s'\n",
      s->device_name, s->model_name);
    DBG (5, "This backend only supports Canon P-, CR & DR-series products.\n");
    return SANE_STATUS_INVAL;
  }

  DBG (15, "init_inquire: Found %s scanner %s version %s at %s\n",
    s->vendor_name, s->model_name, s->version_name, s->device_name);

  DBG (10, "init_inquire: finish\n");

  return SANE_STATUS_GOOD;
}

/*
 * Use INQUIRY VPD to setup more detail about the scanner
 */
static SANE_Status
init_vpd (struct scanner *s)
{
  SANE_Status ret;

  unsigned char cmd[INQUIRY_len];
  size_t cmdLen = INQUIRY_len;

  unsigned char in[INQUIRY_vpd_len];
  size_t inLen = INQUIRY_vpd_len;

  DBG (10, "init_vpd: start\n");

  /* get EVPD */
  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd, INQUIRY_code);
  set_IN_return_size (cmd, inLen);
  set_IN_evpd (cmd, 1);
  set_IN_page_code (cmd, 0xf0);

  ret = do_cmd (
    s, 1, 0,
    cmd, cmdLen,
    NULL, 0,
    in, &inLen
  );

  DBG (15, "init_vpd: length=%0x\n",get_IN_page_length (in));

  /* This scanner supports vital product data.
   * Use this data to set dpi-lists etc. */
  if (ret == SANE_STATUS_GOOD || ret == SANE_STATUS_EOF) {

      DBG (15, "standard options\n");

      s->basic_x_res = get_IN_basic_x_res (in);
      DBG (15, "  basic x res: %d dpi\n",s->basic_x_res);

      s->basic_y_res = get_IN_basic_y_res (in);
      DBG (15, "  basic y res: %d dpi\n",s->basic_y_res);

      s->step_x_res = get_IN_step_x_res (in);
      DBG (15, "  step x res: %d dpi\n", s->step_x_res);

      s->step_y_res = get_IN_step_y_res (in);
      DBG (15, "  step y res: %d dpi\n", s->step_y_res);

      s->max_x_res = get_IN_max_x_res (in);
      DBG (15, "  max x res: %d dpi\n", s->max_x_res);

      s->max_y_res = get_IN_max_y_res (in);
      DBG (15, "  max y res: %d dpi\n", s->max_y_res);

      s->min_x_res = get_IN_min_x_res (in);
      DBG (15, "  min x res: %d dpi\n", s->min_x_res);

      s->min_y_res = get_IN_min_y_res (in);
      DBG (15, "  min y res: %d dpi\n", s->min_y_res);

      /* some scanners list B&W resolutions. */
      s->std_res_x[DPI_60] = get_IN_std_res_60 (in);
      s->std_res_y[DPI_60] = s->std_res_x[DPI_60];
      DBG (15, "  60 dpi: %d\n", s->std_res_x[DPI_60]);

      s->std_res_x[DPI_75] = get_IN_std_res_75 (in);
      s->std_res_y[DPI_75] = s->std_res_x[DPI_75];
      DBG (15, "  75 dpi: %d\n", s->std_res_x[DPI_75]);

      s->std_res_x[DPI_100] = get_IN_std_res_100 (in);
      s->std_res_y[DPI_100] = s->std_res_x[DPI_100];
      DBG (15, "  100 dpi: %d\n", s->std_res_x[DPI_100]);

      s->std_res_x[DPI_120] = get_IN_std_res_120 (in);
      s->std_res_y[DPI_120] = s->std_res_x[DPI_120];
      DBG (15, "  120 dpi: %d\n", s->std_res_x[DPI_120]);

      s->std_res_x[DPI_150] = get_IN_std_res_150 (in);
      s->std_res_y[DPI_150] = s->std_res_x[DPI_150];
      DBG (15, "  150 dpi: %d\n", s->std_res_x[DPI_150]);

      s->std_res_x[DPI_160] = get_IN_std_res_160 (in);
      s->std_res_y[DPI_160] = s->std_res_x[DPI_160];
      DBG (15, "  160 dpi: %d\n", s->std_res_x[DPI_160]);

      s->std_res_x[DPI_180] = get_IN_std_res_180 (in);
      s->std_res_y[DPI_180] = s->std_res_x[DPI_180];
      DBG (15, "  180 dpi: %d\n", s->std_res_x[DPI_180]);

      s->std_res_x[DPI_200] = get_IN_std_res_200 (in);
      s->std_res_y[DPI_200] = s->std_res_x[DPI_200];
      DBG (15, "  200 dpi: %d\n", s->std_res_x[DPI_200]);

      s->std_res_x[DPI_240] = get_IN_std_res_240 (in);
      s->std_res_y[DPI_240] = s->std_res_x[DPI_240];
      DBG (15, "  240 dpi: %d\n", s->std_res_x[DPI_240]);

      s->std_res_x[DPI_300] = get_IN_std_res_300 (in);
      s->std_res_y[DPI_300] = s->std_res_x[DPI_300];
      DBG (15, "  300 dpi: %d\n", s->std_res_x[DPI_300]);

      s->std_res_x[DPI_320] = get_IN_std_res_320 (in);
      s->std_res_y[DPI_320] = s->std_res_x[DPI_320];
      DBG (15, "  320 dpi: %d\n", s->std_res_x[DPI_320]);

      s->std_res_x[DPI_400] = get_IN_std_res_400 (in);
      s->std_res_y[DPI_400] = s->std_res_x[DPI_400];
      DBG (15, "  400 dpi: %d\n", s->std_res_x[DPI_400]);

      s->std_res_x[DPI_480] = get_IN_std_res_480 (in);
      s->std_res_y[DPI_480] = s->std_res_x[DPI_480];
      DBG (15, "  480 dpi: %d\n", s->std_res_x[DPI_480]);

      s->std_res_x[DPI_600] = get_IN_std_res_600 (in);
      s->std_res_y[DPI_600] = s->std_res_x[DPI_600];
      DBG (15, "  600 dpi: %d\n", s->std_res_x[DPI_600]);

      s->std_res_x[DPI_800] = get_IN_std_res_800 (in);
      s->std_res_y[DPI_800] = s->std_res_x[DPI_800];
      DBG (15, "  800 dpi: %d\n", s->std_res_x[DPI_800]);

      s->std_res_x[DPI_1200] = get_IN_std_res_1200 (in);
      s->std_res_y[DPI_1200] = s->std_res_x[DPI_1200];
      DBG (15, "  1200 dpi: %d\n", s->std_res_x[DPI_1200]);

      /* maximum window width and length are reported in basic units.*/
      s->max_x = get_IN_window_width(in) * 1200 / s->basic_x_res;
      DBG(15, "  max width: %d (%2.2f in)\n",s->max_x,(float)s->max_x/1200);

      s->max_y = get_IN_window_length(in) * 1200 / s->basic_y_res;
      DBG(15, "  max length: %d (%2.2f in)\n",s->max_y,(float)s->max_y/1200);

      DBG (15, "  AWD: %d\n", get_IN_awd(in));
      DBG (15, "  CE Emphasis: %d\n", get_IN_ce_emphasis(in));
      DBG (15, "  C Emphasis: %d\n", get_IN_c_emphasis(in));
      DBG (15, "  High quality: %d\n", get_IN_high_quality(in));

      /* known modes FIXME more here? */
      s->can_grayscale = get_IN_multilevel (in);
      DBG (15, "  grayscale: %d\n", s->can_grayscale);

      s->can_halftone = get_IN_half_tone (in);
      DBG (15, "  halftone: %d\n", s->can_halftone);

      s->can_monochrome = get_IN_monochrome (in);
      DBG (15, "  monochrome: %d\n", s->can_monochrome);

      s->can_overflow = get_IN_overflow(in);
      DBG (15, "  overflow: %d\n", s->can_overflow);
  }
  /*FIXME no vpd, set some defaults? */
  else{
    DBG (5, "init_vpd: Your scanner does not support VPD?\n");
    DBG (5, "init_vpd: Please contact kitno455 at gmail dot com\n");
    DBG (5, "init_vpd: with details of your scanner model.\n");
  }

  DBG (10, "init_vpd: finish\n");

  return ret;
}

/*
 * get model specific info that is not in vpd, and correct
 * errors in vpd data. struct is already initialized to 0.
 */
static SANE_Status
init_model (struct scanner *s)
{

  DBG (10, "init_model: start\n");

  s->reverse_by_mode[MODE_LINEART] = 1;
  s->reverse_by_mode[MODE_HALFTONE] = 1;
  s->reverse_by_mode[MODE_GRAYSCALE] = 0;
  s->reverse_by_mode[MODE_COLOR] = 0;

  s->color_interlace[SIDE_FRONT] = COLOR_INTERLACE_RGB;
  s->color_interlace[SIDE_BACK] = COLOR_INTERLACE_RGB;

  s->always_op = 1;
  s->has_df = 1;
  s->has_btc = 1;
  s->has_counter = 1;
  s->has_adf = 1;
  s->has_duplex = 1;
  s->has_buffer = 1;
  s->can_read_panel = 1;
  s->can_write_panel = 1;
  s->has_ssm = 1;

  s->brightness_steps = 255;
  s->contrast_steps = 255;
  s->threshold_steps = 255;

  s->ppl_mod = 1;
  s->bg_color = 0xee;

  /* assume these are same as adf, override below */
  s->valid_x = s->max_x;
  s->max_x_fb = s->max_x;
  s->max_y_fb = s->max_y;

  /* generic settings missing from vpd */
  if (strstr (s->model_name,"C")){
    s->can_color = 1;
  }

  /* specific settings missing from vpd */
  if (strstr (s->model_name,"DR-9080")
    || strstr (s->model_name,"DR-7580")){
    s->has_comp_JPEG = 1;
    s->rgb_format = 2;
  }

  else if (strstr (s->model_name,"DR-7090")){
    s->has_flatbed = 1;
  }

  else if (strstr (s->model_name,"DR-9050")
    || strstr (s->model_name,"DR-7550")
    || strstr (s->model_name,"DR-6050")
    || strstr (s->model_name,"DR-G1100")
    || strstr (s->model_name,"DR-G1130")
  ){

    /*missing*/
    s->std_res_x[DPI_100]=1;
    s->std_res_y[DPI_100]=1;
    s->std_res_x[DPI_150]=1;
    s->std_res_y[DPI_150]=1;
    s->std_res_x[DPI_200]=1;
    s->std_res_y[DPI_200]=1;
    s->std_res_x[DPI_240]=1;
    s->std_res_y[DPI_240]=1;
    s->std_res_x[DPI_300]=1;
    s->std_res_y[DPI_300]=1;
    s->std_res_x[DPI_400]=1;
    s->std_res_y[DPI_400]=1;
    s->std_res_x[DPI_600]=1;
    s->std_res_y[DPI_600]=1;
    
    /*weirdness*/
    s->has_ssm = 0;
    s->has_ssm2 = 1;
  }

  else if (strstr (s->model_name,"DR-4080")
    || strstr (s->model_name,"DR-4580")
    || strstr (s->model_name,"DR-7080")){
    s->has_flatbed = 1;
  }

  else if (strstr (s->model_name,"DR-2580")){
    s->invert_tly = 1;
    s->rgb_format = 1;
    s->color_interlace[SIDE_FRONT] = COLOR_INTERLACE_RRGGBB;
    s->color_interlace[SIDE_BACK] = COLOR_INTERLACE_rRgGbB;
    s->gray_interlace[SIDE_BACK] = GRAY_INTERLACE_gG;
    s->duplex_interlace = DUPLEX_INTERLACE_FBFB;
    s->need_ccal = 1;
    s->need_fcal = 1;
    /*s->duplex_offset = 432; now set in config file*/
    s->duplex_offset_side = SIDE_BACK;

    /*lies*/
    s->can_halftone=0;
    s->can_monochrome=0;
  }

  else if (strstr (s->model_name,"DR-2510")
   || strstr (s->model_name,"DR-2010")
  ){
    s->rgb_format = 1;
    s->always_op = 0;
    s->unknown_byte2 = 0x80;
    s->fixed_width = 1;
    s->valid_x = 8.5 * 1200;
    s->gray_interlace[SIDE_FRONT] = GRAY_INTERLACE_2510;
    s->gray_interlace[SIDE_BACK] = GRAY_INTERLACE_2510;
    s->color_interlace[SIDE_FRONT] = COLOR_INTERLACE_2510;
    s->color_interlace[SIDE_BACK] = COLOR_INTERLACE_2510;
    s->duplex_interlace = DUPLEX_INTERLACE_2510;
    /*s->duplex_offset = 400; now set in config file*/
    s->need_ccal = 1;
    s->need_fcal = 1;
    s->sw_lut = 1;
    /*s->invert_tly = 1;*/

    /*only in Y direction, so we trash them in X*/
    s->std_res_x[DPI_100]=0;
    s->std_res_x[DPI_150]=0;
    s->std_res_x[DPI_200]=0;
    s->std_res_x[DPI_240]=0;
    s->std_res_x[DPI_400]=0;

    /*lies*/
    s->can_halftone=0;
    s->can_monochrome=0;
  }

  /* copied from 2510, possibly incorrect */
  else if (strstr (s->model_name,"DR-3010")){
    s->rgb_format = 1;
    s->always_op = 0;
    s->unknown_byte2 = 0x80;
    s->fixed_width = 1;
    s->valid_x = 8.5 * 1200;
    s->gray_interlace[SIDE_FRONT] = GRAY_INTERLACE_2510;
    s->gray_interlace[SIDE_BACK] = GRAY_INTERLACE_2510;
    s->color_interlace[SIDE_FRONT] = COLOR_INTERLACE_2510;
    s->color_interlace[SIDE_BACK] = COLOR_INTERLACE_2510;
    s->duplex_interlace = DUPLEX_INTERLACE_2510;
    /*s->duplex_offset = 400; now set in config file*/
    s->need_ccal = 1;
    s->need_fcal = 1;
    s->sw_lut = 1;
    s->invert_tly = 1;

    /*only in Y direction, so we trash them in X*/
    s->std_res_x[DPI_100]=0;
    s->std_res_x[DPI_150]=0;
    s->std_res_x[DPI_200]=0;
    s->std_res_x[DPI_240]=0;
    s->std_res_x[DPI_400]=0;

    /*lies*/
    s->can_halftone=0;
    s->can_monochrome=0;
  }

  else if (strstr (s->model_name,"DR-2050")
   || strstr (s->model_name,"DR-2080")){
    s->can_write_panel = 0;
    s->has_df = 0;
    s->fixed_width = 1;
    s->even_Bpl = 1;
    s->color_interlace[SIDE_FRONT] = COLOR_INTERLACE_RRGGBB;
    s->color_interlace[SIDE_BACK] = COLOR_INTERLACE_RRGGBB;
    s->duplex_interlace = DUPLEX_INTERLACE_FBFB;
    s->need_fcal_buffer = 1;
    s->bg_color = 0x08;
    /*s->duplex_offset = 840; now set in config file*/
    s->sw_lut = 1;

    /*lies*/
    s->can_halftone=0;
    s->can_monochrome=0;
  }

  else if (strstr (s->model_name,"DR-3080")){
    s->can_write_panel = 0;
    s->has_df = 0;
    s->has_btc = 0;
  }

  else if (strstr (s->model_name,"DR-5060F")){
    s->can_write_panel = 0;
    s->has_df = 0;
    s->has_btc = 0;
    s->ppl_mod = 32;
    s->reverse_by_mode[MODE_LINEART] = 0;
    s->reverse_by_mode[MODE_HALFTONE] = 0;
  }

  else if (strstr (s->model_name,"DR-5020")){
    s->can_read_panel = 0;
    s->can_write_panel = 0;
    s->has_df = 0;
    s->has_btc = 0;
    s->ppl_mod = 32;
    s->reverse_by_mode[MODE_LINEART] = 0;
    s->reverse_by_mode[MODE_HALFTONE] = 0;
  }

  else if (strstr (s->model_name, "P-208")) {
    s->color_interlace[SIDE_FRONT] = COLOR_INTERLACE_RRGGBB;
    s->color_interlace[SIDE_BACK] = COLOR_INTERLACE_rRgGbB;
    s->gray_interlace[SIDE_BACK] = GRAY_INTERLACE_gG;
    s->duplex_interlace = DUPLEX_INTERLACE_FBFB;
    s->need_ccal = 1;
    s->invert_tly = 1;
    s->can_color = 1;
    s->unknown_byte2 = 0x88;
    s->rgb_format = 1;
    s->has_ssm_pay_head_len = 1;
    s->ppl_mod = 8;
    s->ccal_version = 3;
    s->can_read_sensors = 1;
  }

  else if (strstr (s->model_name, "P-215")) {
    s->color_interlace[SIDE_FRONT] = COLOR_INTERLACE_rRgGbB;
    s->color_interlace[SIDE_BACK] = COLOR_INTERLACE_RRGGBB;
    s->gray_interlace[SIDE_FRONT] = GRAY_INTERLACE_gG;
    s->duplex_interlace = DUPLEX_INTERLACE_FBFB;
    s->need_ccal = 1;
    s->invert_tly = 1;
    s->can_color = 1;
    s->unknown_byte2 = 0x88;
    s->rgb_format = 1;
    s->has_ssm_pay_head_len = 1;
    s->ppl_mod = 8;
    s->ccal_version = 3;
    s->can_read_sensors = 1;
    s->has_card = 1;
  }

  else if (strstr (s->model_name,"DR-M160")){

    /*missing*/
    s->std_res_x[DPI_100]=1;
    s->std_res_y[DPI_100]=1;
    s->std_res_x[DPI_150]=1;
    s->std_res_y[DPI_150]=1;
    s->std_res_x[DPI_200]=1;
    s->std_res_y[DPI_200]=1;
    s->std_res_x[DPI_300]=1;
    s->std_res_y[DPI_300]=1;
    s->std_res_x[DPI_400]=1;
    s->std_res_y[DPI_400]=1;
    s->std_res_x[DPI_600]=1;
    s->std_res_y[DPI_600]=1;
    
    s->has_comp_JPEG = 1;
    s->rgb_format = 1;
    s->can_color = 1;
    s->has_df_ultra = 1;

    s->color_inter_by_res[DPI_100] = COLOR_INTERLACE_GBR;
    s->color_inter_by_res[DPI_150] = COLOR_INTERLACE_GBR;
    s->color_inter_by_res[DPI_200] = COLOR_INTERLACE_BRG;
    s->color_inter_by_res[DPI_400] = COLOR_INTERLACE_GBR;

    /*weirdness*/
    s->always_op = 0;
    s->fixed_width = 1;
    s->invert_tly = 1;
    s->can_write_panel = 0;
    s->has_ssm = 0;
    s->has_ssm2 = 1;
    s->duplex_interlace = DUPLEX_INTERLACE_FFBB;
    s->duplex_offset_side = SIDE_FRONT;

    /*lies*/
    s->can_halftone=0;
    s->can_monochrome=0;
  }

  else if (strstr (s->model_name,"DR-M140")){

    /*missing*/
    s->std_res_x[DPI_100]=1;
    s->std_res_y[DPI_100]=1;
    s->std_res_x[DPI_150]=1;
    s->std_res_y[DPI_150]=1;
    s->std_res_x[DPI_200]=1;
    s->std_res_y[DPI_200]=1;
    s->std_res_x[DPI_300]=1;
    s->std_res_y[DPI_300]=1;
    s->std_res_x[DPI_400]=1;
    s->std_res_y[DPI_400]=1;
    s->std_res_x[DPI_600]=1;
    s->std_res_y[DPI_600]=1;
    
    s->has_comp_JPEG = 1;
    s->rgb_format = 1;
    s->can_color = 1;
    s->has_df_ultra = 1;

    s->color_inter_by_res[DPI_100] = COLOR_INTERLACE_GBR;
    s->color_inter_by_res[DPI_150] = COLOR_INTERLACE_GBR;
    s->color_inter_by_res[DPI_200] = COLOR_INTERLACE_BRG;
    s->color_inter_by_res[DPI_400] = COLOR_INTERLACE_GBR;

    /*weirdness*/
    s->always_op = 0;
    s->fixed_width = 1;
    s->invert_tly = 1;
    s->can_write_panel = 0;
    s->has_ssm = 0;
    s->has_ssm2 = 1;
    s->duplex_interlace = DUPLEX_INTERLACE_FFBB;
    s->duplex_offset_side = SIDE_BACK;

    /*lies*/
    s->can_halftone=0;
    s->can_monochrome=0;
  }

  else if (strstr (s->model_name,"DR-C125")){

    /*confirmed settings*/
    s->gray_interlace[SIDE_FRONT] = GRAY_INTERLACE_2510;
    s->gray_interlace[SIDE_BACK] = GRAY_INTERLACE_2510;
    s->color_interlace[SIDE_FRONT] = COLOR_INTERLACE_2510;
    s->color_interlace[SIDE_BACK] = COLOR_INTERLACE_2510;
    s->duplex_interlace = DUPLEX_INTERLACE_2510;
    s->duplex_offset_side = SIDE_BACK;
    s->unknown_byte2 = 0x88;
    s->need_ccal = 1;
    s->ccal_version = 3;
    s->need_fcal = 1;
    s->sw_lut = 1;
    s->can_color = 1;
    s->rgb_format = 1;
    /*s->duplex_offset = 400; now set in config file*/

    /*only in Y direction, so we trash them in X*/
    s->std_res_x[DPI_100]=0;
    s->std_res_x[DPI_150]=0;
    s->std_res_x[DPI_200]=0;
    s->std_res_x[DPI_240]=0;
    s->std_res_x[DPI_400]=0;

    /*suspected settings*/
    s->always_op = 0;
    s->fixed_width = 1;
    s->valid_x = 8.5 * 1200;
  }

  else if (strstr (s->model_name,"DR-C225")){

    s->color_interlace[SIDE_FRONT] = COLOR_INTERLACE_RRGGBB;
    s->color_interlace[SIDE_BACK] = COLOR_INTERLACE_rRgGbB;
    s->gray_interlace[SIDE_BACK] = GRAY_INTERLACE_gG;
    s->duplex_interlace = DUPLEX_INTERLACE_FBFB;

    s->unknown_byte2 = 0x88;
    s->need_ccal = 1;
    s->ccal_version = 3;
    s->need_fcal = 1;
    s->invert_tly = 1;
    s->can_color = 1;
    s->rgb_format = 1;
    /*s->duplex_offset = 400; now set in config file*/

    /*only in Y direction, so we trash them in X*/
    s->std_res_x[DPI_100]=0;
    s->std_res_x[DPI_150]=0;
    s->std_res_x[DPI_200]=0;
    s->std_res_x[DPI_240]=0;
    s->std_res_x[DPI_400]=0;

    /*suspected settings*/
    s->always_op = 0;
    s->fixed_width = 1;
    s->valid_x = 8.5 * 1200;
  }

  DBG (10, "init_model: finish\n");

  return SANE_STATUS_GOOD;
}

/*
 * This function enables the buttons and preloads the current panel values
 */
static SANE_Status
init_panel (struct scanner *s)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  DBG (10, "init_panel: start\n");

  ret = read_panel(s,0);
  if(ret){
    DBG (5, "init_panel: disabling read_panel\n");
    s->can_read_panel = 0;
    ret = SANE_STATUS_GOOD;
  }

  s->panel_enable_led = 1;
  s->panel_counter = 0;
  ret = send_panel(s);
  if(ret){
    DBG (5, "init_panel: disabling send_panel\n");
    s->can_write_panel = 0;
    ret = SANE_STATUS_GOOD;
  }

  DBG (10, "init_panel: finish\n");

  return ret;
}

/*
 * set good default user values.
 * struct is already initialized to 0.
 */
static SANE_Status
init_user (struct scanner *s)
{

  DBG (10, "init_user: start\n");

  /* source */
  if(s->has_flatbed)
    s->u.source = SOURCE_FLATBED;
  else if(s->has_adf)
    s->u.source = SOURCE_ADF_FRONT;
  else if(s->has_card)
    s->u.source = SOURCE_CARD_FRONT;

  /* scan mode */
  if(s->can_monochrome)
    s->u.mode=MODE_LINEART;
  else if(s->can_halftone)
    s->u.mode=MODE_HALFTONE;
  else if(s->can_grayscale)
    s->u.mode=MODE_GRAYSCALE;
  else if(s->can_color)
    s->u.mode=MODE_COLOR;

  /*x and y res*/
  s->u.dpi_x = s->basic_x_res;
  s->u.dpi_y = s->basic_x_res;

  /* page width US-Letter */
  s->u.page_x = 8.5 * 1200;
  if(s->u.page_x > s->valid_x){
    s->u.page_x = s->valid_x;
  }

  /* page height US-Letter */
  s->u.page_y = 11 * 1200;
  if(s->u.page_y > s->max_y){
    s->u.page_y = s->max_y;
  }

  /* bottom-right x */
  s->u.br_x = s->u.page_x;

  /* bottom-right y */
  s->u.br_y = s->u.page_y;

  s->threshold = 90;
  s->compress_arg = 50;

  DBG (10, "init_user: finish\n");

  return SANE_STATUS_GOOD;
}

/*
 * This function presets the "option" array to blank
 */
static SANE_Status
init_options (struct scanner *s)
{
  int i;

  DBG (10, "init_options: start\n");

  memset (s->opt, 0, sizeof (s->opt));
  for (i = 0; i < NUM_OPTIONS; ++i) {
      s->opt[i].name = "filler";
      s->opt[i].size = sizeof (SANE_Word);
      s->opt[i].cap = SANE_CAP_INACTIVE;
  }

  /* go ahead and setup the first opt, because 
   * frontend may call control_option on it 
   * before calling get_option_descriptor 
   */
  s->opt[OPT_NUM_OPTS].name = SANE_NAME_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;

  DBG (10, "init_options: finish\n");

  return SANE_STATUS_GOOD;
}

/*
 * From the SANE spec:
 * This function is used to establish a connection to a particular
 * device. The name of the device to be opened is passed in argument
 * name. If the call completes successfully, a handle for the device
 * is returned in *h. As a special case, specifying a zero-length
 * string as the device requests opening the first available device
 * (if there is such a device).
 */
SANE_Status
sane_open (SANE_String_Const name, SANE_Handle * handle)
{
  struct scanner *dev = NULL;
  struct scanner *s = NULL;
  SANE_Status ret;
 
  DBG (10, "sane_open: start\n");

  if(scanner_devList){
    DBG (15, "sane_open: searching currently attached scanners\n");
  }
  else{
    DBG (15, "sane_open: no scanners currently attached, attaching\n");

    ret = sane_get_devices(NULL,0);
    if(ret != SANE_STATUS_GOOD){
      return ret;
    }
  }

  if(name[0] == 0){
    DBG (15, "sane_open: no device requested, using default\n");
    s = scanner_devList;
  }
  else{
    DBG (15, "sane_open: device %s requested\n", name);
                                                                                
    for (dev = scanner_devList; dev; dev = dev->next) {
      if (strcmp (dev->sane.name, name) == 0
       || strcmp (dev->device_name, name) == 0) { /*always allow sanei devname*/
        s = dev;
        break;
      }
    }
  }

  if (!s) {
    DBG (5, "sane_open: no device found\n");
    return SANE_STATUS_INVAL;
  }

  DBG (15, "sane_open: device %s found\n", s->sane.name);

  *handle = s;

  /* connect the fd so we can talk to scanner */
  ret = connect_fd(s);
  if(ret != SANE_STATUS_GOOD){
    return ret;
  }

  DBG (10, "sane_open: finish\n");

  return SANE_STATUS_GOOD;
}

/*
 * @@ Section 3 - SANE Options functions
 */

/*
 * Returns the options we know.
 *
 * From the SANE spec:
 * This function is used to access option descriptors. The function
 * returns the option descriptor for option number n of the device
 * represented by handle h. Option number 0 is guaranteed to be a
 * valid option. Its value is an integer that specifies the number of
 * options that are available for device handle h (the count includes
 * option 0). If n is not a valid option index, the function returns
 * NULL. The returned option descriptor is guaranteed to remain valid
 * (and at the returned address) until the device is closed.
 */
const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  struct scanner *s = handle;
  int i;
  SANE_Option_Descriptor *opt = &s->opt[option];

  DBG (20, "sane_get_option_descriptor: %d\n", option);

  if ((unsigned) option >= NUM_OPTIONS)
    return NULL;

  /* "Mode" group -------------------------------------------------------- */
  if(option==OPT_STANDARD_GROUP){
    opt->name = SANE_NAME_STANDARD;
    opt->title = SANE_TITLE_STANDARD;
    opt->desc = SANE_DESC_STANDARD;
    opt->type = SANE_TYPE_GROUP;
    opt->constraint_type = SANE_CONSTRAINT_NONE;
  }

  /* source */
  if(option==OPT_SOURCE){
    i=0;
    if(s->has_flatbed){
      s->source_list[i++]=STRING_FLATBED;
    }
    if(s->has_adf){
      s->source_list[i++]=STRING_ADFFRONT;
  
      if(s->has_back){
        s->source_list[i++]=STRING_ADFBACK;
      }
      if(s->has_duplex){
        s->source_list[i++]=STRING_ADFDUPLEX;
      }
    }
    if(s->has_card){
      s->source_list[i++]=STRING_CARDFRONT;
  
      if(s->has_back){
        s->source_list[i++]=STRING_CARDBACK;
      }
      if(s->has_duplex){
        s->source_list[i++]=STRING_CARDDUPLEX;
      }
    }
    s->source_list[i]=NULL;

    opt->name = SANE_NAME_SCAN_SOURCE;
    opt->title = SANE_TITLE_SCAN_SOURCE;
    opt->desc = SANE_DESC_SCAN_SOURCE;
    opt->type = SANE_TYPE_STRING;
    opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
    opt->constraint.string_list = s->source_list;
    opt->size = maxStringSize (opt->constraint.string_list);
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }

  /* scan mode */
  if(option==OPT_MODE){
    i=0;
    if(s->can_monochrome || s->can_grayscale || s->can_color){
      s->mode_list[i++]=STRING_LINEART;
    }
    if(s->can_halftone){
      s->mode_list[i++]=STRING_HALFTONE;
    }
    if(s->can_grayscale || s->can_color){
      s->mode_list[i++]=STRING_GRAYSCALE;
    }
    if(s->can_color){
      s->mode_list[i++]=STRING_COLOR;
    }
    s->mode_list[i]=NULL;
  
    opt->name = SANE_NAME_SCAN_MODE;
    opt->title = SANE_TITLE_SCAN_MODE;
    opt->desc = SANE_DESC_SCAN_MODE;
    opt->type = SANE_TYPE_STRING;
    opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
    opt->constraint.string_list = s->mode_list;
    opt->size = maxStringSize (opt->constraint.string_list);
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }

  /* resolution */
  /* some scanners only support fixed res
   * build a list of possible choices */
  /* we actually only look at the y resolution choices,
   * and interpolate the image data as required for limited x resolutions */
  if(option==OPT_RES){
    i=0;
    if(s->std_res_y[DPI_60] && s->max_y_res >= 60 && s->min_y_res <= 60){
      s->res_list[++i] = 60;
    }
    if(s->std_res_y[DPI_75] && s->max_y_res >= 75 && s->min_y_res <= 75){
      s->res_list[++i] = 75;
    }
    if(s->std_res_y[DPI_100] && s->max_y_res >= 100 && s->min_y_res <= 100){
      s->res_list[++i] = 100;
    }
    if(s->std_res_y[DPI_120] && s->max_y_res >= 120 && s->min_y_res <= 120){
      s->res_list[++i] = 120;
    }
    if(s->std_res_y[DPI_150] && s->max_y_res >= 150 && s->min_y_res <= 150){
      s->res_list[++i] = 150;
    }
    if(s->std_res_y[DPI_160] && s->max_y_res >= 160 && s->min_y_res <= 160){
      s->res_list[++i] = 160;
    }
    if(s->std_res_y[DPI_180] && s->max_y_res >= 180 && s->min_y_res <= 180){
      s->res_list[++i] = 180;
    }
    if(s->std_res_y[DPI_200] && s->max_y_res >= 200 && s->min_y_res <= 200){
      s->res_list[++i] = 200;
    }
    if(s->std_res_y[DPI_240] && s->max_y_res >= 240 && s->min_y_res <= 240){
      s->res_list[++i] = 240;
    }
    if(s->std_res_y[DPI_300] && s->max_y_res >= 300 && s->min_y_res <= 300){
      s->res_list[++i] = 300;
    }
    if(s->std_res_y[DPI_320] && s->max_y_res >= 320 && s->min_y_res <= 320){
      s->res_list[++i] = 320;
    }
    if(s->std_res_y[DPI_400] && s->max_y_res >= 400 && s->min_y_res <= 400){
      s->res_list[++i] = 400;
    }
    if(s->std_res_y[DPI_480] && s->max_y_res >= 480 && s->min_y_res <= 480){
      s->res_list[++i] = 480;
    }
    if(s->std_res_y[DPI_600] && s->max_y_res >= 600 && s->min_y_res <= 600){
      s->res_list[++i] = 600;
    }
    if(s->std_res_y[DPI_800] && s->max_y_res >= 800 && s->min_y_res <= 800){
      s->res_list[++i] = 800;
    }
    if(s->std_res_y[DPI_1200] && s->max_y_res >= 1200 && s->min_y_res <= 1200){
      s->res_list[++i] = 1200;
    }
    s->res_list[0] = i;
  
    opt->name = SANE_NAME_SCAN_RESOLUTION;
    opt->title = SANE_TITLE_SCAN_RESOLUTION;
    opt->desc = SANE_DESC_SCAN_RESOLUTION;
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_DPI;
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  
    if(s->step_y_res){
      s->res_range.min = s->min_y_res;
      s->res_range.max = s->max_y_res;
      s->res_range.quant = s->step_y_res;
      opt->constraint_type = SANE_CONSTRAINT_RANGE;
      opt->constraint.range = &s->res_range;
    }
    else{
      opt->constraint_type = SANE_CONSTRAINT_WORD_LIST;
      opt->constraint.word_list = s->res_list;
    }
  }

  /* "Geometry" group ---------------------------------------------------- */
  if(option==OPT_GEOMETRY_GROUP){
    opt->name = SANE_NAME_GEOMETRY;
    opt->title = SANE_TITLE_GEOMETRY;
    opt->desc = SANE_DESC_GEOMETRY;
    opt->type = SANE_TYPE_GROUP;
    opt->constraint_type = SANE_CONSTRAINT_NONE;
  }

  /* top-left x */
  if(option==OPT_TL_X){
    /* values stored in 1200 dpi units */
    /* must be converted to MM for sane */
    s->tl_x_range.min = SCANNER_UNIT_TO_FIXED_MM(s->min_x);
    s->tl_x_range.max = SCANNER_UNIT_TO_FIXED_MM(get_page_width(s));
    s->tl_x_range.quant = MM_PER_UNIT_FIX;
  
    opt->name = SANE_NAME_SCAN_TL_X;
    opt->title = SANE_TITLE_SCAN_TL_X;
    opt->desc = SANE_DESC_SCAN_TL_X;
    opt->type = SANE_TYPE_FIXED;
    opt->unit = SANE_UNIT_MM;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &(s->tl_x_range);
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }

  /* top-left y */
  if(option==OPT_TL_Y){
    /* values stored in 1200 dpi units */
    /* must be converted to MM for sane */
    s->tl_y_range.min = SCANNER_UNIT_TO_FIXED_MM(s->min_y);
    s->tl_y_range.max = SCANNER_UNIT_TO_FIXED_MM(get_page_height(s));
    s->tl_y_range.quant = MM_PER_UNIT_FIX;
  
    opt->name = SANE_NAME_SCAN_TL_Y;
    opt->title = SANE_TITLE_SCAN_TL_Y;
    opt->desc = SANE_DESC_SCAN_TL_Y;
    opt->type = SANE_TYPE_FIXED;
    opt->unit = SANE_UNIT_MM;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &(s->tl_y_range);
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }

  /* bottom-right x */
  if(option==OPT_BR_X){
    /* values stored in 1200 dpi units */
    /* must be converted to MM for sane */
    s->br_x_range.min = SCANNER_UNIT_TO_FIXED_MM(s->min_x);
    s->br_x_range.max = SCANNER_UNIT_TO_FIXED_MM(get_page_width(s));
    s->br_x_range.quant = MM_PER_UNIT_FIX;
  
    opt->name = SANE_NAME_SCAN_BR_X;
    opt->title = SANE_TITLE_SCAN_BR_X;
    opt->desc = SANE_DESC_SCAN_BR_X;
    opt->type = SANE_TYPE_FIXED;
    opt->unit = SANE_UNIT_MM;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &(s->br_x_range);
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }

  /* bottom-right y */
  if(option==OPT_BR_Y){
    /* values stored in 1200 dpi units */
    /* must be converted to MM for sane */
    s->br_y_range.min = SCANNER_UNIT_TO_FIXED_MM(s->min_y);
    s->br_y_range.max = SCANNER_UNIT_TO_FIXED_MM(get_page_height(s));
    s->br_y_range.quant = MM_PER_UNIT_FIX;
  
    opt->name = SANE_NAME_SCAN_BR_Y;
    opt->title = SANE_TITLE_SCAN_BR_Y;
    opt->desc = SANE_DESC_SCAN_BR_Y;
    opt->type = SANE_TYPE_FIXED;
    opt->unit = SANE_UNIT_MM;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &(s->br_y_range);
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }

  /* page width */
  if(option==OPT_PAGE_WIDTH){
    /* values stored in 1200 dpi units */
    /* must be converted to MM for sane */
    s->paper_x_range.min = SCANNER_UNIT_TO_FIXED_MM(s->min_x);
    s->paper_x_range.max = SCANNER_UNIT_TO_FIXED_MM(s->valid_x);
    s->paper_x_range.quant = MM_PER_UNIT_FIX;

    opt->name = SANE_NAME_PAGE_WIDTH;
    opt->title = SANE_TITLE_PAGE_WIDTH;
    opt->desc = SANE_DESC_PAGE_WIDTH;
    opt->type = SANE_TYPE_FIXED;
    opt->unit = SANE_UNIT_MM;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->paper_x_range;

    if(s->has_adf || s->has_card){
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      if(s->u.source == SOURCE_FLATBED){
        opt->cap |= SANE_CAP_INACTIVE;
      }
    }
    else{
      opt->cap = SANE_CAP_INACTIVE;
    }
  }

  /* page height */
  if(option==OPT_PAGE_HEIGHT){
    /* values stored in 1200 dpi units */
    /* must be converted to MM for sane */
    s->paper_y_range.min = SCANNER_UNIT_TO_FIXED_MM(s->min_y);
    s->paper_y_range.max = SCANNER_UNIT_TO_FIXED_MM(s->max_y);
    s->paper_y_range.quant = MM_PER_UNIT_FIX;

    opt->name = SANE_NAME_PAGE_HEIGHT;
    opt->title = SANE_TITLE_PAGE_HEIGHT;
    opt->desc = SANE_DESC_PAGE_HEIGHT;
    opt->type = SANE_TYPE_FIXED;
    opt->unit = SANE_UNIT_MM;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->paper_y_range;

    if(s->has_adf || s->has_card){
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      if(s->u.source == SOURCE_FLATBED){
        opt->cap |= SANE_CAP_INACTIVE;
      }
    }
    else{
      opt->cap = SANE_CAP_INACTIVE;
    }
  }

  /* "Enhancement" group ------------------------------------------------- */
  if(option==OPT_ENHANCEMENT_GROUP){
    opt->name = SANE_NAME_ENHANCEMENT;
    opt->title = SANE_TITLE_ENHANCEMENT;
    opt->desc = SANE_DESC_ENHANCEMENT;
    opt->type = SANE_TYPE_GROUP;
    opt->constraint_type = SANE_CONSTRAINT_NONE;
  }

  /* brightness */
  if(option==OPT_BRIGHTNESS){
    opt->name = SANE_NAME_BRIGHTNESS;
    opt->title = SANE_TITLE_BRIGHTNESS;
    opt->desc = SANE_DESC_BRIGHTNESS;
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->brightness_range;
    s->brightness_range.quant=1;

    /* some have hardware brightness (always 0 to 255?) */
    /* some use LUT or GT (-127 to +127)*/
    if (s->brightness_steps){
      s->brightness_range.min=-127;
      s->brightness_range.max=127;
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }
    else{
      opt->cap = SANE_CAP_INACTIVE;
    }
  }

  /* contrast */
  if(option==OPT_CONTRAST){
    opt->name = SANE_NAME_CONTRAST;
    opt->title = SANE_TITLE_CONTRAST;
    opt->desc = SANE_DESC_CONTRAST;
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->contrast_range;
    s->contrast_range.quant=1;

    /* some have hardware contrast (always 0 to 255?) */
    /* some use LUT or GT (-127 to +127)*/
    if (s->contrast_steps){
      s->contrast_range.min=-127;
      s->contrast_range.max=127;
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }
    else {
      opt->cap = SANE_CAP_INACTIVE;
    }
  }

  /*threshold*/
  if(option==OPT_THRESHOLD){
    opt->name = SANE_NAME_THRESHOLD;
    opt->title = SANE_TITLE_THRESHOLD;
    opt->desc = SANE_DESC_THRESHOLD;
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->threshold_range;
    s->threshold_range.min=0;
    s->threshold_range.max=s->threshold_steps;
    s->threshold_range.quant=1;

    if (s->threshold_steps){
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      if(s->u.mode != MODE_LINEART){
        opt->cap |= SANE_CAP_INACTIVE;
      }
    }
    else {
      opt->cap = SANE_CAP_INACTIVE;
    }
  }

  if(option==OPT_RIF){
    opt->name = "rif";
    opt->title = "RIF";
    opt->desc = "Reverse image format";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->has_rif)
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    else
      opt->cap = SANE_CAP_INACTIVE;
  }

  /* "Advanced" group ------------------------------------------------------ */
  if(option==OPT_ADVANCED_GROUP){
    opt->name = SANE_NAME_ADVANCED;
    opt->title = SANE_TITLE_ADVANCED;
    opt->desc = SANE_DESC_ADVANCED;
    opt->type = SANE_TYPE_GROUP;
    opt->constraint_type = SANE_CONSTRAINT_NONE;
  }

  /*image compression*/
  if(option==OPT_COMPRESS){
    i=0;
    s->compress_list[i++]=STRING_NONE;

    if(s->has_comp_JPEG){
#ifndef SANE_JPEG_DISABLED
      s->compress_list[i++]=STRING_JPEG;
#endif
    }

    s->compress_list[i]=NULL;

    opt->name = "compression";
    opt->title = "Compression";
    opt->desc = "Enable compressed data. May crash your front-end program";
    opt->type = SANE_TYPE_STRING;
    opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
    opt->constraint.string_list = s->compress_list;
    opt->size = maxStringSize (opt->constraint.string_list);

    if (i > 1){
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      if (s->u.mode != MODE_COLOR && s->u.mode != MODE_GRAYSCALE){
        opt->cap |= SANE_CAP_INACTIVE;
      }
    }
    else
      opt->cap = SANE_CAP_INACTIVE;
  }

  /*image compression arg*/
  if(option==OPT_COMPRESS_ARG){

    opt->name = "compression-arg";
    opt->title = "Compression argument";
    opt->desc = "Level of JPEG compression. 1 is small file, 100 is large file.";
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->compress_arg_range;
    s->compress_arg_range.quant=1;

    if(s->has_comp_JPEG){
      s->compress_arg_range.min=0;
      s->compress_arg_range.max=100;
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

      if(s->compress != COMP_JPEG){
        opt->cap |= SANE_CAP_INACTIVE;
      }
    }
    else
      opt->cap = SANE_CAP_INACTIVE;
  }

  /*double feed by length*/
  if(option==OPT_DF_LENGTH){
    opt->name = "df-length";
    opt->title = "DF by length";
    opt->desc = "Detect double feeds by comparing document lengths";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_NONE;

    if (1)
     opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else
     opt->cap = SANE_CAP_INACTIVE;
  }

  /*double feed by thickness */
  if(option==OPT_DF_THICKNESS){
  
    opt->name = "df-thickness";
    opt->title = "DF by thickness";
    opt->desc = "Detect double feeds using thickness sensor";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_NONE;

    if (1){
     opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    }
    else
     opt->cap = SANE_CAP_INACTIVE;
  }

  /*deskew by roller*/
  if(option==OPT_ROLLERDESKEW){
    opt->name = "rollerdeskew";
    opt->title = "Roller deskew";
    opt->desc = "Request scanner to correct skewed pages mechanically";
    opt->type = SANE_TYPE_BOOL;
    if (1)
     opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else
     opt->cap = SANE_CAP_INACTIVE;
  }

  /*deskew by software*/
  if(option==OPT_SWDESKEW){
    opt->name = "swdeskew";
    opt->title = "Software deskew";
    opt->desc = "Request driver to rotate skewed pages digitally";
    opt->type = SANE_TYPE_BOOL;
    if (1)
     opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else
     opt->cap = SANE_CAP_INACTIVE;
  }

  /*software despeckle radius*/
  if(option==OPT_SWDESPECK){

    opt->name = "swdespeck";
    opt->title = "Software despeckle diameter";
    opt->desc = "Maximum diameter of lone dots to remove from scan";
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->swdespeck_range;
    s->swdespeck_range.quant=1;

    if(1){
      s->swdespeck_range.min=0;
      s->swdespeck_range.max=9;
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }
    else
      opt->cap = SANE_CAP_INACTIVE;
  }

  /*crop by software*/
  if(option==OPT_SWCROP){
    opt->name = "swcrop";
    opt->title = "Software crop";
    opt->desc = "Request driver to remove border from pages digitally";
    opt->type = SANE_TYPE_BOOL;
    if (1)
     opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else
     opt->cap = SANE_CAP_INACTIVE;
  }

  /*staple detection*/
  if(option==OPT_STAPLEDETECT){
    opt->name = "stapledetect";
    opt->title = "Staple detect";
    opt->desc = "Request scanner to halt if stapled pages are detected";
    opt->type = SANE_TYPE_BOOL;
    if (1)
     opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else
     opt->cap = SANE_CAP_INACTIVE;
  }

  /*dropout color front*/
  if(option==OPT_DROPOUT_COLOR_F){
    s->do_color_list[0] = STRING_NONE;
    s->do_color_list[1] = STRING_RED;
    s->do_color_list[2] = STRING_GREEN;
    s->do_color_list[3] = STRING_BLUE;
    s->do_color_list[4] = STRING_EN_RED;
    s->do_color_list[5] = STRING_EN_GREEN;
    s->do_color_list[6] = STRING_EN_BLUE;
    s->do_color_list[7] = NULL;
  
    opt->name = "dropout-front";
    opt->title = "Dropout color front";
    opt->desc = "One-pass scanners use only one color during gray or binary scanning, useful for colored paper or ink";
    opt->type = SANE_TYPE_STRING;
    opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
    opt->constraint.string_list = s->do_color_list;
    opt->size = maxStringSize (opt->constraint.string_list);

    if (1){
      opt->cap = SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT|SANE_CAP_ADVANCED;
      if(s->u.mode == MODE_COLOR)
        opt->cap |= SANE_CAP_INACTIVE;
    }
    else
      opt->cap = SANE_CAP_INACTIVE;
  }

  /*dropout color back*/
  if(option==OPT_DROPOUT_COLOR_B){
    s->do_color_list[0] = STRING_NONE;
    s->do_color_list[1] = STRING_RED;
    s->do_color_list[2] = STRING_GREEN;
    s->do_color_list[3] = STRING_BLUE;
    s->do_color_list[4] = STRING_EN_RED;
    s->do_color_list[5] = STRING_EN_GREEN;
    s->do_color_list[6] = STRING_EN_BLUE;
    s->do_color_list[7] = NULL;
  
    opt->name = "dropout-back";
    opt->title = "Dropout color back";
    opt->desc = "One-pass scanners use only one color during gray or binary scanning, useful for colored paper or ink";
    opt->type = SANE_TYPE_STRING;
    opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
    opt->constraint.string_list = s->do_color_list;
    opt->size = maxStringSize (opt->constraint.string_list);

    if (1){
      opt->cap = SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT|SANE_CAP_ADVANCED;
      if(s->u.mode == MODE_COLOR)
        opt->cap |= SANE_CAP_INACTIVE;
    }
    else
      opt->cap = SANE_CAP_INACTIVE;
  }

  /*buffer mode*/
  if(option==OPT_BUFFERMODE){
    opt->name = "buffermode";
    opt->title = "Buffer mode";
    opt->desc = "Request scanner to read pages async into internal memory";
    opt->type = SANE_TYPE_BOOL;
    if (s->has_buffer)
     opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    else
     opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_SIDE){
    opt->name = "side";
    opt->title = "Duplex side";
    opt->desc = "Tells which side (0=front, 1=back) of a duplex scan the next call to sane_read will return.";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    opt->size = sizeof(SANE_Word);
    opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    opt->constraint_type = SANE_CONSTRAINT_NONE;
  }

  /* "Sensor" group ------------------------------------------------------ */
  if(option==OPT_SENSOR_GROUP){
    opt->name = SANE_NAME_SENSORS;
    opt->title = SANE_TITLE_SENSORS;
    opt->desc = SANE_DESC_SENSORS;
    opt->type = SANE_TYPE_GROUP;
    opt->constraint_type = SANE_CONSTRAINT_NONE;
  }

  if(option==OPT_START){
    opt->name = "start";
    opt->title = "Start/1 button";
    opt->desc = "Big green or small 1 button";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    if(!s->can_read_panel)
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_STOP){
    opt->name = "stop";
    opt->title = "Stop/2 button";
    opt->desc = "Small orange or small 2 button";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    if(!s->can_read_panel)
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_BUTT3){
    opt->name = "button-3";
    opt->title = "3 button";
    opt->desc = "Small 3 button";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    if(!s->can_read_panel)
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_NEWFILE){
    opt->name = "newfile";
    opt->title = "New File button";
    opt->desc = "New File button";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    if(!s->can_read_panel)
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_COUNTONLY){
    opt->name = "countonly";
    opt->title = "Count Only button";
    opt->desc = "Count Only button";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    if(!s->can_read_panel)
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_BYPASSMODE){
    opt->name = "bypassmode";
    opt->title = "Bypass Mode button";
    opt->desc = "Bypass Mode button";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    if(!s->can_read_panel)
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_COUNTER){
    opt->name = "counter";
    opt->title = "Counter";
    opt->desc = "Scan counter";
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_NONE;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->counter_range;
    s->counter_range.min=0;
    s->counter_range.max=500;
    s->counter_range.quant=1;

    if (s->can_read_panel && s->has_counter)
     opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    else
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_ADF_LOADED){
    opt->name = "adf-loaded";
    opt->title = "ADF Loaded";
    opt->desc = "Paper available in ADF input hopper";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    if(!s->can_read_sensors)
      opt->cap = SANE_CAP_INACTIVE;
  }

  if(option==OPT_CARD_LOADED){
    opt->name = "card-loaded";
    opt->title = "Card Loaded";
    opt->desc = "Paper available in card reader";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    opt->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
    if(!s->can_read_sensors || !s->has_card)
      opt->cap = SANE_CAP_INACTIVE;
  }

  return opt;
}

/**
 * Gets or sets an option value.
 * 
 * From the SANE spec:
 * This function is used to set or inquire the current value of option
 * number n of the device represented by handle h. The manner in which
 * the option is controlled is specified by parameter action. The
 * possible values of this parameter are described in more detail
 * below.  The value of the option is passed through argument val. It
 * is a pointer to the memory that holds the option value. The memory
 * area pointed to by v must be big enough to hold the entire option
 * value (determined by member size in the corresponding option
 * descriptor).
 * 
 * The only exception to this rule is that when setting the value of a
 * string option, the string pointed to by argument v may be shorter
 * since the backend will stop reading the option value upon
 * encountering the first NUL terminator in the string. If argument i
 * is not NULL, the value of *i will be set to provide details on how
 * well the request has been met.
 */
SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
                     SANE_Action action, void *val, SANE_Int * info)
{
  struct scanner *s = (struct scanner *) handle;
  SANE_Int dummy = 0;
  SANE_Status ret = SANE_STATUS_GOOD;

  /* Make sure that all those statements involving *info cannot break (better
   * than having to do "if (info) ..." everywhere!)
   */
  if (info == 0)
    info = &dummy;

  if (option >= NUM_OPTIONS) {
    DBG (5, "sane_control_option: %d too big\n", option);
    return SANE_STATUS_INVAL;
  }

  if (!SANE_OPTION_IS_ACTIVE (s->opt[option].cap)) {
    DBG (5, "sane_control_option: %d inactive\n", option);
    return SANE_STATUS_INVAL;
  }

  /*
   * SANE_ACTION_GET_VALUE: We have to find out the current setting and
   * return it in a human-readable form (often, text).
   */
  if (action == SANE_ACTION_GET_VALUE) {
      SANE_Word * val_p = (SANE_Word *) val;

      DBG (20, "sane_control_option: get value for '%s' (%d)\n", s->opt[option].name,option);

      switch (option) {

        case OPT_NUM_OPTS:
          *val_p = NUM_OPTIONS;
          return SANE_STATUS_GOOD;

        case OPT_SOURCE:
          if(s->u.source == SOURCE_FLATBED){
            strcpy (val, STRING_FLATBED);
          }
          else if(s->u.source == SOURCE_ADF_FRONT){
            strcpy (val, STRING_ADFFRONT);
          }
          else if(s->u.source == SOURCE_ADF_BACK){
            strcpy (val, STRING_ADFBACK);
          }
          else if(s->u.source == SOURCE_ADF_DUPLEX){
            strcpy (val, STRING_ADFDUPLEX);
          }
          else if(s->u.source == SOURCE_CARD_FRONT){
            strcpy (val, STRING_CARDFRONT);
          }
          else if(s->u.source == SOURCE_CARD_BACK){
            strcpy (val, STRING_CARDBACK);
          }
          else if(s->u.source == SOURCE_CARD_DUPLEX){
            strcpy (val, STRING_CARDDUPLEX);
          }
          return SANE_STATUS_GOOD;

        case OPT_MODE:
          if(s->u.mode == MODE_LINEART){
            strcpy (val, STRING_LINEART);
          }
          else if(s->u.mode == MODE_HALFTONE){
            strcpy (val, STRING_HALFTONE);
          }
          else if(s->u.mode == MODE_GRAYSCALE){
            strcpy (val, STRING_GRAYSCALE);
          }
          else if(s->u.mode == MODE_COLOR){
            strcpy (val, STRING_COLOR);
          }
          return SANE_STATUS_GOOD;

        case OPT_RES:
          *val_p = s->u.dpi_x;
          return SANE_STATUS_GOOD;

        case OPT_TL_X:
          *val_p = SCANNER_UNIT_TO_FIXED_MM(s->u.tl_x);
          return SANE_STATUS_GOOD;

        case OPT_TL_Y:
          *val_p = SCANNER_UNIT_TO_FIXED_MM(s->u.tl_y);
          return SANE_STATUS_GOOD;

        case OPT_BR_X:
          *val_p = SCANNER_UNIT_TO_FIXED_MM(s->u.br_x);
          return SANE_STATUS_GOOD;

        case OPT_BR_Y:
          *val_p = SCANNER_UNIT_TO_FIXED_MM(s->u.br_y);
          return SANE_STATUS_GOOD;

        case OPT_PAGE_WIDTH:
          *val_p = SCANNER_UNIT_TO_FIXED_MM(s->u.page_x);
          return SANE_STATUS_GOOD;

        case OPT_PAGE_HEIGHT:
          *val_p = SCANNER_UNIT_TO_FIXED_MM(s->u.page_y);
          return SANE_STATUS_GOOD;

        case OPT_BRIGHTNESS:
          *val_p = s->brightness;
          return SANE_STATUS_GOOD;

        case OPT_CONTRAST:
          *val_p = s->contrast;
          return SANE_STATUS_GOOD;

        case OPT_THRESHOLD:
          *val_p = s->threshold;
          return SANE_STATUS_GOOD;

        case OPT_RIF:
          *val_p = s->rif;
          return SANE_STATUS_GOOD;

        /* Advanced Group */
        case OPT_COMPRESS:
          if(s->compress == COMP_JPEG){
            strcpy (val, STRING_JPEG);
          }
          else{
            strcpy (val, STRING_NONE);
          }
          return SANE_STATUS_GOOD;

        case OPT_COMPRESS_ARG:
          *val_p = s->compress_arg;
          return SANE_STATUS_GOOD;

        case OPT_DF_LENGTH:
          *val_p = s->df_length;
          return SANE_STATUS_GOOD;

        case OPT_DF_THICKNESS:
          *val_p = s->df_thickness;
          return SANE_STATUS_GOOD;

        case OPT_ROLLERDESKEW:
          *val_p = s->rollerdeskew;
          return SANE_STATUS_GOOD;

        case OPT_SWDESKEW:
          *val_p = s->swdeskew;
          return SANE_STATUS_GOOD;

        case OPT_SWDESPECK:
          *val_p = s->swdespeck;
          return SANE_STATUS_GOOD;

        case OPT_SWCROP:
          *val_p = s->swcrop;
          return SANE_STATUS_GOOD;

        case OPT_STAPLEDETECT:
          *val_p = s->stapledetect;
          return SANE_STATUS_GOOD;

        case OPT_DROPOUT_COLOR_F:
          switch (s->dropout_color_f) {
            case COLOR_NONE:
              strcpy (val, STRING_NONE);
              break;
            case COLOR_RED:
              strcpy (val, STRING_RED);
              break;
            case COLOR_GREEN:
              strcpy (val, STRING_GREEN);
              break;
            case COLOR_BLUE:
              strcpy (val, STRING_BLUE);
              break;
            case COLOR_EN_RED:
              strcpy (val, STRING_EN_RED);
              break;
            case COLOR_EN_GREEN:
              strcpy (val, STRING_EN_GREEN);
              break;
            case COLOR_EN_BLUE:
              strcpy (val, STRING_EN_BLUE);
              break;
          }
          return SANE_STATUS_GOOD;

        case OPT_DROPOUT_COLOR_B:
          switch (s->dropout_color_b) {
            case COLOR_NONE:
              strcpy (val, STRING_NONE);
              break;
            case COLOR_RED:
              strcpy (val, STRING_RED);
              break;
            case COLOR_GREEN:
              strcpy (val, STRING_GREEN);
              break;
            case COLOR_BLUE:
              strcpy (val, STRING_BLUE);
              break;
            case COLOR_EN_RED:
              strcpy (val, STRING_EN_RED);
              break;
            case COLOR_EN_GREEN:
              strcpy (val, STRING_EN_GREEN);
              break;
            case COLOR_EN_BLUE:
              strcpy (val, STRING_EN_BLUE);
              break;
          }
          return SANE_STATUS_GOOD;

        case OPT_BUFFERMODE:
          *val_p = s->buffermode;
          return SANE_STATUS_GOOD;

        case OPT_SIDE:
          *val_p = s->side;
          return SANE_STATUS_GOOD;

        /* Sensor Group */
        case OPT_START:
          ret = read_panel(s,OPT_START);
          *val_p = s->panel_start;
          return ret;

        case OPT_STOP:
          ret = read_panel(s,OPT_STOP);
          *val_p = s->panel_stop;
          return ret;

        case OPT_BUTT3:
          ret = read_panel(s,OPT_BUTT3);
          *val_p = s->panel_butt3;
          return ret;

        case OPT_NEWFILE:
          ret = read_panel(s,OPT_NEWFILE);
          *val_p = s->panel_new_file;
          return ret;

        case OPT_COUNTONLY:
          ret = read_panel(s,OPT_COUNTONLY);
          *val_p = s->panel_count_only;
          return ret;

        case OPT_BYPASSMODE:
          ret = read_panel(s,OPT_BYPASSMODE);
          *val_p = s->panel_bypass_mode;
          return ret;

        case OPT_COUNTER:
          ret = read_panel(s,OPT_COUNTER);
          *val_p = s->panel_counter;
          return ret;

        case OPT_ADF_LOADED:
          ret = read_sensors(s,OPT_ADF_LOADED);
          *val_p = s->sensor_adf_loaded;
          return ret;

        case OPT_CARD_LOADED:
          ret = read_sensors(s,OPT_CARD_LOADED);
          *val_p = s->sensor_card_loaded;
          return ret;
      }
  }
  else if (action == SANE_ACTION_SET_VALUE) {
      int tmp;
      SANE_Word val_c;
      SANE_Status status;

      DBG (20, "sane_control_option: set value for '%s' (%d)\n", s->opt[option].name,option);

      if ( s->started ) {
        DBG (5, "sane_control_option: cant set, device busy\n");
        return SANE_STATUS_DEVICE_BUSY;
      }

      if (!SANE_OPTION_IS_SETTABLE (s->opt[option].cap)) {
        DBG (5, "sane_control_option: not settable\n");
        return SANE_STATUS_INVAL;
      }

      status = sanei_constrain_value (s->opt + option, val, info);
      if (status != SANE_STATUS_GOOD) {
        DBG (5, "sane_control_option: bad value\n");
        return status;
      }

      /* may have been changed by constrain, so dont copy until now */
      val_c = *(SANE_Word *)val;

      /*
       * Note - for those options which can assume one of a list of
       * valid values, we can safely assume that they will have
       * exactly one of those values because that's what
       * sanei_constrain_value does. Hence no "else: invalid" branches
       * below.
       */
      switch (option) {
 
        /* Mode Group */
        case OPT_SOURCE:
          if (!strcmp (val, STRING_ADFFRONT)) {
            tmp = SOURCE_ADF_FRONT;
          }
          else if (!strcmp (val, STRING_ADFBACK)) {
            tmp = SOURCE_ADF_BACK;
          }
          else if (!strcmp (val, STRING_ADFDUPLEX)) {
            tmp = SOURCE_ADF_DUPLEX;
          }
          else if (!strcmp (val, STRING_CARDFRONT)) {
            tmp = SOURCE_CARD_FRONT;
          }
          else if (!strcmp (val, STRING_CARDBACK)) {
            tmp = SOURCE_CARD_BACK;
          }
          else if (!strcmp (val, STRING_CARDDUPLEX)) {
            tmp = SOURCE_CARD_DUPLEX;
          }
          else{
            tmp = SOURCE_FLATBED;
          }

          if (s->u.source == tmp) 
              return SANE_STATUS_GOOD;

          s->u.source = tmp;

          *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

        case OPT_MODE:
          if (!strcmp (val, STRING_LINEART)) {
            tmp = MODE_LINEART;
          }
          else if (!strcmp (val, STRING_HALFTONE)) {
            tmp = MODE_HALFTONE;
          }
          else if (!strcmp (val, STRING_GRAYSCALE)) {
            tmp = MODE_GRAYSCALE;
          }
          else{
            tmp = MODE_COLOR;
          }

          if (tmp == s->u.mode)
              return SANE_STATUS_GOOD;

          s->u.mode = tmp;

          *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

        case OPT_RES:

          if (s->u.dpi_x == val_c && s->u.dpi_y == val_c) 
              return SANE_STATUS_GOOD;

          s->u.dpi_x = val_c;
          s->u.dpi_y = val_c;

          *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

        /* Geometry Group */
        case OPT_TL_X:
          if (s->u.tl_x == FIXED_MM_TO_SCANNER_UNIT(val_c))
              return SANE_STATUS_GOOD;

          s->u.tl_x = FIXED_MM_TO_SCANNER_UNIT(val_c);

          *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

        case OPT_TL_Y:
          if (s->u.tl_y == FIXED_MM_TO_SCANNER_UNIT(val_c))
              return SANE_STATUS_GOOD;

          s->u.tl_y = FIXED_MM_TO_SCANNER_UNIT(val_c);

          *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

        case OPT_BR_X:
          if (s->u.br_x == FIXED_MM_TO_SCANNER_UNIT(val_c))
              return SANE_STATUS_GOOD;

          s->u.br_x = FIXED_MM_TO_SCANNER_UNIT(val_c);

          *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

        case OPT_BR_Y:
          if (s->u.br_y == FIXED_MM_TO_SCANNER_UNIT(val_c))
              return SANE_STATUS_GOOD;

          s->u.br_y = FIXED_MM_TO_SCANNER_UNIT(val_c);

          *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

        case OPT_PAGE_WIDTH:
          if (s->u.page_x == FIXED_MM_TO_SCANNER_UNIT(val_c))
              return SANE_STATUS_GOOD;

          s->u.page_x = FIXED_MM_TO_SCANNER_UNIT(val_c);

          *info |= SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

        case OPT_PAGE_HEIGHT:
          if (s->u.page_y == FIXED_MM_TO_SCANNER_UNIT(val_c))
              return SANE_STATUS_GOOD;

          s->u.page_y = FIXED_MM_TO_SCANNER_UNIT(val_c);

          *info |= SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

        /* Enhancement Group */
        case OPT_BRIGHTNESS:
          s->brightness = val_c;
          return SANE_STATUS_GOOD;

        case OPT_CONTRAST:
          s->contrast = val_c;
          return SANE_STATUS_GOOD;

        case OPT_THRESHOLD:
          s->threshold = val_c;
          return SANE_STATUS_GOOD;

        case OPT_RIF:
          s->rif = val_c;
          return SANE_STATUS_GOOD;

        /* Advanced Group */
        case OPT_COMPRESS:
          if (!strcmp (val, STRING_JPEG)) {
            s->compress = COMP_JPEG;
          }
          else{
            s->compress = COMP_NONE;
          }
          return SANE_STATUS_GOOD;

        case OPT_COMPRESS_ARG:
          s->compress_arg = val_c;
          return SANE_STATUS_GOOD;

        case OPT_DF_LENGTH:
          s->df_length = val_c;
          return SANE_STATUS_GOOD;

        case OPT_DF_THICKNESS:
          s->df_thickness = val_c;
          return SANE_STATUS_GOOD;

        case OPT_ROLLERDESKEW:
          s->rollerdeskew = val_c;
          return SANE_STATUS_GOOD;

        case OPT_SWDESKEW:
          s->swdeskew = val_c;
          return SANE_STATUS_GOOD;

        case OPT_SWDESPECK:
          s->swdespeck = val_c;
          return SANE_STATUS_GOOD;

        case OPT_SWCROP:
          s->swcrop = val_c;
          return SANE_STATUS_GOOD;

        case OPT_STAPLEDETECT:
          s->stapledetect = val_c;
          return SANE_STATUS_GOOD;

        case OPT_DROPOUT_COLOR_F:
          if (!strcmp(val, STRING_NONE))
            s->dropout_color_f = COLOR_NONE;
          else if (!strcmp(val, STRING_RED))
            s->dropout_color_f = COLOR_RED;
          else if (!strcmp(val, STRING_GREEN))
            s->dropout_color_f = COLOR_GREEN;
          else if (!strcmp(val, STRING_BLUE))
            s->dropout_color_f = COLOR_BLUE;
          else if (!strcmp(val, STRING_EN_RED))
            s->dropout_color_f = COLOR_EN_RED;
          else if (!strcmp(val, STRING_EN_GREEN))
            s->dropout_color_f = COLOR_EN_GREEN;
          else if (!strcmp(val, STRING_EN_BLUE))
            s->dropout_color_f = COLOR_EN_BLUE;
          return SANE_STATUS_GOOD;

        case OPT_DROPOUT_COLOR_B:
          if (!strcmp(val, STRING_NONE))
            s->dropout_color_b = COLOR_NONE;
          else if (!strcmp(val, STRING_RED))
            s->dropout_color_b = COLOR_RED;
          else if (!strcmp(val, STRING_GREEN))
            s->dropout_color_b = COLOR_GREEN;
          else if (!strcmp(val, STRING_BLUE))
            s->dropout_color_b = COLOR_BLUE;
          else if (!strcmp(val, STRING_EN_RED))
            s->dropout_color_b = COLOR_EN_RED;
          else if (!strcmp(val, STRING_EN_GREEN))
            s->dropout_color_b = COLOR_EN_GREEN;
          else if (!strcmp(val, STRING_EN_BLUE))
            s->dropout_color_b = COLOR_EN_BLUE;
          return SANE_STATUS_GOOD;

        case OPT_BUFFERMODE:
          s->buffermode = val_c;
          return SANE_STATUS_GOOD;

      }
  }                           /* else */

  return SANE_STATUS_INVAL;
}

static SANE_Status
ssm_buffer (struct scanner *s)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  DBG (10, "ssm_buffer: start\n");

  if(s->has_ssm){
  
    unsigned char cmd[SET_SCAN_MODE_len];
    size_t cmdLen = SET_SCAN_MODE_len;
  
    unsigned char out[SSM_PAY_len];
    size_t outLen = SSM_PAY_len;
  
    memset(cmd,0,cmdLen);
    set_SCSI_opcode(cmd, SET_SCAN_MODE_code);
    set_SSM_pf(cmd, 1);
    set_SSM_pay_len(cmd, outLen);
  
    memset(out,0,outLen);
    if(s->has_ssm_pay_head_len){
      set_SSM_pay_head_len(out, SSM_PAY_HEAD_len);
    }
    set_SSM_page_code(out, SM_pc_buffer);
    set_SSM_page_len(out, SSM_PAGE_len);
  
    if(s->s.source == SOURCE_ADF_DUPLEX || s->s.source == SOURCE_CARD_DUPLEX){
      set_SSM_BUFF_duplex(out, 1);
    }
    if(s->s.source == SOURCE_FLATBED){
      set_SSM_BUFF_fb(out, 1);
    }
    else if(s->s.source >= SOURCE_CARD_FRONT){
      set_SSM_BUFF_card(out, 1);
    }
    if(s->buffermode){
      set_SSM_BUFF_async(out, 1);
    }
    if(0){
      set_SSM_BUFF_ald(out, 1);
    }
    if(0){
      set_SSM_BUFF_unk(out,1);
    }
  
    ret = do_cmd (
        s, 1, 0,
        cmd, cmdLen,
        out, outLen,
        NULL, NULL
    );
  }

  else if(s->has_ssm2){

    unsigned char cmd[SET_SCAN_MODE2_len];
    size_t cmdLen = SET_SCAN_MODE2_len;
  
    unsigned char out[SSM2_PAY_len];
    size_t outLen = SSM2_PAY_len;
  
    memset(cmd,0,cmdLen);
    set_SCSI_opcode(cmd, SET_SCAN_MODE2_code);
    set_SSM2_page_code(cmd, SM2_pc_buffer);
    set_SSM2_pay_len(cmd, outLen);
  
    memset(out,0,outLen);
    set_SSM2_BUFF_unk(out, !s->buffermode);
    set_SSM2_BUFF_unk2(out, 0x40);
    set_SSM2_BUFF_sync(out, !s->buffermode);
  
    ret = do_cmd (
        s, 1, 0,
        cmd, cmdLen,
        out, outLen,
        NULL, NULL
    );
  }

  else{
    DBG (10, "ssm_buffer: unsupported\n");
  }

  DBG (10, "ssm_buffer: finish\n");

  return ret;
}

static SANE_Status
ssm_df (struct scanner *s)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  DBG (10, "ssm_df: start\n");
  
  if(!s->has_df){
    DBG (10, "ssm_df: unsupported, finishing\n");
    return ret;
  }
  
  if(s->has_ssm){

    unsigned char cmd[SET_SCAN_MODE_len];
    size_t cmdLen = SET_SCAN_MODE_len;
  
    unsigned char out[SSM_PAY_len];
    size_t outLen = SSM_PAY_len;
  
    memset(cmd,0,cmdLen);
    set_SCSI_opcode(cmd, SET_SCAN_MODE_code);
    set_SSM_pf(cmd, 1);
    set_SSM_pay_len(cmd, outLen);
  
    memset(out,0,outLen);
    if(s->has_ssm_pay_head_len){
      set_SSM_pay_head_len(out, SSM_PAY_HEAD_len);
    }
    set_SSM_page_code(out, SM_pc_df);
    set_SSM_page_len(out, SSM_PAGE_len);
  
    /* deskew by roller */
    if(s->rollerdeskew){
      set_SSM_DF_deskew_roll(out, 1);
    }
    
    /* staple detection */
    if(s->stapledetect){
      set_SSM_DF_staple(out, 1);
    }
  
    /* thickness */
    if(s->df_thickness){
      set_SSM_DF_thick(out, 1);
    }
    
    /* length */
    if(s->df_length){
      set_SSM_DF_len(out, 1);
    }
  
    ret = do_cmd (
        s, 1, 0,
        cmd, cmdLen,
        out, outLen,
        NULL, NULL
    );

  }

  else if(s->has_ssm2){

    unsigned char cmd[SET_SCAN_MODE2_len];
    size_t cmdLen = SET_SCAN_MODE2_len;
  
    unsigned char out[SSM2_PAY_len];
    size_t outLen = SSM2_PAY_len;

    /* send ultrasonic offsets first */
    if(s->df_thickness && s->has_df_ultra){
      memset(cmd,0,cmdLen);
      set_SCSI_opcode(cmd, SET_SCAN_MODE2_code);
      set_SSM2_page_code(cmd, SM2_pc_ultra);
      set_SSM2_pay_len(cmd, outLen);
    
      memset(out,0,outLen);
      set_SSM2_ULTRA_top(out, 0);
      set_SSM2_ULTRA_bot(out, 0);
  
      ret = do_cmd (
          s, 1, 0,
          cmd, cmdLen,
          out, outLen,
          NULL, NULL
      );
    }

    memset(cmd,0,cmdLen);
    set_SCSI_opcode(cmd, SET_SCAN_MODE2_code);
    set_SSM2_page_code(cmd, SM2_pc_df);
    set_SSM2_pay_len(cmd, outLen);
  
    memset(out,0,outLen);
  
    /* thickness */
    if(s->df_thickness){
      set_SSM2_DF_thick(out, 1);
    }
    
    /* length */
    if(s->df_length){
      set_SSM2_DF_len(out, 1);
    }
  
    ret = do_cmd (
        s, 1, 0,
        cmd, cmdLen,
        out, outLen,
        NULL, NULL
    );

  }

  else{
    DBG (10, "ssm_df: unsupported\n");
  }

  DBG (10, "ssm_df: finish\n");

  return ret;
}

static SANE_Status
ssm_do (struct scanner *s)
{
  SANE_Status ret = SANE_STATUS_GOOD;

 DBG (10, "ssm_do: start\n");

 if(!s->can_color){
   DBG (10, "ssm_do: unsupported, finishing\n");
   return ret;
 }
  
 if(s->has_ssm){

    unsigned char cmd[SET_SCAN_MODE_len];
    size_t cmdLen = SET_SCAN_MODE_len;
  
    unsigned char out[SSM_PAY_len];
    size_t outLen = SSM_PAY_len;
  
    memset(cmd,0,cmdLen);
    set_SCSI_opcode(cmd, SET_SCAN_MODE_code);
    set_SSM_pf(cmd, 1);
    set_SSM_pay_len(cmd, outLen);
  
    memset(out,0,outLen);
    if(s->has_ssm_pay_head_len){
      set_SSM_pay_head_len(out, SSM_PAY_HEAD_len);
    }
    set_SSM_page_code(out, SM_pc_dropout);
    set_SSM_page_len(out, SSM_PAGE_len);
  
    set_SSM_DO_unk1(out, 0x03);
  
    switch(s->dropout_color_f){
      case COLOR_RED:
        set_SSM_DO_unk2(out, 0x05);
        set_SSM_DO_f_do(out,SSM_DO_red);
        break;
      case COLOR_GREEN:
        set_SSM_DO_unk2(out, 0x05);
        set_SSM_DO_f_do(out,SSM_DO_green);
        break;
      case COLOR_BLUE:
        set_SSM_DO_unk2(out, 0x05);
        set_SSM_DO_f_do(out,SSM_DO_blue);
        break;
      case COLOR_EN_RED:
        set_SSM_DO_unk2(out, 0x05);
        set_SSM_DO_f_en(out,SSM_DO_red);
        break;
      case COLOR_EN_GREEN:
        set_SSM_DO_unk2(out, 0x05);
        set_SSM_DO_f_en(out,SSM_DO_green);
        break;
      case COLOR_EN_BLUE:
        set_SSM_DO_unk2(out, 0x05);
        set_SSM_DO_f_en(out,SSM_DO_blue);
        break;
    }
  
    switch(s->dropout_color_b){
      case COLOR_RED:
        set_SSM_DO_unk2(out, 0x05);
        set_SSM_DO_b_do(out,SSM_DO_red);
        break;
      case COLOR_GREEN:
        set_SSM_DO_unk2(out, 0x05);
        set_SSM_DO_b_do(out,SSM_DO_green);
        break;
      case COLOR_BLUE:
        set_SSM_DO_unk2(out, 0x05);
        set_SSM_DO_b_do(out,SSM_DO_blue);
        break;
      case COLOR_EN_RED:
        set_SSM_DO_unk2(out, 0x05);
        set_SSM_DO_b_en(out,SSM_DO_red);
        break;
      case COLOR_EN_GREEN:
        set_SSM_DO_unk2(out, 0x05);
        set_SSM_DO_b_en(out,SSM_DO_green);
        break;
      case COLOR_EN_BLUE:
        set_SSM_DO_unk2(out, 0x05);
        set_SSM_DO_b_en(out,SSM_DO_blue);
        break;
    }
  
    ret = do_cmd (
        s, 1, 0,
        cmd, cmdLen,
        out, outLen,
        NULL, NULL
    );

  }

  else if(s->has_ssm2){

    unsigned char cmd[SET_SCAN_MODE2_len];
    size_t cmdLen = SET_SCAN_MODE2_len;
  
    unsigned char out[SSM2_PAY_len];
    size_t outLen = SSM2_PAY_len;
  
    memset(cmd,0,cmdLen);
    set_SCSI_opcode(cmd, SET_SCAN_MODE2_code);
    set_SSM2_page_code(cmd, SM2_pc_dropout);
    set_SSM2_pay_len(cmd, outLen);
  
    memset(out,0,outLen);
  
    switch(s->dropout_color_f){
      case COLOR_RED:
        set_SSM2_DO_do(out,SSM_DO_red);
        break;
      case COLOR_GREEN:
        set_SSM2_DO_do(out,SSM_DO_green);
        break;
      case COLOR_BLUE:
        set_SSM2_DO_do(out,SSM_DO_blue);
        break;
      case COLOR_EN_RED:
        set_SSM2_DO_en(out,SSM_DO_red);
        break;
      case COLOR_EN_GREEN:
        set_SSM2_DO_en(out,SSM_DO_green);
        break;
      case COLOR_EN_BLUE:
        set_SSM2_DO_en(out,SSM_DO_blue);
        break;
    }

    ret = do_cmd (
        s, 1, 0,
        cmd, cmdLen,
        out, outLen,
        NULL, NULL
    );
  }

  else{
    DBG (10, "ssm_do: unsupported\n");
  }

  DBG (10, "ssm_do: finish\n");

  return ret;
}

static SANE_Status
read_sensors(struct scanner *s,SANE_Int option)
{
  SANE_Status ret=SANE_STATUS_GOOD;

  unsigned char cmd[READ_len];
  size_t cmdLen = READ_len;

  unsigned char in[R_SENSORS_len];
  size_t inLen = R_SENSORS_len;

  DBG (10, "read_sensors: start %d\n", option);
 
  if(!s->can_read_sensors){
    DBG (10, "read_sensors: unsupported, finishing\n");
    return ret;
  }

  /* only run this if frontend has already read the last time we got it */
  /* or if we don't care for such bookkeeping (private use) */
  if (!option || !s->sensors_read[option-OPT_ADF_LOADED]) {

    DBG (15, "read_sensors: running\n");

    memset(cmd,0,cmdLen);
    set_SCSI_opcode(cmd, READ_code);
    set_R_datatype_code (cmd, SR_datatype_sensors);
    set_R_xfer_length (cmd, inLen);
    
    ret = do_cmd (
      s, 1, 0,
      cmd, cmdLen,
      NULL, 0,
      in, &inLen
    );
    
    if (ret == SANE_STATUS_GOOD || ret == SANE_STATUS_EOF) {
      /*set flags indicating there is data to read*/
      memset(s->sensors_read,1,sizeof(s->sensors_read));

      s->sensor_adf_loaded = get_R_SENSORS_adf(in);
      s->sensor_card_loaded = get_R_SENSORS_card(in);

      ret = SANE_STATUS_GOOD;
    }
  }
  
  if(option)
    s->sensors_read[option-OPT_ADF_LOADED] = 0;

  DBG (10, "read_sensors: finish\n");
  
  return ret;
}

static SANE_Status
read_panel(struct scanner *s,SANE_Int option)
{
  SANE_Status ret=SANE_STATUS_GOOD;

  unsigned char cmd[READ_len];
  size_t cmdLen = READ_len;

  unsigned char in[R_PANEL_len];
  size_t inLen = R_PANEL_len;

  DBG (10, "read_panel: start %d\n", option);
 
  if(!s->can_read_panel){
    DBG (10, "read_panel: unsupported, finishing\n");
    return ret;
  }

  /* only run this if frontend has already read the last time we got it */
  /* or if we don't care for such bookkeeping (private use) */
  if (!option || !s->panel_read[option-OPT_START]) {

    DBG (15, "read_panel: running\n");

    memset(cmd,0,cmdLen);
    set_SCSI_opcode(cmd, READ_code);
    set_R_datatype_code (cmd, SR_datatype_panel);
    set_R_xfer_length (cmd, inLen);
    
    ret = do_cmd (
      s, 1, 0,
      cmd, cmdLen,
      NULL, 0,
      in, &inLen
    );
    
    if (ret == SANE_STATUS_GOOD || ret == SANE_STATUS_EOF) {
      /*set flags indicating there is data to read*/
      memset(s->panel_read,1,sizeof(s->panel_read));

      s->panel_start = get_R_PANEL_start(in);
      s->panel_stop = get_R_PANEL_stop(in);
      s->panel_butt3 = get_R_PANEL_butt3(in);
      s->panel_new_file = get_R_PANEL_new_file(in);
      s->panel_count_only = get_R_PANEL_count_only(in);
      s->panel_bypass_mode = get_R_PANEL_bypass_mode(in);
      s->panel_enable_led = get_R_PANEL_enable_led(in);
      s->panel_counter = get_R_PANEL_counter(in);

      ret = SANE_STATUS_GOOD;
    }
  }
  
  if(option)
    s->panel_read[option-OPT_START] = 0;

  DBG (10, "read_panel: finish %d\n",s->panel_counter);
  
  return ret;
}

static SANE_Status
send_panel(struct scanner *s)
{
    SANE_Status ret=SANE_STATUS_GOOD;

    unsigned char cmd[SEND_len];
    size_t cmdLen = SEND_len;

    unsigned char out[S_PANEL_len];
    size_t outLen = S_PANEL_len;

    DBG (10, "send_panel: start\n");

    if(!s->can_write_panel){
      DBG (10, "send_panel: unsupported, finishing\n");
      return ret;
    }

    memset(cmd,0,cmdLen);
    set_SCSI_opcode(cmd, SEND_code);
    set_S_xfer_datatype (cmd, SR_datatype_panel);
    set_S_xfer_length (cmd, outLen);

    memset(out,0,outLen);
    set_S_PANEL_enable_led(out,s->panel_enable_led);
    set_S_PANEL_counter(out,s->panel_counter);
  
    ret = do_cmd (
      s, 1, 0,
      cmd, cmdLen,
      out, outLen,
      NULL, NULL
    );
  
    if (ret == SANE_STATUS_EOF) {
        ret = SANE_STATUS_GOOD;
    }
  
    DBG (10, "send_panel: finish %d\n", ret);
  
    return ret;
}

/*
 * @@ Section 4 - SANE scanning functions
 */
/*
 * Called by SANE to retrieve information about the type of data
 * that the current scan will return.
 *
 * From the SANE spec:
 * This function is used to obtain the current scan parameters. The
 * returned parameters are guaranteed to be accurate between the time
 * a scan has been started (sane_start() has been called) and the
 * completion of that request. Outside of that window, the returned
 * values are best-effort estimates of what the parameters will be
 * when sane_start() gets invoked.
 * 
 * Calling this function before a scan has actually started allows,
 * for example, to get an estimate of how big the scanned image will
 * be. The parameters passed to this function are the handle h of the
 * device for which the parameters should be obtained and a pointer p
 * to a parameter structure.
 */
SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
    SANE_Status ret = SANE_STATUS_GOOD;
    struct scanner *s = (struct scanner *) handle;
  
    DBG (10, "sane_get_parameters: start\n");

    if(!s->started){
      ret = update_params(s,0);
      if(ret){
        DBG (5, "sane_get_parameters: up error, returning %d\n", ret);
        return ret;
      }
    }

    /* this backend only sends single frame images */
    params->last_frame = 1;

    params->format = s->i.format;
    params->lines = s->i.height;
    params->depth = s->i.bpp;
    if(params->depth == 24) params->depth = 8;
    params->pixels_per_line = s->i.width;
    params->bytes_per_line = s->i.Bpl;

    DBG(15,"sane_get_parameters: x: max=%d, page=%d, gpw=%d, res=%d\n",
      s->valid_x, s->i.page_x, get_page_width(s), s->i.dpi_x);

    DBG(15,"sane_get_parameters: y: max=%d, page=%d, gph=%d, res=%d\n",
      s->max_y, s->i.page_y, get_page_height(s), s->i.dpi_y);

    DBG(15,"sane_get_parameters: area: tlx=%d, brx=%d, tly=%d, bry=%d\n",
      s->i.tl_x, s->i.br_x, s->i.tl_y, s->i.br_y);

    DBG (15, "sane_get_parameters: params: ppl=%d, Bpl=%d, lines=%d\n", 
      params->pixels_per_line, params->bytes_per_line, params->lines);

    DBG (15, "sane_get_parameters: params: format=%d, depth=%d, last=%d\n", 
      params->format, params->depth, params->last_frame);

    DBG (10, "sane_get_parameters: finish\n");
  
    return ret;
}

SANE_Status
update_params(struct scanner *s, int calib)
{
    SANE_Status ret = SANE_STATUS_GOOD;
  
    DBG (10, "update_params: start\n");
  
    s->u.width = (s->u.br_x - s->u.tl_x) * s->u.dpi_x / 1200;
    s->u.height = (s->u.br_y - s->u.tl_y) * s->u.dpi_y / 1200;

    if (s->u.mode == MODE_COLOR) {
      s->u.format = SANE_FRAME_RGB;
      s->u.bpp = 24;
    }
    else if (s->u.mode == MODE_GRAYSCALE) {
      s->u.format = SANE_FRAME_GRAY;
      s->u.bpp = 8;
    }
    else {
      s->u.format = SANE_FRAME_GRAY;
      s->u.bpp = 1;

      /* round down to byte boundary */
      s->u.width -= s->u.width % 8;
    }

    /* round down to pixel boundary for some scanners */
    s->u.width -= s->u.width % s->ppl_mod;

    /* jpeg requires 8x8 squares */
    if(s->compress == COMP_JPEG && s->u.mode >= MODE_GRAYSCALE){
      s->u.format = SANE_FRAME_JPEG;
      s->u.width -= s->u.width % 8;
      s->u.height -= s->u.height % 8;
    }

    s->u.Bpl = s->u.width * s->u.bpp / 8;
    s->u.valid_Bpl = s->u.Bpl;
    s->u.valid_width = s->u.width;

    DBG (15, "update_params: user params: w:%d h:%d m:%d f:%d b:%d\n",
      s->u.width, s->u.height, s->u.mode, s->u.format, s->u.bpp);
    DBG (15, "update_params: user params: B:%d vB:%d vw:%d\n",
      s->u.Bpl, s->u.valid_Bpl, s->u.valid_width);
    DBG (15, "update_params: user params: x b:%d t:%d d:%d y b:%d t:%d d:%d\n",
      s->u.br_x, s->u.tl_x, s->u.dpi_x, s->u.br_y, s->u.tl_y, s->u.dpi_y);

    /* some scanners are limited in their valid scan params
     * make a second version of the params struct, but 
     * override the user's values with what the scanner can actually do */

    memcpy(&s->s,&s->u,sizeof(struct img_params));

    /*********** missing modes (move up to valid one) **************/
    if(s->s.mode == MODE_LINEART && !s->can_monochrome){
      s->s.mode = MODE_GRAYSCALE;
      s->s.format = SANE_FRAME_GRAY;
      s->s.bpp = 8;
    }
    if(s->s.mode == MODE_GRAYSCALE && !s->can_grayscale){
      s->s.mode = MODE_COLOR;
      s->s.format = SANE_FRAME_RGB;
      s->s.bpp = 24;
    }
    if(s->s.mode == MODE_COLOR && !s->can_color){
      DBG (5, "update_params: no valid mode\n");
      return SANE_STATUS_INVAL;
    }

    /********** missing resolutions (move up to valid one) *********/
    if(!s->step_x_res){
      int i;
      for(i=0;i<DPI_1200;i++){

        /* this res is smaller or invalid, skip it */
        if(s->s.dpi_x > dpi_list[i] || !s->std_res_x[i])
          continue;

        /* same & valid res, done */
        if(s->s.dpi_x == dpi_list[i])
          break;

        /* different & valid res, switch */
        s->s.dpi_x = dpi_list[i];
        break;
      }

      if(i > DPI_1200){
        DBG (5, "update_params: no dpi\n");
        return SANE_STATUS_INVAL;
      }
    }

    /*********** weird scan area (increase to valid one) *********/
    if(s->fixed_width){
      s->s.tl_x = 0;
      s->s.br_x = s->max_x;
      s->s.page_x = s->max_x;
    }

    /*recalculate new params*/
    s->s.width = (s->s.br_x - s->s.tl_x) * s->s.dpi_x / 1200;

    /* round down to byte boundary */
    if(s->s.mode < MODE_GRAYSCALE){
      s->s.width -= s->s.width % 8;
    }

    /* round down to pixel boundary for some scanners */
    s->s.width -= s->s.width % s->ppl_mod;

    s->s.valid_width = s->s.width;
    s->s.valid_Bpl = s->s.valid_width * s->s.bpp / 8;

    /* some machines (DR-2050) require even bytes per scanline */
    /* increase width and Bpl, but not valid_width and valid_Bpl */
    if(s->even_Bpl && (s->s.width % 2)){
      s->s.width++;
    }

    s->s.Bpl = s->s.width * s->s.bpp / 8;

    /* figure out how many valid bytes per line (2510 is padded) */
    if(s->color_interlace[SIDE_FRONT] == COLOR_INTERLACE_2510){
      s->s.valid_Bpl = s->s.Bpl*11/12;
      s->s.valid_width = s->s.width*11/12;
    }

    /* some scanners need longer scans because front/back is offset */
    if((s->u.source == SOURCE_ADF_DUPLEX || s->u.source == SOURCE_CARD_DUPLEX)
      && s->duplex_offset && !calib)
      s->s.height = (s->u.br_y-s->u.tl_y+s->duplex_offset) * s->u.dpi_y / 1200;

    /* round lines up to even number */
    s->s.height += s->s.height % 2;
  
    DBG (15, "update_params: scan params: w:%d h:%d m:%d f:%d b:%d\n",
      s->s.width, s->s.height, s->s.mode, s->s.format, s->s.bpp);
    DBG (15, "update_params: scan params: B:%d vB:%d vw:%d\n",
      s->s.Bpl, s->s.valid_Bpl, s->s.valid_width);
    DBG (15, "update_params: scan params: x b:%d t:%d d:%d y b:%d t:%d d:%d\n",
      s->s.br_x, s->s.tl_x, s->s.dpi_x, s->s.br_y, s->s.tl_y, s->s.dpi_y);

    /* make a third (intermediate) version of the params struct,
     * currently identical to the user's params. this is what
     * we actually will send back to the user (though buffer_xxx
     * functions might change these values after this runs) */

    /* calibration code needs the data just as it comes from the scanner */
    if(calib)
      memcpy(&s->i,&s->s,sizeof(struct img_params));
    /* normal scans need the data cleaned for presentation to the user */
    else{
      memcpy(&s->i,&s->u,sizeof(struct img_params));
      /*dumb scanners pad the top of front page in duplex*/
      if(s->i.source == SOURCE_ADF_DUPLEX || s->i.source == SOURCE_CARD_DUPLEX)
        s->i.skip_lines[s->duplex_offset_side] = s->duplex_offset * s->i.dpi_y / 1200;
    }

    DBG (15, "update_params: i params: w:%d h:%d m:%d f:%d b:%d\n",
      s->i.width, s->i.height, s->i.mode, s->i.format, s->i.bpp);
    DBG (15, "update_params: i params: B:%d vB:%d vw:%d\n",
      s->i.Bpl, s->i.valid_Bpl, s->i.valid_width);
    DBG (15, "update_params: i params: x b:%d t:%d d:%d y b:%d t:%d d:%d\n",
      s->i.br_x, s->i.tl_x, s->i.dpi_x, s->i.br_y, s->i.tl_y, s->i.dpi_y);

    DBG (10, "update_params: finish\n");
    return ret;
}

/* reset image size parameters after buffer_xxx functions changed them */
SANE_Status
update_i_params(struct scanner *s)
{
    SANE_Status ret = SANE_STATUS_GOOD;
  
    DBG (10, "update_i_params: start\n");

    s->i.width = s->u.width;
    s->i.Bpl = s->u.Bpl;
 
    DBG (10, "update_i_params: finish\n");
    return ret;
}

/*
 * Called by SANE when a page acquisition operation is to be started.
 * commands: set window, object pos, and scan
 *
 * this will be called between sides of a duplex scan,
 * and at the start of each page of an adf batch.
 * hence, we spend alot of time playing with s->started, etc.
 */
SANE_Status
sane_start (SANE_Handle handle)
{
  struct scanner *s = handle;
  SANE_Status ret = SANE_STATUS_GOOD;

  DBG (10, "sane_start: start\n");
  DBG (15, "started=%d, side=%d, source=%d\n",
    s->started, s->side, s->u.source);

  /* undo any prior sane_cancel calls */
  s->cancelled=0;

  /* protect this block from sane_cancel */
  s->reading=1;

  /* not finished with current side, error */
  if (s->started && !s->u.eof[s->side]) {
    DBG(5,"sane_start: previous transfer not finished?");
    return SANE_STATUS_INVAL;
  }

  /* batch start? inititalize struct and scanner */
  if(!s->started){

    /* load side marker */
    if(s->u.source == SOURCE_ADF_BACK || s->u.source == SOURCE_CARD_BACK){
      s->side = SIDE_BACK;
    }
    else{
      s->side = SIDE_FRONT;
    }

    /* eject paper leftover*/
    if(object_position (s, SANE_FALSE)){
      DBG (5, "sane_start: ERROR: cannot eject page\n");
    }

    /* wait for scanner to finish eject */
    ret = wait_scanner (s);
    if (ret != SANE_STATUS_GOOD) {
      DBG (5, "sane_start: ERROR: cannot wait scanner\n");
      goto errors;
    }

    /* load the brightness/contrast lut with linear slope for calibration */
    ret = load_lut (s->lut, 8, 8, 0, 255, 0, 0);
    if (ret != SANE_STATUS_GOOD) {
      DBG (5, "sane_start: ERROR: cannot load lut\n");
      goto errors;
    }

    /* AFE cal */
    if((ret = calibrate_AFE(s))){
      DBG (5, "sane_start: ERROR: cannot cal afe\n");
      goto errors;
    }

    /* fine cal */
    if((ret = calibrate_fine(s))){
      DBG (5, "sane_start: ERROR: cannot cal fine\n");
      goto errors;
    }

    if((ret = calibrate_fine_buffer(s))){
      DBG (5, "sane_start: ERROR: cannot cal fine from buffer\n");
      goto errors;
    }

    /* reset the page counter after calibration */
    s->panel_counter = 0;
    s->prev_page = 0;
    if(send_panel(s)){
      DBG (5, "sane_start: ERROR: cannot send panel\n");
    }

    /* load our own private copy of scan params */
    ret = update_params(s,0);
    if (ret != SANE_STATUS_GOOD) {
      DBG (5, "sane_start: ERROR: cannot update_params\n");
      goto errors;
    }

    /* set window command */
    ret = set_window(s);
    if (ret != SANE_STATUS_GOOD) {
      DBG (5, "sane_start: ERROR: cannot set window\n");
      goto errors;
    }

    /* buffer/duplex/ald/fb/card command */
    ret = ssm_buffer(s);
    if (ret != SANE_STATUS_GOOD) {
      DBG (5, "sane_start: ERROR: cannot ssm buffer\n");
      goto errors;
    }

    /* dropout color command */
    ret = ssm_do(s);
    if (ret != SANE_STATUS_GOOD) {
      DBG (5, "sane_start: ERROR: cannot ssm do\n");
      goto errors;
    }

    /* double feed detection command */
    ret = ssm_df(s);
    if (ret != SANE_STATUS_GOOD) {
      DBG (5, "sane_start: ERROR: cannot ssm df\n");
      goto errors;
    }

    /* clean scan params for new scan */
    ret = clean_params(s);
    if (ret != SANE_STATUS_GOOD) {
      DBG (5, "sane_start: ERROR: cannot clean_params\n");
      goto errors;
    }

    /* make large buffers to hold the images */
    ret = image_buffers(s,1);
    if (ret != SANE_STATUS_GOOD) {
      DBG (5, "sane_start: ERROR: cannot load buffers\n");
      goto errors;
    }

    /* load the brightness/contrast lut with user choices */
    ret = load_lut (s->lut, 8, 8, 0, 255, s->contrast, s->brightness);
    if (ret != SANE_STATUS_GOOD) {
      DBG (5, "sane_start: ERROR: cannot load lut\n");
      goto errors;
    }

    /* card reader dislikes op? */
    if(s->s.source < SOURCE_CARD_FRONT){
      /* grab next page */
      ret = object_position (s, SANE_TRUE);
      if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: ERROR: cannot load page\n");
        goto errors;
      }
  
      /* wait for scanner to finish load */
      ret = wait_scanner (s);
      if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: ERROR: cannot wait scanner\n");
        goto errors;
      }
    }

    /* start scanning */
    ret = start_scan (s,0);
    if (ret != SANE_STATUS_GOOD) {
      DBG (5, "sane_start: ERROR: cannot start_scan\n");
      goto errors;
    }

    s->started = 1;
  }

  /* stuff done for subsequent images */
  else{

    /* duplex needs to switch sides */
    if(s->s.source == SOURCE_ADF_DUPLEX || s->s.source == SOURCE_CARD_DUPLEX){
      s->side = !s->side;
    }

    /* reset the intermediate params */
    ret = update_i_params(s);
    if (ret != SANE_STATUS_GOOD) {
      DBG (5, "sane_start: ERROR: cannot update_i_params\n");
      goto errors;
    }

    /* set clean defaults with new sheet of paper */
    /* dont reset the transfer vars on backside of duplex page */
    /* otherwise buffered back page will be lost */
    /* ingest paper with adf (no-op for fb) */
    /* dont call object pos or scan on back side of duplex scan */
    if(s->side == SIDE_FRONT || s->s.source == SOURCE_ADF_BACK || s->s.source == SOURCE_CARD_BACK){

      /* clean scan params for new scan */
      ret = clean_params(s);
      if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: ERROR: cannot clean_params\n");
        goto errors;
      }

      /* big scanners and small ones in non-buff mode: OP to detect paper */
      if(s->always_op || !s->buffermode){
        ret = object_position (s, SANE_TRUE);
        if (ret != SANE_STATUS_GOOD) {
          DBG (5, "sane_start: ERROR: cannot load page\n");
          goto errors;
        }

        /* user wants unbuffered scans */
        /* send scan command */
        if(!s->buffermode){
          ret = start_scan (s,0);
          if (ret != SANE_STATUS_GOOD) {
            DBG (5, "sane_start: ERROR: cannot start_scan\n");
            goto errors;
          }
        }
      }
  
      /* small, buffering scanners check for more pages by reading counter */
      else{
        ret = read_panel (s, OPT_COUNTER);
        if (ret != SANE_STATUS_GOOD) {
          DBG (5, "sane_start: ERROR: cannot load page\n");
          goto errors;
        }
        if(s->prev_page == s->panel_counter){
          DBG (5, "sane_start: same counter (%d) no paper?\n",s->prev_page);
          ret = SANE_STATUS_NO_DOCS;
          goto errors;
        }
        DBG (5, "sane_start: diff counter (%d/%d)\n",
          s->prev_page,s->panel_counter);
      }
    }
  }

  /* reset jpeg params on each page */
  s->jpeg_stage=JPEG_STAGE_NONE;
  s->jpeg_ff_offset=0;

  DBG (15, "started=%d, side=%d, source=%d\n",
    s->started, s->side, s->u.source);

  /* certain options require the entire image to 
   * be collected from the scanner before we can
   * tell the user the size of the image. the sane 
   * API has no way to inform the frontend of this,
   * so we block and buffer. yuck */
  if( (s->swdeskew || s->swdespeck || s->swcrop)
    && s->s.format != SANE_FRAME_JPEG
  ){

    /* get image */
    while(!s->s.eof[s->side] && !ret){
      SANE_Int len = 0;
      ret = sane_read((SANE_Handle)s, NULL, 0, &len);
    }

    /* check for errors */
    if (ret != SANE_STATUS_GOOD) {
      DBG (5, "sane_start: ERROR: cannot buffer image\n");
      goto errors;
    }

    DBG (5, "sane_start: OK: done buffering\n");

    /* finished buffering, adjust image as required */
    if(s->swdeskew){
      buffer_deskew(s,s->side);
    }
    if(s->swcrop){
      buffer_crop(s,s->side);
    }
    if(s->swdespeck){
      buffer_despeck(s,s->side);
    }

  }

  ret = check_for_cancel(s);
  s->reading = 0;

  DBG (10, "sane_start: finish %d\n", ret);
  return ret;

  errors:
    DBG (10, "sane_start: error %d\n", ret);
    s->started = 0;
    s->cancelled = 0;
    s->reading = 0;
    return ret;
}

/*
 * cleans params for new scan
 */
static SANE_Status
clean_params (struct scanner *s)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  DBG (10, "clean_params: start\n");

  s->u.eof[0]=0;
  s->u.eof[1]=0;
  s->u.bytes_sent[0]=0;
  s->u.bytes_sent[1]=0;
  s->u.bytes_tot[0]=0;
  s->u.bytes_tot[1]=0;

  s->i.eof[0]=0;
  s->i.eof[1]=0;
  s->i.bytes_sent[0]=0;
  s->i.bytes_sent[1]=0;
  s->i.bytes_tot[0]=0;
  s->i.bytes_tot[1]=0;

  s->s.eof[0]=0;
  s->s.eof[1]=0;
  s->s.bytes_sent[0]=0;
  s->s.bytes_sent[1]=0;
  s->s.bytes_tot[0]=0;
  s->s.bytes_tot[1]=0;

  /* store the number of front bytes */ 
  if ( s->u.source != SOURCE_ADF_BACK && s->u.source != SOURCE_CARD_BACK )
    s->u.bytes_tot[SIDE_FRONT] = s->u.Bpl * s->u.height;

  if ( s->i.source != SOURCE_ADF_BACK && s->i.source != SOURCE_CARD_BACK )
    s->i.bytes_tot[SIDE_FRONT] = s->i.Bpl * s->i.height;

  if ( s->s.source != SOURCE_ADF_BACK && s->s.source != SOURCE_CARD_BACK )
    s->s.bytes_tot[SIDE_FRONT] = s->s.Bpl * s->s.height;

  /* store the number of back bytes */ 
  if ( s->u.source == SOURCE_ADF_DUPLEX || s->u.source == SOURCE_ADF_BACK 
    || s->u.source == SOURCE_CARD_DUPLEX || s->u.source == SOURCE_CARD_BACK )
    s->u.bytes_tot[SIDE_BACK] = s->u.Bpl * s->u.height;

  if ( s->i.source == SOURCE_ADF_DUPLEX || s->i.source == SOURCE_ADF_BACK
    || s->i.source == SOURCE_CARD_DUPLEX || s->i.source == SOURCE_CARD_BACK )
    s->i.bytes_tot[SIDE_BACK] = s->i.Bpl * s->i.height;

  if ( s->s.source == SOURCE_ADF_DUPLEX || s->s.source == SOURCE_ADF_BACK
    || s->s.source == SOURCE_CARD_DUPLEX || s->s.source == SOURCE_CARD_BACK )
    s->s.bytes_tot[SIDE_BACK] = s->s.Bpl * s->s.height;

  DBG (10, "clean_params: finish\n");

  return ret;
}

/*
 * frees/callocs buffers to hold the scan data
 */
static SANE_Status
image_buffers (struct scanner *s, int setup)
{
  SANE_Status ret = SANE_STATUS_GOOD;
  int side;

  DBG (10, "image_buffers: start\n");

  for(side=0;side<2;side++){

    /* free current buffer */
    if (s->buffers[side]) {
      DBG (15, "image_buffers: free buffer %d.\n",side);
      free(s->buffers[side]);
      s->buffers[side] = NULL;
    }

    /* build new buffer if asked */
    if(s->i.bytes_tot[side] && setup){
      s->buffers[side] = calloc (1,s->i.bytes_tot[side]);
      if (!s->buffers[side]) {
        DBG (5, "image_buffers: Error, no buffer %d.\n",side);
        return SANE_STATUS_NO_MEM;
      }
    }
  }

  DBG (10, "image_buffers: finish\n");

  return ret;
}

/*
 * This routine issues a SCSI SET WINDOW command to the scanner, using the
 * values currently in the s->s param structure.
 */
static SANE_Status
set_window (struct scanner *s)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  /* The command specifies the number of bytes in the data phase 
   * the data phase has a header, followed by 1 window desc block 
   * the header specifies the number of bytes in 1 window desc block
   */

  unsigned char cmd[SET_WINDOW_len];
  size_t cmdLen = SET_WINDOW_len;
 
  unsigned char out[SW_header_len + SW_desc_len];
  size_t outLen = SW_header_len + SW_desc_len;

  unsigned char * header = out;                       /*header*/
  unsigned char * desc1 = out + SW_header_len;        /*descriptor*/

  DBG (10, "set_window: start\n");

  /*build the payload*/
  memset(out,0,outLen);

  /* set window desc size in header */
  set_WPDB_wdblen(header, SW_desc_len);

  /* init the window block */
  if (s->s.source == SOURCE_ADF_BACK || s->s.source == SOURCE_CARD_BACK) {
    set_WD_wid (desc1, WD_wid_back);
  }
  else{
    set_WD_wid (desc1, WD_wid_front);
  }

  set_WD_Xres (desc1, s->s.dpi_x);
  set_WD_Yres (desc1, s->s.dpi_y);

  /* some machines need max width */
  if(s->fixed_width){
    set_WD_ULX (desc1, 0);
    set_WD_width (desc1, s->max_x);
  }

  /* or they align left */
  else if(s->u.source == SOURCE_FLATBED){
    set_WD_ULX (desc1, s->s.tl_x);
    set_WD_width (desc1, s->s.width * 1200/s->s.dpi_x);
  }

  /* or we have to center the window ourselves */
  else{
    set_WD_ULX (desc1, (s->max_x - s->s.page_x) / 2 + s->s.tl_x);
    set_WD_width (desc1, s->s.width * 1200/s->s.dpi_x);
  }

  /* some models require that the tly value be inverted? */
  if(s->invert_tly)
    set_WD_ULY (desc1, ~s->s.tl_y);
  else
    set_WD_ULY (desc1, s->s.tl_y);

  set_WD_length (desc1, s->s.height * 1200/s->s.dpi_y);

  if(s->has_btc){
    /*convert our common -127 to +127 range into HW's range
     *FIXME: this code assumes hardware range of 0-255 */
    set_WD_brightness (desc1, s->brightness+128);
  
    set_WD_threshold (desc1, s->threshold);
  
    /*convert our common -127 to +127 range into HW's range
     *FIXME: this code assumes hardware range of 0-255 */
    set_WD_contrast (desc1, s->contrast+128);
  }

  set_WD_composition (desc1, s->s.mode);

  if(s->s.bpp == 24)
    set_WD_bitsperpixel (desc1, 8);
  else
    set_WD_bitsperpixel (desc1, s->s.bpp);

  if(s->s.mode == MODE_HALFTONE){
    /*set_WD_ht_type(desc1, s->ht_type);
    set_WD_ht_pattern(desc1, s->ht_pattern);*/
  }

  set_WD_rif (desc1, s->rif);
  set_WD_rgb(desc1, s->rgb_format);
  set_WD_padding(desc1, s->padding);

  /*FIXME: what is this? */
  set_WD_reserved2(desc1, s->unknown_byte2);

  set_WD_compress_type(desc1, COMP_NONE);
  set_WD_compress_arg(desc1, 0);

  /* some scanners support jpeg image compression, for color/gs only */
  if(s->s.format == SANE_FRAME_JPEG){
    set_WD_compress_type(desc1, COMP_JPEG);
    set_WD_compress_arg(desc1, s->compress_arg);
  }

  /*build the command*/
  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd, SET_WINDOW_code);
  set_SW_xferlen(cmd, outLen);

  ret = do_cmd (
    s, 1, 0,
    cmd, cmdLen,
    out, outLen,
    NULL, NULL
  );

  if (!ret && (s->s.source == SOURCE_ADF_DUPLEX || s->s.source == SOURCE_CARD_DUPLEX)) {
      set_WD_wid (desc1, WD_wid_back);
      ret = do_cmd (
        s, 1, 0,
        cmd, cmdLen,
        out, outLen,
        NULL, NULL
      );
  }

  DBG (10, "set_window: finish\n");

  return ret;
}

/*
 * Issues the SCSI OBJECT POSITION command if an ADF is in use.
 */
static SANE_Status
object_position (struct scanner *s, int i_load)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  unsigned char cmd[OBJECT_POSITION_len];
  size_t cmdLen = OBJECT_POSITION_len;

  DBG (10, "object_position: start\n");

  if (s->u.source == SOURCE_FLATBED) {
    DBG (10, "object_position: flatbed no-op\n");
    return SANE_STATUS_GOOD;
  }

  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd, OBJECT_POSITION_code);

  if (i_load) {
    DBG (15, "object_position: load\n");
    set_OP_autofeed (cmd, OP_Feed);
  }
  else {
    DBG (15, "object_position: eject\n");
    set_OP_autofeed (cmd, OP_Discharge);
  }

  ret = do_cmd (
    s, 1, 0,
    cmd, cmdLen,
    NULL, 0,
    NULL, NULL
  );
  if (ret != SANE_STATUS_GOOD)
    return ret;

  DBG (10, "object_position: finish\n");

  return ret;
}

/*
 * Issues SCAN command.
 * 
 * (This doesn't actually read anything, it just tells the scanner
 * to start scanning.)
 */
static SANE_Status
start_scan (struct scanner *s, int type)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  unsigned char cmd[SCAN_len];
  size_t cmdLen = SCAN_len;

  unsigned char out[] = {WD_wid_front, WD_wid_back};
  size_t outLen = 2;

  DBG (10, "start_scan: start\n");

  /* calibration scans use 0xff or 0xfe */
  if(type){
    out[0] = type;
    out[1] = type;
  }

  if (s->s.source != SOURCE_ADF_DUPLEX && s->s.source != SOURCE_CARD_DUPLEX) {
    outLen--;
    if(s->s.source == SOURCE_ADF_BACK || s->s.source == SOURCE_CARD_BACK) {
      out[0] = WD_wid_back;
    }
  }

  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd, SCAN_code);
  set_SC_xfer_length (cmd, outLen);

  ret = do_cmd (
    s, 1, 0,
    cmd, cmdLen,
    out, outLen,
    NULL, NULL
  );

  DBG (10, "start_scan: finish\n");

  return ret;
}

/*
 * Called by SANE to read data.
 * 
 * From the SANE spec:
 * This function is used to read image data from the device
 * represented by handle h.  Argument buf is a pointer to a memory
 * area that is at least maxlen bytes long.  The number of bytes
 * returned is stored in *len. A backend must set this to zero when
 * the call fails (i.e., when a status other than SANE_STATUS_GOOD is
 * returned).
 * 
 * When the call succeeds, the number of bytes returned can be
 * anywhere in the range from 0 to maxlen bytes.
 */
SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len, SANE_Int * len)
{
  struct scanner *s = (struct scanner *) handle;
  SANE_Status ret=SANE_STATUS_GOOD;

  DBG (10, "sane_read: start\n");

  *len=0;

  /* maybe cancelled? */
  if(!s->started){
    DBG (5, "sane_read: not started, call sane_start\n");
    return SANE_STATUS_CANCELLED;
  }

  /* sane_start required between sides */
  if(s->u.bytes_sent[s->side] == s->i.bytes_tot[s->side]){
    s->u.eof[s->side] = 1;
    DBG (15, "sane_read: returning eof\n");
    return SANE_STATUS_EOF;
  }

  s->reading = 1;

  /* double width pnm interlacing */
  if((s->s.source == SOURCE_ADF_DUPLEX || s->s.source == SOURCE_CARD_DUPLEX)
    && s->s.format <= SANE_FRAME_RGB
    && s->duplex_interlace != DUPLEX_INTERLACE_NONE
  ){

    /* buffer both sides */
    if(!s->s.eof[SIDE_FRONT] || !s->s.eof[SIDE_BACK]){
      ret = read_from_scanner_duplex(s, 0);
      if(ret){
        DBG(5,"sane_read: front returning %d\n",ret);
        goto errors;
      }
      /*read last block, update counter*/
      if(s->s.eof[SIDE_FRONT] && s->s.eof[SIDE_BACK]){
        s->prev_page++;
        DBG(15,"sane_read: duplex counter %d\n",s->prev_page);
      }
    }
  }
    
  /* simplex or non-alternating duplex */
  else{
    if(!s->s.eof[s->side]){
      ret = read_from_scanner(s, s->side, 0);
      if(ret){
        DBG(5,"sane_read: side %d returning %d\n",s->side,ret);
        goto errors;
      }
      /*read last block, update counter*/
      if(s->s.eof[s->side]){
        s->prev_page++;
        DBG(15,"sane_read: side %d counter %d\n",s->side,s->prev_page);
      }
    }
  }

  /* copy a block from buffer to frontend */
  ret = read_from_buffer(s,buf,max_len,len,s->side);
  if(ret)
    goto errors;
  
  ret = check_for_cancel(s);
  s->reading = 0;

  DBG (10, "sane_read: finish %d\n", ret);
  return ret;

  errors:
    DBG (10, "sane_read: error %d\n", ret);
    s->reading = 0;
    s->cancelled = 0;
    s->started = 0;
    return ret;
}

static SANE_Status
read_from_scanner(struct scanner *s, int side, int exact)
{
  SANE_Status ret=SANE_STATUS_GOOD;

  unsigned char cmd[READ_len];
  size_t cmdLen = READ_len;

  unsigned char * in;
  size_t inLen = 0;

  size_t bytes = s->buffer_size;
  size_t remain = s->s.bytes_tot[side] - s->s.bytes_sent[side];

  DBG (10, "read_from_scanner: start\n");

  /* all requests must end on line boundary */
  bytes -= (bytes % s->s.Bpl);

  /* some larger scanners require even bytes per block */
  if(bytes % 2){
    bytes -= s->s.Bpl;
  }

  /* usually (image) we want to read too much data, and get RS */
  /* sometimes (calib) we want to do an exact read */
  if(exact && bytes > remain){
    bytes = remain;
  }

  DBG(15, "read_from_scanner: si:%d to:%d rx:%d re:%lu bu:%d pa:%lu ex:%d\n",
      side, s->s.bytes_tot[side], s->s.bytes_sent[side],
      (unsigned long)remain, s->buffer_size, (unsigned long)bytes, exact);

  inLen = bytes;
  in = malloc(inLen);
  if(!in){
    DBG(5, "read_from_scanner: not enough mem for buffer: %d\n",(int)inLen);
    return SANE_STATUS_NO_MEM;
  }

  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd, READ_code);
  set_R_datatype_code (cmd, SR_datatype_image);

  set_R_xfer_length (cmd, inLen);

  ret = do_cmd (
    s, 1, 0,
    cmd, cmdLen,
    NULL, 0,
    in, &inLen
  );

  if (ret == SANE_STATUS_GOOD) {
    DBG(15, "read_from_scanner: got GOOD, returning GOOD %lu\n", (unsigned long)inLen);
  }
  else if (ret == SANE_STATUS_EOF) {
    DBG(15, "read_from_scanner: got EOF, finishing %lu\n", (unsigned long)inLen);
  }
  else if (ret == SANE_STATUS_DEVICE_BUSY) {
    DBG(5, "read_from_scanner: got BUSY, returning GOOD\n");
    inLen = 0;
    ret = SANE_STATUS_GOOD;
  }
  else {
    DBG(5, "read_from_scanner: error reading data block status = %d\n",ret);
    inLen = 0;
  }

  /* this is jpeg data, we need to fix the missing image size */
  if(s->s.format == SANE_FRAME_JPEG){

    /* look for the SOF header near the beginning */
    if(s->jpeg_stage == JPEG_STAGE_NONE || s->jpeg_ff_offset < 0x0d){

      size_t i;

      for(i=0;i<inLen;i++){
  
        /* about to change stage */
        if(s->jpeg_stage == JPEG_STAGE_NONE && in[i] == 0xff){
          s->jpeg_ff_offset=0;
          continue;
        }
  
        s->jpeg_ff_offset++;

        /* last byte was an ff, this byte is SOF */
        if(s->jpeg_ff_offset == 1 && in[i] == 0xc0){
          s->jpeg_stage = JPEG_STAGE_SOF;
          continue;
        }
  
        if(s->jpeg_stage == JPEG_STAGE_SOF){

          /* lines in start of frame, overwrite it */
          if(s->jpeg_ff_offset == 5){
            in[i] = (s->s.height >> 8) & 0xff;
            continue;
          }
          if(s->jpeg_ff_offset == 6){
            in[i] = s->s.height & 0xff;
            continue;
          }
      
          /* width in start of frame, overwrite it */
          if(s->jpeg_ff_offset == 7){
            in[i] = (s->s.width >> 8) & 0xff;
            continue;
          }
          if(s->jpeg_ff_offset == 8){
            in[i] = s->s.width & 0xff;
            continue;
          }
        }
      }
    }
  }

  /*scanner may have sent more data than we asked for, chop it*/
  if(inLen > remain){
    inLen = remain;
  }

  /* we've got some data, descramble and store it */
  if(inLen){
    copy_simplex(s,in,inLen,side);
  }

  free(in);

  /* we've read all data, but not eof. clear and pretend */
  if(exact && inLen == remain){
    DBG (10, "read_from_scanner: exact read, clearing\n");
    ret = object_position (s,SANE_FALSE);
    if(ret){
      return ret;
    }
    ret = SANE_STATUS_EOF;
  }

  if(ret == SANE_STATUS_EOF){

    /* this is jpeg data, we need to change the total size */
    if(s->s.format == SANE_FRAME_JPEG){
      s->s.bytes_tot[side] = s->s.bytes_sent[side];
      s->i.bytes_tot[side] = s->i.bytes_sent[side];
      s->u.bytes_tot[side] = s->i.bytes_sent[side];
    } 

    /* this is non-jpeg data, fill remainder, change rx'd size */
    else{

      DBG (15, "read_from_scanner: eof: %d %d\n", s->i.bytes_tot[side], s->i.bytes_sent[side]);

      /* clone the last line repeatedly until the end */
      while(s->i.bytes_tot[side] > s->i.bytes_sent[side]){
        memcpy(
          s->buffers[side]+s->i.bytes_sent[side]-s->i.Bpl,
          s->buffers[side]+s->i.bytes_sent[side],
          s->i.Bpl
        );
        s->i.bytes_sent[side] += s->i.Bpl;
      }

      DBG (15, "read_from_scanner: eof2: %d %d\n", s->i.bytes_tot[side], s->i.bytes_sent[side]);

      /* pretend we got all the data from scanner */
      s->s.bytes_sent[side] = s->s.bytes_tot[side];
    }

    s->i.eof[side] = 1;
    s->s.eof[side] = 1;
    ret = SANE_STATUS_GOOD;
  }

  DBG(15, "read_from_scanner: sto:%d srx:%d sef:%d uto:%d urx:%d uef:%d\n",
    s->s.bytes_tot[side], s->s.bytes_sent[side], s->s.eof[side],
    s->u.bytes_tot[side], s->u.bytes_sent[side], s->u.eof[side]);

  DBG (10, "read_from_scanner: finish\n");

  return ret;
}

/* cheaper scanners interlace duplex scans on a byte basis
 * this code requests double width lines from scanner */
static SANE_Status
read_from_scanner_duplex(struct scanner *s,int exact)
{
  SANE_Status ret=SANE_STATUS_GOOD;

  unsigned char cmd[READ_len];
  size_t cmdLen = READ_len;

  unsigned char * in;
  size_t inLen = 0;

  size_t bytes = s->buffer_size;
  size_t remain = s->s.bytes_tot[SIDE_FRONT] + s->s.bytes_tot[SIDE_BACK]
    - s->s.bytes_sent[SIDE_FRONT] - s->s.bytes_sent[SIDE_BACK];

  DBG (10, "read_from_scanner_duplex: start\n");

  /* all requests must end on WIDE line boundary */
  bytes -= (bytes % (s->s.Bpl*2));

  /* usually (image) we want to read too much data, and get RS */
  /* sometimes (calib) we want to do an exact read */
  if(exact && bytes > remain){
    bytes = remain;
  }

  DBG(15, "read_from_scanner_duplex: re:%lu bu:%d pa:%lu ex:%d\n",
      (unsigned long)remain, s->buffer_size, (unsigned long)bytes, exact);

  inLen = bytes;
  in = malloc(inLen);
  if(!in){
    DBG(5, "read_from_scanner_duplex: not enough mem for buffer: %d\n",
      (int)inLen);
    return SANE_STATUS_NO_MEM;
  }

  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd, READ_code);
  set_R_datatype_code (cmd, SR_datatype_image);

  set_R_xfer_length (cmd, inLen);

  ret = do_cmd (
    s, 1, 0,
    cmd, cmdLen,
    NULL, 0,
    in, &inLen
  );

  if (ret == SANE_STATUS_GOOD) {
    DBG(15, "read_from_scanner_duplex: got GOOD, returning GOOD %lu\n", (unsigned long)inLen);
  }
  else if (ret == SANE_STATUS_EOF) {
    DBG(15, "read_from_scanner_duplex: got EOF, finishing %lu\n", (unsigned long)inLen);
  }
  else if (ret == SANE_STATUS_DEVICE_BUSY) {
    DBG(5, "read_from_scanner_duplex: got BUSY, returning GOOD\n");
    inLen = 0;
    ret = SANE_STATUS_GOOD;
  }
  else {
    DBG(5, "read_from_scanner_duplex: error reading data block status = %d\n",
      ret);
    inLen = 0;
  }

  /*scanner may have sent more data than we asked for, chop it*/
  if(inLen > remain){
    inLen = remain;
  }

  /* we've got some data, descramble and store it */
  if(inLen){
    copy_duplex(s,in,inLen);
  }

  free(in);

  /* we've read all data, but not eof. clear and pretend */
  if(exact && inLen == remain){
    DBG (10, "read_from_scanner_duplex: exact read, clearing\n");
    ret = object_position (s,SANE_FALSE);
    if(ret){
      return ret;
    }
    ret = SANE_STATUS_EOF;
  }

  if(ret == SANE_STATUS_EOF){

    /* this is jpeg data, we need to change the total size */
    if(s->s.format == SANE_FRAME_JPEG){
      s->s.bytes_tot[SIDE_FRONT] = s->s.bytes_sent[SIDE_FRONT];
      s->s.bytes_tot[SIDE_BACK] = s->s.bytes_sent[SIDE_BACK];
      s->i.bytes_tot[SIDE_FRONT] = s->i.bytes_sent[SIDE_FRONT];
      s->i.bytes_tot[SIDE_BACK] = s->i.bytes_sent[SIDE_BACK];
      s->u.bytes_tot[SIDE_FRONT] = s->i.bytes_sent[SIDE_FRONT];
      s->u.bytes_tot[SIDE_BACK] = s->i.bytes_sent[SIDE_BACK];
    }

    /* this is non-jpeg data, fill remainder, change rx'd size */
    else{

      DBG (15, "read_from_scanner_duplex: eof: %d %d %d %d\n",
        s->i.bytes_tot[SIDE_FRONT], s->i.bytes_sent[SIDE_FRONT],
        s->i.bytes_tot[SIDE_BACK], s->i.bytes_sent[SIDE_BACK]
      );

      /* clone the last line repeatedly until the end */
      while(s->i.bytes_tot[SIDE_FRONT] > s->i.bytes_sent[SIDE_FRONT]){
        memcpy(
          s->buffers[SIDE_FRONT]+s->i.bytes_sent[SIDE_FRONT]-s->i.Bpl,
          s->buffers[SIDE_FRONT]+s->i.bytes_sent[SIDE_FRONT],
          s->i.Bpl
        );
        s->i.bytes_sent[SIDE_FRONT] += s->i.Bpl;
      }

      /* clone the last line repeatedly until the end */
      while(s->i.bytes_tot[SIDE_BACK] > s->i.bytes_sent[SIDE_BACK]){
        memcpy(
          s->buffers[SIDE_BACK]+s->i.bytes_sent[SIDE_BACK]-s->i.Bpl,
          s->buffers[SIDE_BACK]+s->i.bytes_sent[SIDE_BACK],
          s->i.Bpl
        );
        s->i.bytes_sent[SIDE_BACK] += s->i.Bpl;
      }

      DBG (15, "read_from_scanner_duplex: eof2: %d %d %d %d\n",
        s->i.bytes_tot[SIDE_FRONT], s->i.bytes_sent[SIDE_FRONT],
        s->i.bytes_tot[SIDE_BACK], s->i.bytes_sent[SIDE_BACK]
      );

      /* pretend we got all the data from scanner */
      s->s.bytes_sent[SIDE_FRONT] = s->s.bytes_tot[SIDE_FRONT];
      s->s.bytes_sent[SIDE_BACK] = s->s.bytes_tot[SIDE_BACK];
    }

    s->i.eof[SIDE_FRONT] = 1;
    s->i.eof[SIDE_BACK] = 1;
    s->s.eof[SIDE_FRONT] = 1;
    s->s.eof[SIDE_BACK] = 1;
    ret = SANE_STATUS_GOOD;
  }

  DBG (10, "read_from_scanner_duplex: finish\n");

  return ret;
}

/* these functions copy image data from input buffer to scanner struct 
 * descrambling it, and putting it in the right side buffer */
/* NOTE: they assume buffer is scanline aligned */
static SANE_Status
copy_simplex(struct scanner *s, unsigned char * buf, int len, int side)
{
  SANE_Status ret=SANE_STATUS_GOOD;
  int i, j;
  int bwidth = s->s.Bpl;
  int pwidth = s->s.width;
  int t = bwidth/3;
  int f = bwidth/4;
  int tw = bwidth/12;

  unsigned char * line = NULL;
  int line_next = 0;
  int inter = get_color_inter(s,side,s->s.dpi_x);

  /* jpeg data should not pass thru this function, so copy and bail out */
  if(s->s.format > SANE_FRAME_RGB){
    DBG (15, "copy_simplex: jpeg bulk copy\n");
    memcpy(s->buffers[side]+s->i.bytes_sent[side], buf, len);
    s->i.bytes_sent[side] += len;
    s->s.bytes_sent[side] += len;
    return ret;
  }
  
  DBG (15, "copy_simplex: per-line copy\n");

  line = malloc(bwidth);
  if(!line) return SANE_STATUS_NO_MEM;

  /* ingest each line */
  for(i=0; i<len; i+=bwidth){

    int lineNum = s->s.bytes_sent[side] / bwidth;
  
    /*increment number of bytes rx'd from scanner*/
    s->s.bytes_sent[side] += bwidth;

    /*have some padding from scanner to drop*/
    if ( lineNum < s->i.skip_lines[side]
      || lineNum - s->i.skip_lines[side] >= s->i.height
    ){
      continue;
    }

    line_next = 0;

    if(s->s.format == SANE_FRAME_GRAY){
  
      switch (s->gray_interlace[side]) {
  
        /* one line has the following format: ggg...GGG
         * where the 'capital' letters are the beginning of the line */
        case GRAY_INTERLACE_gG:
          DBG (17, "copy_simplex: gray, gG\n");
          for (j=bwidth-1; j>=0; j--){
            line[line_next++] = buf[i+j];
          }
          break;
    
        case GRAY_INTERLACE_2510:
          DBG (17, "copy_simplex: gray, 2510\n");
       
          /* first read head (third byte of every three) */
          for(j=bwidth-1;j>=0;j-=3){
            line[line_next++] = buf[i+j];
          }
          /* second read head (first byte of every three) */
          for(j=bwidth*3/4-3;j>=0;j-=3){
            line[line_next++] = buf[i+j];
          }
          /* third read head (second byte of every three) */
          for(j=bwidth-2;j>=0;j-=3){
            line[line_next++] = buf[i+j];
          }
          /* padding */
          for(j=0;j<tw;j++){
            line[line_next++] = 0;
          }
          break;
      }
    }
  
    else if (s->s.format == SANE_FRAME_RGB){
  
      switch (inter) {
    
        /* scanner returns color data as bgrbgr... */
        case COLOR_INTERLACE_BGR:
          DBG (17, "copy_simplex: color, BGR\n");
          for (j=0; j<pwidth; j++){
            line[line_next++] = buf[i+j*3+2];
            line[line_next++] = buf[i+j*3+1];
            line[line_next++] = buf[i+j*3];
          }
          break;
    
        /* scanner returns color data as gbrgbr... */
        case COLOR_INTERLACE_GBR:
          DBG (17, "copy_simplex: color, GBR\n");
          for (j=0; j<pwidth; j++){
            line[line_next++] = buf[i+j*3+2];
            line[line_next++] = buf[i+j*3];
            line[line_next++] = buf[i+j*3+1];
          }
          break;
    
        /* scanner returns color data as brgbrg... */
        case COLOR_INTERLACE_BRG:
          DBG (17, "copy_simplex: color, BRG\n");
          for (j=0; j<pwidth; j++){
            line[line_next++] = buf[i+j*3+1];
            line[line_next++] = buf[i+j*3+2];
            line[line_next++] = buf[i+j*3];
          }
          break;
    
        /* one line has the following format: RRR...rrrGGG...gggBBB...bbb */
        case COLOR_INTERLACE_RRGGBB:
          DBG (17, "copy_simplex: color, RRGGBB\n");
          for (j=0; j<pwidth; j++){
            line[line_next++] = buf[i+j];
            line[line_next++] = buf[i+pwidth+j];
            line[line_next++] = buf[i+2*pwidth+j];
          }
          break;
    
        /* one line has the following format: rrr...RRRggg...GGGbbb...BBB
         * where the 'capital' letters are the beginning of the line */
        case COLOR_INTERLACE_rRgGbB:
          DBG (17, "copy_simplex: color, rRgGbB\n");
          for (j=pwidth-1; j>=0; j--){
            line[line_next++] = buf[i+j];
            line[line_next++] = buf[i+pwidth+j];
            line[line_next++] = buf[i+2*pwidth+j];
          }
          break;
    
        case COLOR_INTERLACE_2510:
          DBG (17, "copy_simplex: color, 2510\n");
        
          /* first read head (third byte of every three) */
          for(j=t-1;j>=0;j-=3){
            line[line_next++] = buf[i+j];
            line[line_next++] = buf[i+t+j];
            line[line_next++] = buf[i+2*t+j];
          }
          /* second read head (first byte of every three) */
          for(j=f-3;j>=0;j-=3){
            line[line_next++] = buf[i+j];
            line[line_next++] = buf[i+t+j];
            line[line_next++] = buf[i+2*t+j];
          }
          /* third read head (second byte of every three) */
          for(j=t-2;j>=0;j-=3){
            line[line_next++] = buf[i+j];
            line[line_next++] = buf[i+t+j];
            line[line_next++] = buf[i+2*t+j];
          }
          /* padding */
          for(j=0;j<tw;j++){
            line[line_next++] = 0;
          }
          break;
      }
    }
  
    /* nothing sent above? just copy one line of the block */
    /* used by uninterlaced gray/color */
    if(!line_next){
      DBG (17, "copy_simplex: default\n");
      memcpy(line+line_next,buf+i,bwidth);
      line_next = bwidth;
    }
  
    /* invert image if scanner needs it for this mode */
    if(s->reverse_by_mode[s->s.mode]){
      for(j=0; j<line_next; j++){
        line[j] ^= 0xff;
      }
    }
  
    /* apply calibration if we have it */
    if(s->f_offset[side]){
      DBG (17, "copy_simplex: apply offset\n");
      for(j=0; j<s->s.valid_Bpl; j++){
        int curr = line[j] - s->f_offset[side][j];
        if(curr < 0) curr = 0;
        line[j] = curr;
      }
    }

    if(s->f_gain[side]){
      DBG (17, "copy_simplex: apply gain\n");
      for(j=0; j<s->s.valid_Bpl; j++){
        int curr = line[j] * 240/s->f_gain[side][j];
        if(curr > 255) curr = 255;
        line[j] = curr;
      }
    }
  
    /* apply brightness and contrast if hardware cannot do it */
    if(s->sw_lut && (s->s.mode == MODE_COLOR || s->s.mode == MODE_GRAYSCALE)){
      DBG (17, "copy_simplex: apply brightness/contrast\n");
      for(j=0; j<s->s.valid_Bpl; j++){
        line[j] = s->lut[line[j]];
      }
    }

    /*copy the line into the buffer*/
    ret = copy_line(s,line,side);
    if(ret){
      break;
    }
  }

  free(line);

  DBG (10, "copy_simplex: finished\n");

  return ret;
}

/* split the data between two buffers, hand them to copy_simplex()
 * assumes that the buffer aligns to a double-wide line boundary */
static SANE_Status
copy_duplex(struct scanner *s, unsigned char * buf, int len)
{
  SANE_Status ret=SANE_STATUS_GOOD;
  int i,j;
  int bwidth = s->s.Bpl;
  int dbwidth = 2*bwidth;
  unsigned char * front;
  unsigned char * back;
  int flen=0, blen=0;

  DBG (10, "copy_duplex: start\n");

  /*split the input into two simplex output buffers*/
  front = calloc(1,len/2);
  if(!front){
    DBG (5, "copy_duplex: no front mem\n");
    return SANE_STATUS_NO_MEM;
  }
  back = calloc(1,len/2);
  if(!back){
    DBG (5, "copy_duplex: no back mem\n");
    free(front);
    return SANE_STATUS_NO_MEM;
  }

  if(s->duplex_interlace == DUPLEX_INTERLACE_2510){

    DBG (10, "copy_duplex: 2510\n");

    for(i=0; i<len; i+=dbwidth){
  
      for(j=0;j<dbwidth;j+=6){

        /* we are actually only partially descrambling,
         * copy_simplex() does the rest */

        /* front */
        /* 2nd head: 2nd byte -> 1st byte */
        /* 3rd head: 4th byte -> 2nd byte */
        /* 1st head: 5th byte -> 3rd byte */
        front[flen++] = buf[i+j+2];
        front[flen++] = buf[i+j+4];
        front[flen++] = buf[i+j+5];

        /* back */
        /* 2nd head: 3rd byte -> 1st byte */
        /* 3rd head: 0th byte -> 2nd byte */
        /* 1st head: 1st byte -> 3rd byte */
        back[blen++] = buf[i+j+3];
        back[blen++] = buf[i+j];
        back[blen++] = buf[i+j+1];
      }
    }
  }

  /* full line of front, then full line of back */
  else if(s->duplex_interlace == DUPLEX_INTERLACE_FFBB){
    for(i=0; i<len; i+=dbwidth){
      memcpy(front+flen,buf+i,bwidth);
      flen+=bwidth;
      memcpy(back+blen,buf+i+bwidth,bwidth);
      blen+=bwidth;
    }
  }

  /*just alternating bytes, FBFBFB*/
  else {
    for(i=0; i<len; i+=2){
      front[flen++] = buf[i];
      back[blen++] = buf[i+1];
    }
  }

  copy_simplex(s,front,flen,SIDE_FRONT);
  copy_simplex(s,back,blen,SIDE_BACK);

  free(front);
  free(back);

  DBG (10, "copy_duplex: finished\n");

  return ret;
}

/* downsample a single line from scanner's size to user's size */
/* and copy into final buffer */
static SANE_Status
copy_line(struct scanner *s, unsigned char * buff, int side)
{
  SANE_Status ret=SANE_STATUS_GOOD;
  int spwidth = s->s.width;
  int sbwidth = s->s.Bpl;
  int ibwidth = s->i.Bpl;
  unsigned char * line;
  int offset = 0;
  int i, j;

  DBG (20, "copy_line: start\n");

  /* the 'standard' case: non-stupid scan */
  if(s->s.width == s->i.width
    && s->s.dpi_x == s->i.dpi_x
    && s->s.mode == s->i.mode
  ){

    memcpy(s->buffers[side]+s->i.bytes_sent[side], buff, sbwidth);
    s->i.bytes_sent[side] += sbwidth;

    DBG (20, "copy_line: finished smart\n");
    return ret;
  }

  /* the 'corner' case: stupid scan */

  /*setup 24 bit color single line buffer*/
  line = malloc(spwidth*3);
  if(!line) return SANE_STATUS_NO_MEM;

  /*load single line color buffer*/
  switch (s->s.mode) {

    case MODE_COLOR:
      memcpy(line, buff, sbwidth);
      break;

    case MODE_GRAYSCALE:
      for(i=0;i<spwidth;i++){
        line[i*3] = line[i*3+1] = line[i*3+2] = buff[i];
      }
      break;

    default:
      for(i=0;i<sbwidth;i++){
        unsigned char curr = buff[i];

        line[i*24+0] = line[i*24+1] = line[i*24+2] = ((curr >> 7) & 1) ?0:255;
        line[i*24+3] = line[i*24+4] = line[i*24+5] = ((curr >> 6) & 1) ?0:255;
        line[i*24+6] = line[i*24+7] = line[i*24+8] = ((curr >> 5) & 1) ?0:255;
        line[i*24+9] = line[i*24+10] = line[i*24+11] = ((curr >> 4) & 1) ?0:255;
        line[i*24+12] = line[i*24+13] = line[i*24+14] =((curr >> 3) & 1) ?0:255;
        line[i*24+15] = line[i*24+16] = line[i*24+17] =((curr >> 2) & 1) ?0:255;
        line[i*24+18] = line[i*24+19] = line[i*24+20] =((curr >> 1) & 1) ?0:255;
        line[i*24+21] = line[i*24+22] = line[i*24+23] =((curr >> 0) & 1) ?0:255;
      }
      break;
  }

  /* scan is higher res than user wanted, scale it */
  /*FIXME: interpolate instead */
  if(s->i.dpi_x != s->s.dpi_x){
    for(i=0;i<spwidth;i++){
      int source = i * s->s.dpi_x/s->i.dpi_x * 3;

      if(source+2 >= spwidth*3)
        break;

      line[i*3] = line[source];
      line[i*3+1] = line[source+1];
      line[i*3+2] = line[source+2];
    }
  }
  
  /* scan is wider than user wanted, skip some pixels on left side */
  if(s->i.width != s->s.width){
    offset = ((s->valid_x-s->i.page_x) / 2 + s->i.tl_x) * s->i.dpi_x/1200;
  }

  /* change mode, store line in buffer */
  switch (s->i.mode) {
  
    case MODE_COLOR:
      memcpy(s->buffers[side]+s->i.bytes_sent[side], line+offset, ibwidth);
      s->i.bytes_sent[side] += ibwidth;
      break;
  
    case MODE_GRAYSCALE:
      for(i=0;i<ibwidth;i++){
        int source = (offset+i)*3;
        s->buffers[side][s->i.bytes_sent[side]++]
          = ((int)line[source] + line[source+1] + line[source+2])/3;
      }
      break;
  
    default:
      /*loop over output bytes*/
      for(i=0;i<ibwidth;i++){

        unsigned char curr = 0;
        int thresh = s->threshold*3;

        /*loop over output bits*/
        for(j=0;j<8;j++){
          int source = offset*3 + i*24 + j*3;
          if( (line[source] + line[source+1] + line[source+2]) < thresh ){
            curr |= 1 << (7-j); 
          }
        }

        s->buffers[side][s->i.bytes_sent[side]++] = curr;
      }
      break;
  }

  free(line);

  DBG (20, "copy_line: finish stupid\n");

  return ret;
}

static SANE_Status
read_from_buffer(struct scanner *s, SANE_Byte * buf, SANE_Int max_len,
  SANE_Int * len, int side)
{
  SANE_Status ret=SANE_STATUS_GOOD;
  int bytes = max_len;
  int remain = s->i.bytes_sent[side] - s->u.bytes_sent[side];

  DBG (10, "read_from_buffer: start\n");

  /* figure out the max amount to transfer */
  if(bytes > remain)
    bytes = remain;
  
  *len = bytes;
  
  /*FIXME this needs to timeout eventually */
  if(!bytes){
    DBG(5,"read_from_buffer: nothing to do\n");
    return SANE_STATUS_GOOD;
  }
  
  DBG(15, "read_from_buffer: si:%d to:%d tx:%d bu:%d pa:%d\n", side,
    s->i.bytes_tot[side], s->u.bytes_sent[side], max_len, bytes);

  /* copy to caller */
  memcpy(buf,s->buffers[side]+s->u.bytes_sent[side],bytes);
  s->u.bytes_sent[side] += bytes;

  DBG (10, "read_from_buffer: finished\n");

  return ret;
}

/*
 * @@ Section 5 - calibration functions
 */

#if 0
static SANE_Status
foo_AFE(struct scanner *s)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  unsigned char cmd[] = {
    0x3b, 0x00, 0x10, 0x08, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00
  };
  size_t cmdLen = 12;

  unsigned char in[4];
  size_t inLen = 4;

  DBG (10, "foo_AFE: start\n");

  ret = do_cmd (
    s, 1, 0,
    cmd, cmdLen,
    NULL, 0,
    in, &inLen
  );
  if (ret != SANE_STATUS_GOOD)
    return ret;

  DBG (10, "foo_AFE: finish\n");

  return ret;
}
#endif

/*
 * makes several scans, adjusts coarse calibration
 */
static SANE_Status
calibrate_AFE (struct scanner *s)
{
  SANE_Status ret = SANE_STATUS_GOOD;
  int i, j, k;
  int min, max;
  int lines = 8;

  /*buffer these for later*/
  int old_tl_y = s->u.tl_y;
  int old_br_y = s->u.br_y;
  int old_mode = s->u.mode;
  int old_source = s->u.source;

  DBG (10, "calibrate_AFE: start\n");

  if(!s->need_ccal){
    DBG (10, "calibrate_AFE: not required\n");
    return ret;
  }

  /* always cal with a short scan in duplex color */
  s->u.tl_y = 0;
  s->u.br_y = lines * 1200 / s->u.dpi_y;
  s->u.mode = MODE_COLOR;
  s->u.source = SOURCE_ADF_DUPLEX;

  /* load our own private copy of scan params */
  ret = update_params(s,1);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_AFE: ERROR: cannot update_params\n");
    goto cleanup;
  }

  if(s->c_res == s->s.dpi_x && s->c_mode == s->s.mode){
    DBG (10, "calibrate_AFE: already done\n");
    goto cleanup;
  }

  /* clean scan params for new scan */
  ret = clean_params(s);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_AFE: ERROR: cannot clean_params\n");
    goto cleanup;
  }

  /* make buffers to hold the images */
  ret = image_buffers(s,1);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_AFE: ERROR: cannot load buffers\n");
    goto cleanup;
  }

  /*blast the existing fine cal data so reading code wont apply it*/
  ret = offset_buffers(s,0);
  ret = gain_buffers(s,0);

  /* need to tell it we want duplex */
  ret = ssm_buffer(s);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_AFE: ERROR: cannot ssm buffer\n");
    goto cleanup;
  }

  /* set window command */
  ret = set_window(s);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_AFE: ERROR: cannot set window\n");
    goto cleanup;
  }
  
  /* first pass (black offset), lamp off, no offset/gain/exposure */
  DBG (15, "calibrate_AFE: offset\n");

  /* blast all the existing coarse cal data */
  for(i=0;i<2;i++){
    s->c_gain[i]   = 1;
    s->c_offset[i] = 1;
    for(j=0;j<3;j++){
      s->c_exposure[i][j] = 0;
    }
  }

  ret = write_AFE(s);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_AFE: ERROR: cannot write afe\n");
    goto cleanup;
  }

  ret = calibration_scan(s,0xff);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_AFE: ERROR: cannot make offset cal scan\n");
    goto cleanup;
  }

  for(i=0;i<2;i++){
    min = 255;
    for(j=0; j<s->s.valid_Bpl; j++){
      if(s->buffers[i][j] < min)
        min = s->buffers[i][j];
    }
    s->c_offset[i] = min*3-2;
    DBG (15, "calibrate_AFE: offset %d %d %02x\n", i, min, s->c_offset[i]);
  }

  /*handle second pass (per channel exposure), lamp on, overexposed*/
  DBG (15, "calibrate_AFE: exposure\n");
  for(i=0;i<2;i++){
    for(j=0; j<3; j++){
      s->c_exposure[i][j] = 0x320;
    }
  }

  ret = write_AFE(s);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_AFE: ERROR: cannot write afe\n");
    goto cleanup;
  }

  ret = calibration_scan(s,0xfe);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_AFE: ERROR: cannot make exposure cal scan\n");
    goto cleanup;
  }

  for(i=0;i<2;i++){ /*sides*/
    for(j=0;j<3;j++){ /*channels*/
      max = 0;
      for(k=j; k<s->s.valid_Bpl; k+=3){ /*bytes*/
        if(s->buffers[i][k] > max)
          max = s->buffers[i][k];
      }

      /*generally we reduce the exposure (smaller number) */
      if(old_mode == MODE_COLOR)
        s->c_exposure[i][j] = s->c_exposure[i][j] * 102/max;
      else
        s->c_exposure[i][j] = s->c_exposure[i][j] * 64/max;

      DBG (15, "calibrate_AFE: exp %d %d %d %02x\n", i, j, max,
        s->c_exposure[i][j]);
    }
  }

  /*handle third pass (gain), lamp on with current offset/exposure */
  DBG (15, "calibrate_AFE: gain\n");

  ret = write_AFE(s);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_AFE: ERROR: cannot write afe\n");
    goto cleanup;
  }

  ret = calibration_scan(s,0xfe);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_AFE: ERROR: cannot make gain cal scan\n");
    goto cleanup;
  }

  for(i=0;i<2;i++){
    max = 0;
    for(j=0; j<s->s.valid_Bpl; j++){
      if(s->buffers[i][j] > max)
        max = s->buffers[i][j];
    }

    if(old_mode == MODE_COLOR)
      s->c_gain[i] = (250-max)*4/5;
    else
      s->c_gain[i] = (125-max)*4/5;

    if(s->c_gain[i] < 1)
      s->c_gain[i] = 1;

    DBG (15, "calibrate_AFE: gain %d %d %02x\n", i, max, s->c_gain[i]);
  }

  /*handle fourth pass (offset again), lamp off*/
#if 0
  DBG (15, "calibrate_AFE: offset2\n");

  ret = write_AFE(s);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_AFE: ERROR: cannot write afe\n");
    goto cleanup;
  }

  ret = calibration_scan(s,0xff);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_AFE: ERROR: cannot make offset2 cal scan\n");
    goto cleanup;
  }

  for(i=0;i<2;i++){
    min = 255;
    for(j=0; j<s->s.valid_Bpl; j++){
      if(s->buffers[i][j] < min)
        min = s->buffers[i][j];
    }
    /*s->c_offset[i] += min*3-2;*/
    DBG (15, "calibrate_AFE: offset2 %d %d %02x\n", i, min, s->c_offset[i]);
  }
#endif

  /*send final afe params to scanner*/
  ret = write_AFE(s);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_AFE: ERROR: cannot write afe\n");
    goto cleanup;
  }

  /* log current cal type */
  s->c_res = s->s.dpi_x;
  s->c_mode = s->s.mode;

  cleanup:

  /* recover user settings */
  s->u.tl_y = old_tl_y;
  s->u.br_y = old_br_y;
  s->u.mode = old_mode;
  s->u.source = old_source;

  DBG (10, "calibrate_AFE: finish %d\n",ret);

  return ret;
}


/* alternative version- extracts data from scanner memory */
static SANE_Status
calibrate_fine_buffer (struct scanner *s)
{
  SANE_Status ret = SANE_STATUS_GOOD;
  int i, j, k;

  unsigned char cmd[READ_len];
  size_t cmdLen = READ_len;

  unsigned char * in = NULL;
  size_t inLen = 0, reqLen = 0;

  /*buffer these for later*/
  int old_tl_y = s->u.tl_y;
  int old_br_y = s->u.br_y;
  int old_source = s->u.source;

  DBG (10, "calibrate_fine_buffer: start\n");

  if(!s->need_fcal_buffer){
    DBG (10, "calibrate_fine_buffer: not required\n");
    return ret;
  }

  /* pretend we are doing a 1 line scan in duplex */
  s->u.tl_y = 0;
  s->u.br_y = 1200 / s->u.dpi_y;
  s->u.source = SOURCE_ADF_DUPLEX;

  /* load our own private copy of scan params */
  ret = update_params(s,1);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_fine_buffer: ERROR: cannot update_params\n");
    goto cleanup;
  }

  if(s->f_res == s->s.dpi_x && s->f_mode == s->s.mode){
    DBG (10, "calibrate_fine_buffer: already done\n");
    goto cleanup;
  }

  /* clean scan params for new scan */
  ret = clean_params(s);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_fine_buffer: ERROR: cannot clean_params\n");
    goto cleanup;
  }

  /*calibration buffers in scanner are single color channel, but duplex*/
  reqLen = s->s.width*2;

  in = malloc(reqLen);
  if (!in) {
    DBG (5, "calibrate_fine_buffer: ERROR: cannot malloc in\n");
    ret = SANE_STATUS_NO_MEM;
    goto cleanup;
  }

  /*fine offset*/
  ret = offset_buffers(s,1);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_fine_buffer: ERROR: cannot load offset buffers\n");
    goto cleanup;
  }
 
  DBG (5, "calibrate_fine_buffer: %d %x\n", s->s.dpi_x/10, s->s.dpi_x/10);

  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd, READ_code);
  set_R_datatype_code (cmd, SR_datatype_fineoffset);
  set_R_xfer_lid (cmd, s->s.dpi_x/10);
  set_R_xfer_length (cmd, reqLen);

  inLen = reqLen;

  hexdump(15, "cmd:", cmd, cmdLen);

  ret = do_cmd (
    s, 1, 0,
    cmd, cmdLen,
    NULL, 0,
    in, &inLen
  );
  if (ret != SANE_STATUS_GOOD)
    goto cleanup;

  for(i=0;i<2;i++){

    /*color mode, expand offset across all three channels? */
    if(s->s.format == SANE_FRAME_RGB){
      for(j=0; j<s->s.valid_width; j++){
        
        /*red*/  
        s->f_offset[i][j*3] = in[j*2+i];
        if(s->f_offset[i][j*3] < 1)
          s->f_offset[i][j*3] = 1;

        /*green and blue, same as red*/  
        s->f_offset[i][j*3+1] = s->f_offset[i][j*3+2] = s->f_offset[i][j*3];
      }
    }

    /*gray mode, copy*/
    else{
      for(j=0; j<s->s.valid_width; j++){

        s->f_offset[i][j] = in[j*2+i];
        if(s->f_offset[i][j] < 1)
          s->f_offset[i][j] = 1;
      }
    }

    hexdump(15, "off:", s->f_offset[i], s->s.valid_Bpl);
  }

  /*fine gain*/
  ret = gain_buffers(s,1);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_fine_buffer: ERROR: cannot load gain buffers\n");
    goto cleanup;
  }

  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd, READ_code);
  set_R_datatype_code (cmd, SR_datatype_finegain);
  set_R_xfer_lid (cmd, s->s.dpi_x/10);
  set_R_xfer_length (cmd, reqLen);

  /*color gain split into three buffers, grab them and merge*/
  if(s->s.format == SANE_FRAME_RGB){

    int codes[] = {R_FINE_uid_red,R_FINE_uid_green,R_FINE_uid_blue};

    for(k=0;k<3;k++){
  
      set_R_xfer_uid (cmd, codes[k]);
      inLen = reqLen;
    
      hexdump(15, "cmd:", cmd, cmdLen);

      ret = do_cmd (
        s, 1, 0,
        cmd, cmdLen,
        NULL, 0,
        in, &inLen
      );
      if (ret != SANE_STATUS_GOOD)
        goto cleanup;
  
      for(i=0;i<2;i++){
        for(j=0; j<s->s.valid_width; j++){
          
          s->f_gain[i][j*3+k] = in[j*2+i]*3/4;
    
          if(s->f_gain[i][j*3+k] < 1)
            s->f_gain[i][j*3+k] = 1;
        }
      }
    }
  }

  /*gray gain, copy*/
  else{

    set_R_xfer_uid (cmd, R_FINE_uid_gray);
    inLen = reqLen;
    
    hexdump(15, "cmd:", cmd, cmdLen);

    ret = do_cmd (
      s, 1, 0,
      cmd, cmdLen,
      NULL, 0,
      in, &inLen
    );
    if (ret != SANE_STATUS_GOOD)
      goto cleanup;
  
    for(i=0;i<2;i++){
      for(j=0; j<s->s.valid_width; j++){
        
        s->f_gain[i][j] = in[j*2+i]*3/4;
    
        if(s->f_gain[i][j] < 1)
          s->f_gain[i][j] = 1;
      }
    }
  }

  for(i=0;i<2;i++){
    hexdump(15, "gain:", s->f_gain[i], s->s.valid_Bpl);
  }

  /* log current cal type */
  s->f_res = s->s.dpi_x;
  s->f_mode = s->s.mode;

  cleanup:

  if(in){
    free(in);
  }

  /* recover user settings */
  s->u.tl_y = old_tl_y;
  s->u.br_y = old_br_y;
  s->u.source = old_source;

  DBG (10, "calibrate_fine_buffer: finish %d\n",ret);

  return ret;
}

/*
 * makes several scans, adjusts fine calibration
 */
static SANE_Status
calibrate_fine (struct scanner *s)
{
  SANE_Status ret = SANE_STATUS_GOOD;
  int i, j, k;
  int min, max;
  int lines = 8;

  /*buffer these for later*/
  int old_tl_y = s->u.tl_y;
  int old_br_y = s->u.br_y;
  int old_source = s->u.source;

  DBG (10, "calibrate_fine: start\n");

  if(!s->need_fcal){
    DBG (10, "calibrate_fine: not required\n");
    return ret;
  }

  /* always cal with a short scan in duplex */
  s->u.tl_y = 0;
  s->u.br_y = lines * 1200 / s->u.dpi_y;
  s->u.source = SOURCE_ADF_DUPLEX;

  /* load our own private copy of scan params */
  ret = update_params(s,1);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_fine: ERROR: cannot update_params\n");
    goto cleanup;
  }

  if(s->f_res == s->s.dpi_x && s->f_mode == s->s.mode){
    DBG (10, "calibrate_fine: already done\n");
    goto cleanup;
  }

  /* clean scan params for new scan */
  ret = clean_params(s);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibration_fine: ERROR: cannot clean_params\n");
    goto cleanup;
  }

  /* make buffers to hold the images */
  ret = image_buffers(s,1);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_fine: ERROR: cannot load buffers\n");
    goto cleanup;
  }
  
  /*blast the existing fine cal data so reading code wont apply it*/
  ret = offset_buffers(s,0);
  ret = gain_buffers(s,0);

  /* need to tell it we want duplex */
  ret = ssm_buffer(s);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_fine: ERROR: cannot ssm buffer\n");
    goto cleanup;
  }
  
  /* set window command */
  ret = set_window(s);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_fine: ERROR: cannot set window\n");
    goto cleanup;
  }
  
  /*handle fifth pass (fine offset), lamp off*/
  DBG (15, "calibrate_fine: offset\n");
  ret = calibration_scan(s,0xff);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_fine: ERROR: cannot make offset cal scan\n");
    goto cleanup;
  }

  ret = offset_buffers(s,1);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_fine: ERROR: cannot load offset buffers\n");
    goto cleanup;
  }
  
  for(i=0;i<2;i++){
    for(j=0; j<s->s.valid_Bpl; j++){
      min = 0;
      for(k=j;k<lines*s->s.Bpl;k+=s->s.Bpl){
        min += s->buffers[i][k];
      }
      s->f_offset[i][j] = min/lines;
    }
    hexdump(15, "off:", s->f_offset[i], s->s.valid_Bpl);
  }

  /*handle sixth pass (fine gain), lamp on*/
  DBG (15, "calibrate_fine: gain\n");
  ret = calibration_scan(s,0xfe);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_fine: ERROR: cannot make gain cal scan\n");
    goto cleanup;
  }

  ret = gain_buffers(s,1);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibrate_fine: ERROR: cannot load gain buffers\n");
    goto cleanup;
  }
  
  for(i=0;i<2;i++){
    for(j=0; j<s->s.valid_Bpl; j++){
      max = 0;
      for(k=j;k<lines*s->s.Bpl;k+=s->s.Bpl){
        max += s->buffers[i][k];
      }
      s->f_gain[i][j] = max/lines;

      if(s->f_gain[i][j] < 1)
        s->f_gain[i][j] = 1;
    }
    hexdump(15, "gain:", s->f_gain[i], s->s.valid_Bpl);
  }

  /* log current cal type */
  s->f_res = s->s.dpi_x;
  s->f_mode = s->s.mode;

  cleanup:

  /* recover user settings */
  s->u.tl_y = old_tl_y;
  s->u.br_y = old_br_y;
  s->u.source = old_source;

  DBG (10, "calibrate_fine: finish %d\n",ret);

  return ret;
}

/*
 * sends AFE params, and ingests entire duplex image into buffers
 */
static SANE_Status
calibration_scan (struct scanner *s, int scan)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  DBG (10, "calibration_scan: start\n");

  /* clean scan params for new scan */
  ret = clean_params(s);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibration_scan: ERROR: cannot clean_params\n");
    return ret;
  }
  
  /* start scanning */
  ret = start_scan (s,scan);
  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "calibration_scan: ERROR: cannot start_scan\n");
    return ret;
  }
  
  while(!s->s.eof[SIDE_FRONT] && !s->s.eof[SIDE_BACK]){
    ret = read_from_scanner_duplex(s,1);
  }

  DBG (10, "calibration_scan: finished\n");

  return ret;
}

/*
 * sends AFE and exposure params
 */
static SANE_Status
write_AFE(struct scanner *s)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  unsigned char cmd[COR_CAL_len];
  size_t cmdLen = COR_CAL_len;

  /*use the longest payload for buffer*/
  unsigned char pay[CC3_pay_len]; 
  size_t payLen = CC3_pay_len;

  DBG (10, "write_AFE: start\n");

  /* newer scanners use a longer cc payload */
  if(s->ccal_version == 3){

    memset(cmd,0,cmdLen);
    set_SCSI_opcode(cmd, COR_CAL_code);
    set_CC_version(cmd,CC3_pay_ver);
    set_CC_xferlen(cmd,payLen);
  
    memset(pay,0,payLen);

    set_CC3_gain_f_r(pay,s->c_gain[SIDE_FRONT]);
    set_CC3_gain_f_g(pay,s->c_gain[SIDE_FRONT]);
    set_CC3_gain_f_b(pay,s->c_gain[SIDE_FRONT]);

    set_CC3_off_f_r(pay,s->c_offset[SIDE_FRONT]);
    set_CC3_off_f_g(pay,s->c_offset[SIDE_FRONT]);
    set_CC3_off_f_b(pay,s->c_offset[SIDE_FRONT]);
  
    set_CC3_exp_f_r(pay,s->c_exposure[SIDE_FRONT][CHAN_RED]);
    set_CC3_exp_f_g(pay,s->c_exposure[SIDE_FRONT][CHAN_GREEN]);
    set_CC3_exp_f_b(pay,s->c_exposure[SIDE_FRONT][CHAN_BLUE]);

    set_CC3_gain_b_r(pay,s->c_gain[SIDE_BACK]);
    set_CC3_gain_b_g(pay,s->c_gain[SIDE_BACK]);
    set_CC3_gain_b_b(pay,s->c_gain[SIDE_BACK]);

    set_CC3_off_b_r(pay,s->c_offset[SIDE_BACK]);
    set_CC3_off_b_g(pay,s->c_offset[SIDE_BACK]);
    set_CC3_off_b_b(pay,s->c_offset[SIDE_BACK]);

    set_CC3_exp_b_r(pay,s->c_exposure[SIDE_BACK][CHAN_RED]);
    set_CC3_exp_b_g(pay,s->c_exposure[SIDE_BACK][CHAN_GREEN]);
    set_CC3_exp_b_b(pay,s->c_exposure[SIDE_BACK][CHAN_BLUE]);
  }

  else{
    payLen = CC_pay_len;

    memset(cmd,0,cmdLen);
    set_SCSI_opcode(cmd, COR_CAL_code);
    set_CC_version(cmd,CC_pay_ver);
    set_CC_xferlen(cmd,payLen);
  
    memset(pay,0,payLen);
    set_CC_f_gain(pay,s->c_gain[SIDE_FRONT]);
    set_CC_unk1(pay,1);
    set_CC_f_offset(pay,s->c_offset[SIDE_FRONT]);
    set_CC_unk2(pay,1);
    set_CC_exp_f_r1(pay,s->c_exposure[SIDE_FRONT][CHAN_RED]);
    set_CC_exp_f_g1(pay,s->c_exposure[SIDE_FRONT][CHAN_GREEN]);
    set_CC_exp_f_b1(pay,s->c_exposure[SIDE_FRONT][CHAN_BLUE]);
    set_CC_exp_f_r2(pay,s->c_exposure[SIDE_FRONT][CHAN_RED]);
    set_CC_exp_f_g2(pay,s->c_exposure[SIDE_FRONT][CHAN_GREEN]);
    set_CC_exp_f_b2(pay,s->c_exposure[SIDE_FRONT][CHAN_BLUE]);
  
    set_CC_b_gain(pay,s->c_gain[SIDE_BACK]);
    set_CC_b_offset(pay,s->c_offset[SIDE_BACK]);
    set_CC_exp_b_r1(pay,s->c_exposure[SIDE_BACK][CHAN_RED]);
    set_CC_exp_b_g1(pay,s->c_exposure[SIDE_BACK][CHAN_GREEN]);
    set_CC_exp_b_b1(pay,s->c_exposure[SIDE_BACK][CHAN_BLUE]);
    set_CC_exp_b_r2(pay,s->c_exposure[SIDE_BACK][CHAN_RED]);
    set_CC_exp_b_g2(pay,s->c_exposure[SIDE_BACK][CHAN_GREEN]);
    set_CC_exp_b_b2(pay,s->c_exposure[SIDE_BACK][CHAN_BLUE]);
  }

  ret = do_cmd (
    s, 1, 0,
    cmd, cmdLen,
    pay, payLen,
    NULL, NULL
  );
  if (ret != SANE_STATUS_GOOD)
    return ret;

  DBG (10, "write_AFE: finish\n");

  return ret;
}

/*
 * frees/callocs buffers to hold the fine cal offset data
 */
static SANE_Status
offset_buffers (struct scanner *s, int setup)
{
  SANE_Status ret = SANE_STATUS_GOOD;
  int side;

  DBG (10, "offset_buffers: start\n");

  for(side=0;side<2;side++){

    if (s->f_offset[side]) {
      DBG (15, "offset_buffers: free f_offset %d.\n",side);
      free(s->f_offset[side]);
      s->f_offset[side] = NULL;
    }

    if(setup){
      s->f_offset[side] = calloc (1,s->s.Bpl);
      if (!s->f_offset[side]) {
        DBG (5, "offset_buffers: error, no f_offset %d.\n",side);
        return SANE_STATUS_NO_MEM;
      }
    }
  }

  DBG (10, "offset_buffers: finish\n");

  return ret;
}

/*
 * frees/callocs buffers to hold the fine cal gain data
 */
static SANE_Status
gain_buffers (struct scanner *s, int setup)
{
  SANE_Status ret = SANE_STATUS_GOOD;
  int side;

  DBG (10, "gain_buffers: start\n");

  for(side=0;side<2;side++){

    if (s->f_gain[side]) {
      DBG (15, "gain_buffers: free f_gain %d.\n",side);
      free(s->f_gain[side]);
      s->f_gain[side] = NULL;
    }

    if(setup){
      s->f_gain[side] = calloc (1,s->s.Bpl);
      if (!s->f_gain[side]) {
        DBG (5, "gain_buffers: error, no f_gain %d.\n",side);
        return SANE_STATUS_NO_MEM;
      }
    }
  }

  DBG (10, "gain_buffers: finish\n");

  return ret;
}

/*
 * @@ Section 6 - SANE cleanup functions
 */
/*
 * Cancels a scan. 
 *
 * It has been said on the mailing list that sane_cancel is a bit of a
 * misnomer because it is routinely called to signal the end of a
 * batch - quoting David Mosberger-Tang:
 * 
 * > In other words, the idea is to have sane_start() be called, and
 * > collect as many images as the frontend wants (which could in turn
 * > consist of multiple frames each as indicated by frame-type) and
 * > when the frontend is done, it should call sane_cancel(). 
 * > Sometimes it's better to think of sane_cancel() as "sane_stop()"
 * > but that name would have had some misleading connotations as
 * > well, that's why we stuck with "cancel".
 * 
 * The current consensus regarding duplex and ADF scans seems to be
 * the following call sequence: sane_start; sane_read (repeat until
 * EOF); sane_start; sane_read...  and then call sane_cancel if the
 * batch is at an end. I.e. do not call sane_cancel during the run but
 * as soon as you get a SANE_STATUS_NO_DOCS.
 * 
 * From the SANE spec:
 * This function is used to immediately or as quickly as possible
 * cancel the currently pending operation of the device represented by
 * handle h.  This function can be called at any time (as long as
 * handle h is a valid handle) but usually affects long-running
 * operations only (such as image acquisition). It is safe to call
 * this function asynchronously (e.g., from within a signal handler).
 * It is important to note that completion of this operaton does not
 * imply that the currently pending operation has been cancelled. It
 * only guarantees that cancellation has been initiated. Cancellation
 * completes only when the cancelled call returns (typically with a
 * status value of SANE_STATUS_CANCELLED).  Since the SANE API does
 * not require any other operations to be re-entrant, this implies
 * that a frontend must not call any other operation until the
 * cancelled operation has returned.
 */
void
sane_cancel (SANE_Handle handle)
{
  struct scanner * s = (struct scanner *) handle;

  DBG (10, "sane_cancel: start\n");
  s->cancelled = 1;

  /* if there is no other running function to check, we do it */
  if(!s->reading)
    check_for_cancel(s);

  DBG (10, "sane_cancel: finish\n");
}

/* checks started and cancelled flags in scanner struct,
 * sends cancel command to scanner if required. don't call
 * this function asyncronously, wait for pending operation */
static SANE_Status
check_for_cancel(struct scanner *s)
{
  SANE_Status ret=SANE_STATUS_GOOD;

  DBG (10, "check_for_cancel: start\n");

  if(s->started && s->cancelled){
    unsigned char cmd[CANCEL_len];
    size_t cmdLen = CANCEL_len;
  
    DBG (15, "check_for_cancel: cancelling\n");
  
    /* cancel scan */
    memset(cmd,0,cmdLen);
    set_SCSI_opcode(cmd, CANCEL_code);
  
    ret = do_cmd (
        s, 1, 0,
        cmd, cmdLen,
        NULL, 0,
        NULL, NULL
    );
    if(ret){
      DBG (5, "check_for_cancel: ignoring bad cancel: %d\n",ret);
    }
  
    ret = object_position(s,SANE_FALSE);
    if(ret){
      DBG (5, "check_for_cancel: ignoring bad eject: %d\n",ret);
    }

    s->started = 0;
    s->cancelled = 0;
    ret = SANE_STATUS_CANCELLED;
  }
  else if(s->cancelled){
    DBG (15, "check_for_cancel: already cancelled\n");
    s->cancelled = 0;
    ret = SANE_STATUS_CANCELLED;
  }

  DBG (10, "check_for_cancel: finish %d\n",ret);
  return ret;
}

/*
 * Ends use of the scanner.
 * 
 * From the SANE spec:
 * This function terminates the association between the device handle
 * passed in argument h and the device it represents. If the device is
 * presently active, a call to sane_cancel() is performed first. After
 * this function returns, handle h must not be used anymore.
 */
void
sane_close (SANE_Handle handle)
{
  struct scanner * s = (struct scanner *) handle;

  DBG (10, "sane_close: start\n");
  disconnect_fd(s);
  image_buffers(s,0);
  offset_buffers(s,0);
  gain_buffers(s,0);
  DBG (10, "sane_close: finish\n");
}

static SANE_Status
disconnect_fd (struct scanner *s)
{
  DBG (10, "disconnect_fd: start\n");

  if(s->fd > -1){
    if (s->connection == CONNECTION_USB) {
      DBG (15, "disconnecting usb device\n");
      sanei_usb_close (s->fd);
    }
    else if (s->connection == CONNECTION_SCSI) {
      DBG (15, "disconnecting scsi device\n");
      sanei_scsi_close (s->fd);
    }
    s->fd = -1;
  }

  DBG (10, "disconnect_fd: finish\n");

  return SANE_STATUS_GOOD;
}

/*
 * Terminates the backend.
 * 
 * From the SANE spec:
 * This function must be called to terminate use of a backend. The
 * function will first close all device handles that still might be
 * open (it is recommended to close device handles explicitly through
 * a call to sane_close(), but backends are required to release all
 * resources upon a call to this function). After this function
 * returns, no function other than sane_init() may be called
 * (regardless of the status value returned by sane_exit(). Neglecting
 * to call this function may result in some resources not being
 * released properly.
 */
void
sane_exit (void)
{
  struct scanner *dev, *next;

  DBG (10, "sane_exit: start\n");

  for (dev = scanner_devList; dev; dev = next) {
      disconnect_fd(dev);
      next = dev->next;
      free (dev);
  }

  if (sane_devArray)
    free (sane_devArray);

  scanner_devList = NULL;
  sane_devArray = NULL;

  DBG (10, "sane_exit: finish\n");
}


/*
 * @@ Section 7 - misc helper functions
 */
static void
default_globals(void)
{
  global_buffer_size = global_buffer_size_default;
  global_padded_read = global_padded_read_default;
  global_extra_status = global_extra_status_default;
  global_duplex_offset = global_duplex_offset_default;
  global_vendor_name[0] = 0;
  global_model_name[0] = 0;
  global_version_name[0] = 0;
}

/*
 * Called by the SANE SCSI core and our usb code on device errors
 * parses the request sense return data buffer,
 * decides the best SANE_Status for the problem, produces debug msgs,
 * and copies the sense buffer into the scanner struct
 */
static SANE_Status
sense_handler (int fd, unsigned char * sensed_data, void *arg)
{
  struct scanner *s = arg;
  unsigned int sense = get_RS_sense_key (sensed_data);
  unsigned int asc = get_RS_ASC (sensed_data);
  unsigned int ascq = get_RS_ASCQ (sensed_data);
  unsigned int eom = get_RS_EOM (sensed_data);
  unsigned int ili = get_RS_ILI (sensed_data);
  unsigned int info = get_RS_information (sensed_data);

  DBG (5, "sense_handler: start\n");

  /* kill compiler warning */
  fd = fd;

  /* copy the rs return data into the scanner struct
     so that the caller can use it if he wants
  memcpy(&s->rs_buffer,sensed_data,RS_return_size);
  */

  DBG (5, "Sense=%#02x, ASC=%#02x, ASCQ=%#02x, EOM=%d, ILI=%d, info=%#08x\n", sense, asc, ascq, eom, ili, info);

  switch (sense) {
    case 0:
      if (ili == 1) {
        s->rs_info = info;
        DBG  (5, "No sense: EOM remainder:%d\n",info);
        return SANE_STATUS_EOF;
      }
      DBG  (5, "No sense: unknown asc/ascq\n");
      return SANE_STATUS_GOOD;

    case 1:
      if (asc == 0x37 && ascq == 0x00) {
        DBG  (5, "Recovered error: parameter rounded\n");
        return SANE_STATUS_GOOD;
      }
      DBG  (5, "Recovered error: unknown asc/ascq\n");
      return SANE_STATUS_GOOD;

    case 2:
      if (asc == 0x04 && ascq == 0x01) {
        DBG  (5, "Not ready: previous command unfinished\n");
        return SANE_STATUS_DEVICE_BUSY;
      }
      DBG  (5, "Not ready: unknown asc/ascq\n");
      return SANE_STATUS_DEVICE_BUSY;

    case 3:
      if (asc == 0x36 && ascq == 0x00) {
        DBG  (5, "Medium error: no cartridge\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (asc == 0x3a && ascq == 0x00) {
        DBG  (5, "Medium error: hopper empty\n");
        return SANE_STATUS_NO_DOCS;
      }
      if (asc == 0x80 && ascq == 0x00) {
        DBG  (5, "Medium error: paper jam\n");
        return SANE_STATUS_JAMMED;
      }
      if (asc == 0x80 && ascq == 0x01) {
        DBG  (5, "Medium error: cover open\n");
        return SANE_STATUS_COVER_OPEN;
      }
      if (asc == 0x81 && ascq == 0x01) {
        DBG  (5, "Medium error: double feed\n");
        return SANE_STATUS_JAMMED;
      }
      if (asc == 0x81 && ascq == 0x02) {
        DBG  (5, "Medium error: skew detected\n");
        return SANE_STATUS_JAMMED;
      }
      if (asc == 0x81 && ascq == 0x04) {
        DBG  (5, "Medium error: staple detected\n");
        return SANE_STATUS_JAMMED;
      }
      DBG  (5, "Medium error: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;

    case 4:
      if (asc == 0x60 && ascq == 0x00) {
        DBG  (5, "Hardware error: lamp error\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (asc == 0x80 && ascq == 0x01) {
        DBG  (5, "Hardware error: CPU check error\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (asc == 0x80 && ascq == 0x02) {
        DBG  (5, "Hardware error: RAM check error\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (asc == 0x80 && ascq == 0x03) {
        DBG  (5, "Hardware error: ROM check error\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (asc == 0x80 && ascq == 0x04) {
        DBG  (5, "Hardware error: hardware check error\n");
        return SANE_STATUS_IO_ERROR;
      }
      DBG  (5, "Hardware error: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;

    case 5:
      if (asc == 0x1a && ascq == 0x00) {
        DBG  (5, "Illegal request: Parameter list error\n");
        return SANE_STATUS_INVAL;
      }
      if (asc == 0x20 && ascq == 0x00) {
        DBG  (5, "Illegal request: invalid command\n");
        return SANE_STATUS_INVAL;
      }
      if (asc == 0x24 && ascq == 0x00) {
        DBG  (5, "Illegal request: invalid CDB field\n");
        return SANE_STATUS_INVAL;
      }
      if (asc == 0x25 && ascq == 0x00) {
        DBG  (5, "Illegal request: unsupported logical unit\n");
        return SANE_STATUS_UNSUPPORTED;
      }
      if (asc == 0x26 && ascq == 0x00) {
        DBG  (5, "Illegal request: invalid field in parm list\n");
        return SANE_STATUS_INVAL;
      }
      if (asc == 0x2c && ascq == 0x00) {
        DBG  (5, "Illegal request: command sequence error\n");
        return SANE_STATUS_INVAL;
      }
      if (asc == 0x2c && ascq == 0x01) {
        DBG  (5, "Illegal request: too many windows\n");
        return SANE_STATUS_INVAL;
      }
      if (asc == 0x3a && ascq == 0x00) {
        DBG  (5, "Illegal request: no paper\n");
        return SANE_STATUS_NO_DOCS;
      }
      if (asc == 0x3d && ascq == 0x00) {
        DBG  (5, "Illegal request: invalid IDENTIFY\n");
        return SANE_STATUS_INVAL;
      }
      if (asc == 0x55 && ascq == 0x00) {
        DBG  (5, "Illegal request: scanner out of memory\n");
        return SANE_STATUS_NO_MEM;
      }
      DBG  (5, "Illegal request: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;
      break;

    case 6:
      if (asc == 0x29 && ascq == 0x00) {
        DBG  (5, "Unit attention: device reset\n");
        return SANE_STATUS_GOOD;
      }
      if (asc == 0x2a && ascq == 0x00) {
        DBG  (5, "Unit attention: param changed by 2nd initiator\n");
        return SANE_STATUS_GOOD;
      }
      DBG  (5, "Unit attention: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;
      break;

    case 7:
      DBG  (5, "Data protect: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;

    case 8:
      DBG  (5, "Blank check: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;

    case 9:
      DBG  (5, "Vendor defined: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;

    case 0xa:
      DBG  (5, "Copy aborted: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;

    case 0xb:
      if (asc == 0x00 && ascq == 0x00) {
        DBG  (5, "Aborted command: no sense/cancelled\n");
        return SANE_STATUS_CANCELLED;
      }
      if (asc == 0x45 && ascq == 0x00) {
        DBG  (5, "Aborted command: reselect failure\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (asc == 0x47 && ascq == 0x00) {
        DBG  (5, "Aborted command: SCSI parity error\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (asc == 0x48 && ascq == 0x00) {
        DBG  (5, "Aborted command: initiator error message\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (asc == 0x49 && ascq == 0x00) {
        DBG  (5, "Aborted command: invalid message\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (asc == 0x80 && ascq == 0x00) {
        DBG  (5, "Aborted command: timeout\n");
        return SANE_STATUS_IO_ERROR;
      }
      DBG  (5, "Aborted command: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;
      break;

    case 0xc:
      DBG  (5, "Equal: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;

    case 0xd:
      DBG  (5, "Volume overflow: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;

    case 0xe:
      if (asc == 0x3b && ascq == 0x0d) {
        DBG  (5, "Miscompare: too many docs\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (asc == 0x3b && ascq == 0x0e) {
        DBG  (5, "Miscompare: too few docs\n");
        return SANE_STATUS_IO_ERROR;
      }
      DBG  (5, "Miscompare: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;

    default:
      DBG (5, "Unknown Sense Code\n");
      return SANE_STATUS_IO_ERROR;
  }

  DBG (5, "sense_handler: should never happen!\n");

  return SANE_STATUS_IO_ERROR;
}

/*
 * take a bunch of pointers, send commands to scanner
 */
static SANE_Status
do_cmd(struct scanner *s, int runRS, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t * inLen
)
{
    if (s->connection == CONNECTION_SCSI) {
        return do_scsi_cmd(s, runRS, shortTime,
                 cmdBuff, cmdLen,
                 outBuff, outLen,
                 inBuff, inLen
        );
    }
    if (s->connection == CONNECTION_USB) {
        return do_usb_cmd(s, runRS, shortTime,
                 cmdBuff, cmdLen,
                 outBuff, outLen,
                 inBuff, inLen
        );
    }
    return SANE_STATUS_INVAL;
}

static SANE_Status
do_scsi_cmd(struct scanner *s, int runRS, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t * inLen
)
{
  int ret;

  /*shut up compiler*/
  runRS=runRS;
  shortTime=shortTime;

  DBG(10, "do_scsi_cmd: start\n");

  DBG(25, "cmd: writing %d bytes\n", (int)cmdLen);
  hexdump(30, "cmd: >>", cmdBuff, cmdLen);

  if(outBuff && outLen){
    DBG(25, "out: writing %d bytes\n", (int)outLen);
    hexdump(30, "out: >>", outBuff, outLen);
  }
  if (inBuff && inLen){
    DBG(25, "in: reading %d bytes\n", (int)*inLen);
    memset(inBuff,0,*inLen);
  }

  ret = sanei_scsi_cmd2(s->fd, cmdBuff, cmdLen, outBuff, outLen, inBuff, inLen);

  if(ret != SANE_STATUS_GOOD && ret != SANE_STATUS_EOF){
    DBG(5,"do_scsi_cmd: return '%s'\n",sane_strstatus(ret));
    return ret;
  }

  if (inBuff && inLen){
    if(ret == SANE_STATUS_EOF){
      DBG(25, "in: short read, remainder %lu bytes\n", (u_long)s->rs_info);
      *inLen -= s->rs_info;
    }
    hexdump(31, "in: <<", inBuff, *inLen);
    DBG(25, "in: read %d bytes\n", (int)*inLen);
  }

  DBG(10, "do_scsi_cmd: finish\n");

  return ret;
}

static SANE_Status
do_usb_cmd(struct scanner *s, int runRS, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t * inLen
)
{
    size_t cmdOffset = 0;
    size_t cmdLength = 0;
    size_t cmdActual = 0;
    unsigned char * cmdBuffer = NULL;
    int cmdTimeout = 0;

    size_t outOffset = 0;
    size_t outLength = 0;
    size_t outActual = 0;
    unsigned char * outBuffer = NULL;
    int outTimeout = 0;

    size_t inOffset = 0;
    size_t inLength = 0;
    size_t inActual = 0;
    unsigned char * inBuffer = NULL;
    int inTimeout = 0;

    size_t extraLength = 0;

    int ret = 0;
    int ret2 = 0;

    struct timeval timer;
    gettimeofday(&timer,NULL);

    DBG (10, "do_usb_cmd: start %lu %lu\n", (long unsigned int)timer.tv_sec, (long unsigned int)timer.tv_usec);

    /****************************************************************/
    /* the command stage */
    {
      cmdOffset = USB_HEADER_LEN;
      cmdLength = cmdOffset+USB_COMMAND_LEN;
      cmdActual = cmdLength;
      cmdTimeout = USB_COMMAND_TIME;

      /* change timeout */
      if(shortTime)
        cmdTimeout/=60;
      sanei_usb_set_timeout(cmdTimeout);

      /* build buffer */
      cmdBuffer = calloc(cmdLength,1);
      if(!cmdBuffer){
        DBG(5,"cmd: no mem\n");
        return SANE_STATUS_NO_MEM;
      }
  
      /* build a USB packet around the SCSI command */
      cmdBuffer[3] = cmdLength-4;
      cmdBuffer[5] = 1;
      cmdBuffer[6] = 0x90;
      memcpy(cmdBuffer+cmdOffset,cmdBuff,cmdLen);
  
      /* write the command out */
      DBG(25, "cmd: writing %d bytes, timeout %d\n", (int)cmdLength, cmdTimeout);
      hexdump(30, "cmd: >>", cmdBuffer, cmdLength);
      ret = sanei_usb_write_bulk(s->fd, cmdBuffer, &cmdActual);
      DBG(25, "cmd: wrote %d bytes, retVal %d\n", (int)cmdActual, ret);
  
      if(cmdLength != cmdActual){
        DBG(5,"cmd: wrong size %d/%d\n", (int)cmdLength, (int)cmdActual);
        free(cmdBuffer);
        return SANE_STATUS_IO_ERROR;
      }
      if(ret != SANE_STATUS_GOOD){
        DBG(5,"cmd: write error '%s'\n",sane_strstatus(ret));
        free(cmdBuffer);
        return ret;
      }
      free(cmdBuffer);
    }

    /****************************************************************/
    /* the extra status stage, used by few scanners                 */
    /* this is like the regular status block, with an additional    */
    /* length component at the end */
    if(s->extra_status){
      ret2 = do_usb_status(s,runRS,shortTime,&extraLength);

      /* bail out on bad RS status */
      if(ret2){
        DBG(5,"extra: bad RS status, %d\n", ret2);
        return ret2;
      }
    }

    /****************************************************************/
    /* the output stage */
    if(outBuff && outLen){

      outOffset = USB_HEADER_LEN;
      outLength = outOffset+outLen;
      outActual = outLength;
      outTimeout = USB_DATA_TIME;

      /* change timeout */
      if(shortTime)
        outTimeout/=60;
      sanei_usb_set_timeout(outTimeout);

      /* build outBuffer */
      outBuffer = calloc(outLength,1);
      if(!outBuffer){
        DBG(5,"out: no mem\n");
        return SANE_STATUS_NO_MEM;
      }
  
      /* build a USB packet around the SCSI command */
      outBuffer[3] = outLength-4;
      outBuffer[5] = 2;
      outBuffer[6] = 0xb0;
      memcpy(outBuffer+outOffset,outBuff,outLen);
  
      /* write the command out */
      DBG(25, "out: writing %d bytes, timeout %d\n", (int)outLength, outTimeout);
      hexdump(30, "out: >>", outBuffer, outLength);
      ret = sanei_usb_write_bulk(s->fd, outBuffer, &outActual);
      DBG(25, "out: wrote %d bytes, retVal %d\n", (int)outActual, ret);
  
      if(outLength != outActual){
        DBG(5,"out: wrong size %d/%d\n", (int)outLength, (int)outActual);
        free(outBuffer);
        return SANE_STATUS_IO_ERROR;
      }
      if(ret != SANE_STATUS_GOOD){
        DBG(5,"out: write error '%s'\n",sane_strstatus(ret));
        free(outBuffer);
        return ret;
      }
      free(outBuffer);
    }

    /****************************************************************/
    /* the input stage */
    if(inBuff && inLen){

      inOffset = 0;
      if(s->padded_read)
        inOffset = USB_HEADER_LEN;

      inLength = inOffset+*inLen;
      inActual = inLength;

      /* use the extra length to alter the amount of in we request */
      if(s->extra_status && extraLength && *inLen > extraLength){
        DBG(5,"in: adjust extra, %d %d\n", (int)*inLen, (int)extraLength);
        inActual = inOffset+extraLength;
      }

      /*blast caller's copy in case we error out*/
      *inLen = 0;

      inTimeout = USB_DATA_TIME;

      /* change timeout */
      if(shortTime)
        inTimeout/=60;
      sanei_usb_set_timeout(inTimeout);

      /* build inBuffer */
      inBuffer = calloc(inActual,1);
      if(!inBuffer){
        DBG(5,"in: no mem\n");
        return SANE_STATUS_NO_MEM;
      }
  
      DBG(25, "in: reading %d bytes, timeout %d\n", (int)inActual, inTimeout);
      ret = sanei_usb_read_bulk(s->fd, inBuffer, &inActual);
      DBG(25, "in: read %d bytes, retval %d\n", (int)inActual, ret);
      hexdump(31, "in: <<", inBuffer, inActual);

      if(!inActual){
        DBG(5,"in: got no data, clearing\n");
        free(inBuffer);
	return do_usb_clear(s,1,runRS);
      }
      if(inActual < inOffset){
        DBG(5,"in: read shorter than inOffset\n");
        free(inBuffer);
        return SANE_STATUS_IO_ERROR;
      }
      if(ret != SANE_STATUS_GOOD){
        DBG(5,"in: return error '%s'\n",sane_strstatus(ret));
        free(inBuffer);
        return ret;
      }

      /* note that inBuffer is not copied and freed here...*/
    }

    /****************************************************************/
    /* the normal status stage */
    ret2 = do_usb_status(s,runRS,shortTime,&extraLength);

    /* if status said EOF, adjust input with remainder count */
    if(ret2 == SANE_STATUS_EOF && inBuffer){

      /* EOF is ok */
      ret2 = SANE_STATUS_GOOD;

      if(inActual < inLength - s->rs_info){
        DBG(5,"in: we read < RS, ignoring RS: %d < %d (%d-%d)\n",
          (int)inActual,(int)(inLength-s->rs_info),(int)inLength,(int)s->rs_info);
      }
      else if(inActual > inLength - s->rs_info){
        DBG(5,"in: we read > RS, using RS: %d to %d (%d-%d)\n",
          (int)inActual,(int)(inLength-s->rs_info),(int)inLength,(int)s->rs_info);
        inActual = inLength - s->rs_info;
      }
    }

    /* bail out on bad RS status */
    if(ret2){
      if(inBuffer) free(inBuffer);
      DBG(5,"stat: bad RS status, %d\n", ret2);
      return ret2;
    }

    /* now that we have read status, deal with input buffer */
    if(inBuffer){
      if(inLength != inActual){
        ret = SANE_STATUS_EOF;
        DBG(5,"in: short read, %d/%d\n", (int)inLength,(int)inActual);
      }
  
      /* ignore the USB packet around the SCSI command */
      *inLen = inActual - inOffset;
      memcpy(inBuff,inBuffer+inOffset,*inLen);
  
      free(inBuffer);
    }

    gettimeofday(&timer,NULL);

    DBG (10, "do_usb_cmd: finish %lu %lu\n", (long unsigned int)timer.tv_sec, (long unsigned int)timer.tv_usec);

    return ret;
}

static SANE_Status
do_usb_status(struct scanner *s, int runRS, int shortTime, size_t * extraLength)
{

#define EXTRA_READ_len 4

    size_t statPadding = 0;
    size_t statOffset = 0;
    size_t statLength = 0;
    size_t statActual = 0;
    unsigned char * statBuffer = NULL;
    int statTimeout = 0;

    int ret = 0;

    if(s->padded_read)
      statPadding = USB_HEADER_LEN;

    statLength = statPadding+USB_STATUS_LEN;
    statOffset = statLength-1;

    if(s->extra_status)
      statLength += EXTRA_READ_len;

    statActual = statLength;
    statTimeout = USB_STATUS_TIME;

    /* change timeout */
    if(shortTime)
      statTimeout/=60;
    sanei_usb_set_timeout(statTimeout);

    /* build statBuffer */
    statBuffer = calloc(statLength,1);
    if(!statBuffer){
      DBG(5,"stat: no mem\n");
      return SANE_STATUS_NO_MEM;
    }
  
    DBG(25, "stat: reading %d bytes, timeout %d\n", (int)statLength, statTimeout);
    ret = sanei_usb_read_bulk(s->fd, statBuffer, &statActual);
    DBG(25, "stat: read %d bytes, retval %d\n", (int)statActual, ret);
    hexdump(30, "stat: <<", statBuffer, statActual);
  
    /*weird status*/
    if(ret != SANE_STATUS_GOOD){
      DBG(5,"stat: clearing error '%s'\n",sane_strstatus(ret));
      ret = do_usb_clear(s,1,runRS);
    }
    /*short read*/
    else if(statLength != statActual){
      DBG(5,"stat: clearing short %d/%d\n",(int)statLength,(int)statActual);
      ret = do_usb_clear(s,1,runRS);
    }
    /*inspect the status byte of the response*/
    else if(statBuffer[statOffset]){
      DBG(5,"stat: status %d\n",statBuffer[statLength-1-4]);
      ret = do_usb_clear(s,0,runRS);
    }

    /*extract the extra length byte of the response*/
    if(s->extra_status){
      *extraLength = get_ES_length(statBuffer);
      DBG(15,"stat: extra %d\n",(int)*extraLength);
    }

    free(statBuffer);

    return ret;
}

static SANE_Status
do_usb_clear(struct scanner *s, int clear, int runRS)
{
    SANE_Status ret, ret2;

    DBG (10, "do_usb_clear: start\n");

    usleep(100000);

    if(clear){
      DBG (15, "do_usb_clear: clear halt\n");
      ret = sanei_usb_clear_halt(s->fd);
      if(ret != SANE_STATUS_GOOD){
        DBG(5,"do_usb_clear: cant clear halt, returning %d\n", ret);
        return ret;
      }
    }

    /* caller is interested in having RS run on errors */
    if(runRS){

        unsigned char rs_cmd[REQUEST_SENSE_len];
        size_t rs_cmdLen = REQUEST_SENSE_len;

        unsigned char rs_in[RS_return_size];
        size_t rs_inLen = RS_return_size;

        memset(rs_cmd,0,rs_cmdLen);
        set_SCSI_opcode(rs_cmd, REQUEST_SENSE_code);
	set_RS_return_size(rs_cmd, rs_inLen);

        DBG(25,"rs sub call >>\n");
        ret2 = do_cmd(
          s,0,0,
          rs_cmd, rs_cmdLen,
          NULL,0,
          rs_in, &rs_inLen
        );
        DBG(25,"rs sub call <<\n");
  
        if(ret2 == SANE_STATUS_EOF){
          DBG(5,"rs: got EOF, returning IO_ERROR\n");
          return SANE_STATUS_IO_ERROR;
        }
        if(ret2 != SANE_STATUS_GOOD){
          DBG(5,"rs: return error '%s'\n",sane_strstatus(ret2));
          return ret2;
        }

        /* parse the rs data */
        ret2 = sense_handler( 0, rs_in, (void *)s );

        DBG (10, "do_usb_clear: finish after RS\n");
        return ret2;
    }

    DBG (10, "do_usb_clear: finish with io error\n");

    return SANE_STATUS_IO_ERROR;
}

static SANE_Status
wait_scanner(struct scanner *s) 
{
  SANE_Status ret = SANE_STATUS_GOOD;

  unsigned char cmd[TEST_UNIT_READY_len];
  size_t cmdLen = TEST_UNIT_READY_len;

  DBG (10, "wait_scanner: start\n");

  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd,TEST_UNIT_READY_code);

  ret = do_cmd (
    s, 0, 1,
    cmd, cmdLen,
    NULL, 0,
    NULL, NULL
  );
  
  if (ret != SANE_STATUS_GOOD) {
    DBG(5,"WARNING: Brain-dead scanner. Hitting with stick\n");
    ret = do_cmd (
      s, 0, 1,
      cmd, cmdLen,
      NULL, 0,
      NULL, NULL
    );
  }
  if (ret != SANE_STATUS_GOOD) {
    DBG(5,"WARNING: Brain-dead scanner. Hitting with stick again\n");
    ret = do_cmd (
      s, 0, 1,
      cmd, cmdLen,
      NULL, 0,
      NULL, NULL
    );
  }

  if (ret != SANE_STATUS_GOOD) {
    DBG (5, "wait_scanner: error '%s'\n", sane_strstatus (ret));
  }

  DBG (10, "wait_scanner: finish\n");

  return ret;
}

/* Some scanners have per-resolution
 * color interlacing values, but most
 * don't. This helper can tell the 
 * difference.
 */
static int
get_color_inter(struct scanner *s, int side, int res) 
{
  int i;
  for(i=0;i<DPI_1200;i++){
    if(res == dpi_list[i])
      break;
  }

  if(s->color_inter_by_res[i])
    return s->color_inter_by_res[i];

  return s->color_interlace[side];
}

/* s->u.page_x stores the user setting
 * for the paper width in adf. sometimes,
 * we need a value that differs from this
 * due to using FB or overscan.
 */
static int
get_page_width(struct scanner *s) 
{
  int width = s->u.page_x;

  /* scanner max for fb */
  if(s->u.source == SOURCE_FLATBED){
      return s->max_x_fb;
  }

  /* cant overscan larger than scanner max */
  if(width > s->valid_x){
      return s->valid_x;
  }

  /* overscan adds a margin to both sides */
  return width;
}

/* s->u.page_y stores the user setting
 * for the paper height in adf. sometimes,
 * we need a value that differs from this
 * due to using FB or overscan.
 */
static int
get_page_height(struct scanner *s) 
{
  int height = s->u.page_y;

  /* scanner max for fb */
  if(s->u.source == SOURCE_FLATBED){
      return s->max_y_fb;
  }

  /* cant overscan larger than scanner max */
  if(height > s->max_y){
      return s->max_y;
  }

  /* overscan adds a margin to both sides */
  return height;
}


/**
 * Convenience method to determine longest string size in a list.
 */
static size_t
maxStringSize (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  int i;

  for (i = 0; strings[i]; ++i) {
    size = strlen (strings[i]) + 1;
    if (size > max_size)
      max_size = size;
  }

  return max_size;
}

/*
 * Prints a hex dump of the given buffer onto the debug output stream.
 */
static void
hexdump (int level, char *comment, unsigned char *p, int l)
{
  int i;
  char line[70]; /* 'xxx: xx xx ... xx xx abc */
  char *hex = line+4;
  char *bin = line+53;

  if(DBG_LEVEL < level)
    return;

  line[0] = 0;

  DBG (level, "%s\n", comment);

  for (i = 0; i < l; i++, p++) {

    /* at start of line */
    if ((i % 16) == 0) {

      /* not at start of first line, print current, reset */
      if (i) {
        DBG (level, "%s\n", line);
      }

      memset(line,0x20,69);
      line[69] = 0;
      hex = line + 4;
      bin = line + 53;

      sprintf (line, "%3.3x:", i);
    }

    /* the hex section */
    sprintf (hex, " %2.2x", *p);
    hex += 3;
    *hex = ' ';

    /* the char section */
    if(*p >= 0x20 && *p <= 0x7e){
      *bin=*p;
    }
    else{
      *bin='.';
    }
    bin++;
  }

  /* print last (partial) line */
  if (i)
    DBG (level, "%s\n", line);
}

/**
 * An advanced method we don't support but have to define.
 */
SANE_Status
sane_set_io_mode (SANE_Handle h, SANE_Bool non_blocking)
{
  DBG (10, "sane_set_io_mode\n");
  DBG (15, "%d %p\n", non_blocking, h);
  return SANE_STATUS_UNSUPPORTED;
}

/**
 * An advanced method we don't support but have to define.
 */
SANE_Status
sane_get_select_fd (SANE_Handle h, SANE_Int *fdp)
{
  DBG (10, "sane_get_select_fd\n");
  DBG (15, "%p %d\n", h, *fdp);
  return SANE_STATUS_UNSUPPORTED;
}

/*
 * @@ Section 8 - Image processing functions
 */

/* Look in image for likely upper and left paper edges, then rotate
 * image so that upper left corner of paper is upper left of image.
 * FIXME: should we do this before we binarize instead of after? */
static SANE_Status
buffer_deskew(struct scanner *s, int side)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  int pwidth = s->i.width;
  int width = s->i.Bpl;
  int height = s->i.height;

  double TSlope = 0;
  int TXInter = 0;
  int TYInter = 0;
  double TSlopeHalf = 0;
  int TOffsetHalf = 0;

  double LSlope = 0;
  int LXInter = 0;
  int LYInter = 0;
  double LSlopeHalf = 0;
  int LOffsetHalf = 0;

  int rotateX = 0;
  int rotateY = 0;

  int * topBuf = NULL, * botBuf = NULL;

  DBG (10, "buffer_deskew: start\n");

  /* get buffers for edge detection */
  topBuf = getTransitionsY(s,side,1);
  if(!topBuf){
    DBG (5, "buffer_deskew: cant gTY\n");
    ret = SANE_STATUS_NO_MEM;
    goto cleanup;
  }

  if(0){
    int i;
    for(i=0;i<width;i++){
      if(topBuf[i] >=0 && topBuf[i] < height)
        s->buffers[side][topBuf[i]*width+i] = 0;
    }
  }

  botBuf = getTransitionsY(s,side,0);
  if(!botBuf){
    DBG (5, "buffer_deskew: cant gTY\n");
    ret = SANE_STATUS_NO_MEM;
    goto cleanup;
  }

  /* find best top line */
  ret = getEdgeIterate (pwidth, height, s->i.dpi_y, topBuf,
    &TSlope, &TXInter, &TYInter);
  if(ret){
    DBG(5,"buffer_deskew: gEI error: %d",ret);
    goto cleanup;
  }
  DBG(15,"top: %04.04f %d %d\n",TSlope,TXInter,TYInter);

  /* slope is too shallow, don't want to divide by 0 */
  if(fabs(TSlope) < 0.0001){
    DBG(15,"buffer_deskew: slope too shallow: %0.08f\n",TSlope);
    goto cleanup;
  }

  /* find best left line, perpendicular to top line */
  LSlope = (double)-1/TSlope;
  ret = getEdgeSlope (pwidth, height, topBuf, botBuf, LSlope,
    &LXInter, &LYInter);
  if(ret){
    DBG(5,"buffer_deskew: gES error: %d",ret);
    goto cleanup;
  }
  DBG(15,"buffer_deskew: left: %04.04f %d %d\n",LSlope,LXInter,LYInter);

  /* find point about which to rotate */
  TSlopeHalf = tan(atan(TSlope)/2);
  TOffsetHalf = LYInter;
  DBG(15,"buffer_deskew: top half: %04.04f %d\n",TSlopeHalf,TOffsetHalf);

  LSlopeHalf = tan((atan(LSlope) + ((LSlope < 0)?-M_PI_2:M_PI_2))/2);
  LOffsetHalf = - LSlopeHalf * TXInter;
  DBG(15,"buffer_deskew: left half: %04.04f %d\n",LSlopeHalf,LOffsetHalf);

  rotateX = (LOffsetHalf-TOffsetHalf) / (TSlopeHalf-LSlopeHalf);
  rotateY = TSlopeHalf * rotateX + TOffsetHalf;
  DBG(15,"buffer_deskew: rotate: %d %d\n",rotateX,rotateY);

  ret = rotateOnCenter (s, side, rotateX, rotateY, TSlope);
  if(ret){
    DBG(5,"buffer_deskew: gES error: %d",ret);
    goto cleanup;
  }

  cleanup:
  if(topBuf)
    free(topBuf);
  if(botBuf)
    free(botBuf);

  DBG (10, "buffer_deskew: finish\n");
  return ret;
}

/* Look in image for likely left/right/bottom paper edges, then crop
 * image to match. Does not attempt to rotate the image.
 * FIXME: should we do this before we binarize instead of after? */
static SANE_Status
buffer_crop(struct scanner *s, int side)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  int bwidth = s->i.Bpl;
  int width = s->i.width;
  int height = s->i.height;

  int top = 0;
  int bot = 0;
  int left = width;
  int right = 0;

  int * topBuf = NULL, * botBuf = NULL;
  int * leftBuf = NULL, * rightBuf = NULL;
  int leftCount = 0, rightCount = 0, botCount = 0;
  int i;

  DBG (10, "buffer_crop: start\n");

  /* get buffers to find sides and bottom */
  topBuf = getTransitionsY(s,side,1);
  if(!topBuf){
    DBG (5, "buffer_crop: no topBuf\n");
    ret = SANE_STATUS_NO_MEM;
    goto cleanup;
  }

  botBuf = getTransitionsY(s,side,0);
  if(!botBuf){
    DBG (5, "buffer_crop: no botBuf\n");
    ret = SANE_STATUS_NO_MEM;
    goto cleanup;
  }

  leftBuf = getTransitionsX(s,side,1);
  if(!leftBuf){
    DBG (5, "buffer_crop: no leftBuf\n");
    ret = SANE_STATUS_NO_MEM;
    goto cleanup;
  }

  rightBuf = getTransitionsX(s,side,0);
  if(!rightBuf){
    DBG (5, "buffer_crop: no rightBuf\n");
    ret = SANE_STATUS_NO_MEM;
    goto cleanup;
  }

  /* loop thru top and bottom lists, look for l and r extremes */
  for(i=0; i<width; i++){
    if(botBuf[i] > topBuf[i]){
      if(left > i){
        left = i;
      }

      leftCount++;
      if(leftCount > 3){
        break;
      }
    }
    else{
      leftCount = 0;
      left = width;
    }
  }

  for(i=width-1; i>=0; i--){
    if(botBuf[i] > topBuf[i]){
      if(right < i){
        right = i;
      }

      rightCount++;
      if(rightCount > 3){
        break;
      }
    }
    else{
      rightCount = 0;
      right = -1;
    }
  }

  /* loop thru left and right lists, look for bottom extreme */
  for(i=height-1; i>=0; i--){
    if(rightBuf[i] > leftBuf[i]){
      if(bot < i){
        bot = i;
      }

      botCount++;
      if(botCount > 3){
        break;
      }
    }
    else{
      botCount = 0;
      bot = -1;
    }
  }

  DBG (15, "buffer_crop: t:%d b:%d l:%d r:%d\n",top,bot,left,right);

  /* now crop the image */
  /*FIXME: crop duplex backside at same time?*/
  if(left < right && top < bot){

    int pixels = 0;
    int bytes = 0;
    unsigned char * line = NULL;

    /*convert left and right to bytes, figure new byte and pixel width */
    switch (s->i.mode) {

      case MODE_COLOR:
        pixels = right-left;
        bytes = pixels * 3;
        left *= 3;
        right *= 3;
        break;

      case MODE_GRAYSCALE:
        pixels = right-left;
        bytes = right-left;
        break;

      case MODE_LINEART:
      case MODE_HALFTONE:
        left /= 8;
        right = (right+7)/8;
        bytes = right-left;
        pixels = bytes * 8;
        break;
    }

    DBG (15, "buffer_crop: l:%d r:%d p:%d b:%d\n",left,right,pixels,bytes);

    line = malloc(bytes);
    if(!line){
      DBG (5, "buffer_crop: no line\n");
      ret = SANE_STATUS_NO_MEM;
      goto cleanup;
    }

    s->i.bytes_sent[side] = 0;

    for(i=top; i<bot; i++){
      memcpy(line, s->buffers[side] + i*bwidth + left, bytes);
      memcpy(s->buffers[side] + s->i.bytes_sent[side], line, bytes);
      s->i.bytes_sent[side] += bytes;
    }

    s->i.bytes_tot[side] = s->i.bytes_sent[side];
    s->i.width = pixels;
    s->i.height = bot-top;
    s->i.Bpl = bytes;

    free(line);
  }

  cleanup:
  if(topBuf)
    free(topBuf);
  if(botBuf)
    free(botBuf);
  if(leftBuf)
    free(leftBuf);
  if(rightBuf)
    free(rightBuf);

  DBG (10, "buffer_crop: finish\n");
  return ret;
}

/* Look in image for disconnected 'spots' of the requested size.
 * Replace the spots with the average color of the surrounding pixels.
 * FIXME: should we do this before we binarize instead of after? */
static SANE_Status
buffer_despeck(struct scanner *s, int side)
{
  SANE_Status ret = SANE_STATUS_GOOD;
  int i,j,k,l,n;
  int w = s->i.Bpl;
  int pw = s->i.width;
  int h = s->i.height;
  int t = w*h;
  int d = s->swdespeck;

  DBG (10, "buffer_despeck: start\n");

  switch (s->i.mode){

    case MODE_COLOR:
      for(i=w; i<t-w-(w*d); i+=w){
        for(j=1; j<pw-1-d; j++){

          int thresh = 255*3;
          int outer[] = {0,0,0};
          int hits = 0;

          /*loop over rows and columns in window */
          for(k=0; k<d; k++){
            for(l=0; l<d; l++){
              int tmp = 0;

              for(n=0; n<3; n++){
                tmp += s->buffers[side][i + j*3 + k*w + l*3 + n];
              }

              if(tmp < thresh)
                thresh = tmp;
            }
          }

          thresh = (thresh + 255*3 + 255*3)/3;
    
          /*loop over rows and columns around window */
          for(k=-1; k<d+1; k++){
            for(l=-1; l<d+1; l++){

              int tmp[3];
  
              /* dont count pixels in the window */
              if(k != -1 && k != d && l != -1 && l != d)
                continue;
  
              for(n=0; n<3; n++){
                tmp[n] = s->buffers[side][i + j*3 + k*w + l*3 + n];
                outer[n] += tmp[n];
              }
              if(tmp[0]+tmp[1]+tmp[2] < thresh){
                hits++;
                break;
              }
            }
          }

          for(n=0; n<3; n++){
            outer[n] /= (4*d + 4);
          }

          /*no hits, overwrite with avg surrounding color*/
          if(!hits){
            for(k=0; k<d; k++){
              for(l=0; l<d; l++){
                for(n=0; n<3; n++){
                  s->buffers[side][i + j*3 + k*w + l*3 + n] = outer[n];
                }
              }
            }
          }

        }
      }
      break;

    case MODE_GRAYSCALE:
      for(i=w; i<t-w-(w*d); i+=w){
        for(j=1; j<w-1-d; j++){

          int thresh = 255;
          int outer = 0;
          int hits = 0;

          for(k=0; k<d; k++){
            for(l=0; l<d; l++){
              if(s->buffers[side][i + j + k*w + l] < thresh)
                thresh = s->buffers[side][i + j + k*w + l];
            }
          }

          thresh = (thresh + 255 + 255)/3;
    
          /*loop over rows and columns around window */
          for(k=-1; k<d+1; k++){
            for(l=-1; l<d+1; l++){

              int tmp = 0;

              /* dont count pixels in the window */
              if(k != -1 && k != d && l != -1 && l != d)
                continue;
  
              tmp = s->buffers[side][i + j + k*w + l];

              if(tmp < thresh){
                hits++;
                break;
              }

              outer += tmp;
            }
          }

          outer /= (4*d + 4);

          /*no hits, overwrite with avg surrounding color*/
          if(!hits){
            for(k=0; k<d; k++){
              for(l=0; l<d; l++){
                s->buffers[side][i + j + k*w + l] = outer;
              }
            }
          }

        }
      }
      break;

    case MODE_LINEART:
    case MODE_HALFTONE:
      for(i=w; i<t-w-(w*d); i+=w){
        for(j=1; j<pw-1-d; j++){
          
          int curr = 0;
          int hits = 0;

          for(k=0; k<d; k++){
            for(l=0; l<d; l++){
              curr += s->buffers[side][i + k*w + (j+l)/8] >> (7-(j+l)%8) & 1;
            }
          }

          if(!curr)
            continue;

          /*loop over rows and columns around window */
          for(k=-1; k<d+1; k++){
            for(l=-1; l<d+1; l++){

              /* dont count pixels in the window */
              if(k != -1 && k != d && l != -1 && l != d)
                continue;
  
              hits += s->buffers[side][i + k*w + (j+l)/8] >> (7-(j+l)%8) & 1;

              if(hits)
                break;
            }
          }

          /*no hits, overwrite with white*/
          if(!hits){
            for(k=0; k<d; k++){
              for(l=0; l<d; l++){
                s->buffers[side][i + k*w + (j+l)/8] &= ~(1 << (7-(j+l)%8));
              }
            }
          }

        }
      }
      break;

    default:
      break;
  }

  DBG (10, "buffer_despeck: finish\n");
  return ret;
}

/* Loop thru the image width and look for first color change in each column.
 * Return a malloc'd array. Caller is responsible for freeing. */
int * 
getTransitionsY (struct scanner *s, int side, int top)
{
  int * buff;

  int i, j, k;
  int near, far;
  int winLen = 9;

  int width = s->i.width;
  int height = s->i.height;
  int depth = 1;

  /* defaults for bottom-up */
  int firstLine = height-1;
  int lastLine = -1;
  int direction = -1;

  DBG (10, "getTransitionsY: start\n");

  buff = calloc(width,sizeof(int));
  if(!buff){
    DBG (5, "getTransitionsY: no buff\n");
    return NULL;
  }

  /* override for top-down */
  if(top){
    firstLine = 0;
    lastLine = height;
    direction = 1;
  }

  /* load the buff array with y value for first color change from edge
   * gray/color uses a different algo from binary/halftone */
  switch (s->i.mode) {

    case MODE_COLOR:
      depth = 3;

    case MODE_GRAYSCALE:

      for(i=0; i<width; i++){
        buff[i] = lastLine;
  
        /* load the near and far windows with repeated copy of first pixel */
        near = 0;
        for(k=0; k<depth; k++){
          near += s->buffers[side][(firstLine*width+i) * depth + k];
        }
        near *= winLen;
        far = near;
  
        /* move windows, check delta */
        for(j=firstLine+direction; j!=lastLine; j+=direction){
  
          int farLine = j-winLen*2*direction;
          int nearLine = j-winLen*direction;

          if(farLine < 0 || farLine >= height){
            farLine = firstLine;
          }
          if(nearLine < 0 || nearLine >= height){
            nearLine = firstLine;
          }

          for(k=0; k<depth; k++){
            far -= s->buffers[side][(farLine*width+i)*depth+k];
            far += s->buffers[side][(nearLine*width+i)*depth+k];

            near -= s->buffers[side][(nearLine*width+i)*depth+k];
            near += s->buffers[side][(j*width+i)*depth+k];
          }
  
          if(abs(near - far) > winLen*depth*9){
            buff[i] = j;
            break;
          }
        }
      }
      break;

    case MODE_LINEART:
    case MODE_HALFTONE:
      for(i=0; i<width; i++){
        buff[i] = lastLine;
  
        /* load the near window with first pixel */
        near = s->buffers[side][(firstLine*width+i)/8] >> (7-(i%8)) & 1;
  
        /* move */
        for(j=firstLine+direction; j!=lastLine; j+=direction){
          if((s->buffers[side][(j*width+i)/8] >> (7-(i%8)) & 1) != near){
            buff[i] = j;
            break;
          }
        }
      }
      break;

  }

  /* blast any stragglers with no neighbors within .5 inch */
  for(i=0;i<width-7;i++){
    int sum = 0;
    for(j=1;j<=7;j++){
      if(abs(buff[i+j] - buff[i]) < s->i.dpi_y/2)
        sum++;
    }
    if(sum < 2)
      buff[i] = lastLine;
  }

  DBG (10, "getTransitionsY: finish\n");

  return buff;
}

/* Loop thru the image height and look for first color change in each row.
 * Return a malloc'd array. Caller is responsible for freeing. */
int * 
getTransitionsX (struct scanner *s, int side, int left)
{
  int * buff;

  int i, j, k;
  int near, far;
  int winLen = 9;

  int bwidth = s->i.Bpl;
  int width = s->i.width;
  int height = s->i.height;
  int depth = 1;

  /* defaults for right-first */
  int firstCol = width-1;
  int lastCol = -1;
  int direction = -1;

  DBG (10, "getTransitionsX: start\n");

  buff = calloc(height,sizeof(int));
  if(!buff){
    DBG (5, "getTransitionsY: no buff\n");
    return NULL;
  }

  /* override for left-first*/
  if(left){
    firstCol = 0;
    lastCol = width;
    direction = 1;
  }

  /* load the buff array with x value for first color change from edge
   * gray/color uses a different algo from binary/halftone */
  switch (s->i.mode) {

    case MODE_COLOR:
      depth = 3;

    case MODE_GRAYSCALE:

      for(i=0; i<height; i++){
        buff[i] = lastCol;
  
        /* load the near and far windows with repeated copy of first pixel */
        near = 0;
        for(k=0; k<depth; k++){
          near += s->buffers[side][i*bwidth + k];
        }
        near *= winLen;
        far = near;
  
        /* move windows, check delta */
        for(j=firstCol+direction; j!=lastCol; j+=direction){
  
          int farCol = j-winLen*2*direction;
          int nearCol = j-winLen*direction;

          if(farCol < 0 || farCol >= width){
            farCol = firstCol;
          }
          if(nearCol < 0 || nearCol >= width){
            nearCol = firstCol;
          }

          for(k=0; k<depth; k++){
            far -= s->buffers[side][i*bwidth + farCol*depth + k];
            far += s->buffers[side][i*bwidth + nearCol*depth + k];

            near -= s->buffers[side][i*bwidth + nearCol*depth + k];
            near += s->buffers[side][i*bwidth + j*depth + k];
          }
  
          if(abs(near - far) > winLen*depth*9){
            buff[i] = j;
            break;
          }
        }
      }
      break;

    case MODE_LINEART:
    case MODE_HALFTONE:
      for(i=0; i<height; i++){
        buff[i] = lastCol;
  
        /* load the near window with first pixel */
        near = s->buffers[side][i*bwidth + firstCol/8] >> (7-(firstCol%8)) & 1;
  
        /* move */
        for(j=firstCol+direction; j!=lastCol; j+=direction){
          if((s->buffers[side][i*bwidth + j/8] >> (7-(j%8)) & 1) != near){
            buff[i] = j;
            break;
          }
        }
      }
      break;

  }

  /* blast any stragglers with no neighbors within .5 inch */
  for(i=0;i<height-7;i++){
    int sum = 0;
    for(j=1;j<=7;j++){
      if(abs(buff[i+j] - buff[i]) < s->i.dpi_x/2)
        sum++;
    }
    if(sum < 2)
      buff[i] = lastCol;
  }

  DBG (10, "getTransitionsX: finish\n");

  return buff;
}

/* Loop thru a getTransitions array, and use a simplified Hough transform
 * to divide likely edges into a 2-d array of bins. Then weight each
 * bin based on its angle and offset. Return the 'best' bin. */
static SANE_Status
getLine (int height, int width, int * buff,
 int slopes, double minSlope, double maxSlope,
 int offsets, int minOffset, int maxOffset,
 double * finSlope, int * finOffset, int * finDensity)
{
  SANE_Status ret = 0;

  int ** lines = NULL;
  int i, j;
  int rise, run;
  double slope;
  int offset;
  int sIndex, oIndex;
  int hWidth = width/2;

  double * slopeCenter = NULL;
  int * slopeScale = NULL;
  double * offsetCenter = NULL;
  int * offsetScale = NULL;

  int maxDensity = 1;
  double absMaxSlope = fabs(maxSlope);
  double absMinSlope = fabs(minSlope);
  int absMaxOffset = abs(maxOffset);
  int absMinOffset = abs(minOffset);

  DBG(10,"getLine: start %+0.4f %+0.4f %d %d\n",
    minSlope,maxSlope,minOffset,maxOffset);

  /*silence compiler*/
  height = height;

  if(absMaxSlope < absMinSlope)
    absMaxSlope = absMinSlope;

  if(absMaxOffset < absMinOffset)
    absMaxOffset = absMinOffset;

  /* build an array of pretty-print values for slope */
  slopeCenter = calloc(slopes,sizeof(double));
  if(!slopeCenter){
    DBG(5,"getLine: cant load slopeCenter\n");
    ret = SANE_STATUS_NO_MEM;
    goto cleanup;
  }

  /* build an array of scaling factors for slope */
  slopeScale = calloc(slopes,sizeof(int));
  if(!slopeScale){
    DBG(5,"getLine: cant load slopeScale\n");
    ret = SANE_STATUS_NO_MEM;
    goto cleanup;
  }

  for(j=0;j<slopes;j++){

    /* find central value of this 'bucket' */
    slopeCenter[j] = (
      (double)j*(maxSlope-minSlope)/slopes+minSlope 
      + (double)(j+1)*(maxSlope-minSlope)/slopes+minSlope
    )/2;

    /* scale value from the requested range into an inverted 100-1 range
     * input close to 0 makes output close to 100 */
    slopeScale[j] = 101 - fabs(slopeCenter[j])*100/absMaxSlope;
  }

  /* build an array of pretty-print values for offset */
  offsetCenter = calloc(offsets,sizeof(double));
  if(!offsetCenter){
    DBG(5,"getLine: cant load offsetCenter\n");
    ret = SANE_STATUS_NO_MEM;
    goto cleanup;
  }

  /* build an array of scaling factors for offset */
  offsetScale = calloc(offsets,sizeof(int));
  if(!offsetScale){
    DBG(5,"getLine: cant load offsetScale\n");
    ret = SANE_STATUS_NO_MEM;
    goto cleanup;
  }

  for(j=0;j<offsets;j++){

    /* find central value of this 'bucket'*/
    offsetCenter[j] = (
      (double)j/offsets*(maxOffset-minOffset)+minOffset
      + (double)(j+1)/offsets*(maxOffset-minOffset)+minOffset
    )/2;

    /* scale value from the requested range into an inverted 100-1 range
     * input close to 0 makes output close to 100 */
    offsetScale[j] = 101 - fabs(offsetCenter[j])*100/absMaxOffset;
  }

  /* build 2-d array of 'density', divided into slope and offset ranges */
  lines = calloc(slopes, sizeof(int *));
  if(!lines){
    DBG(5,"getLine: cant load lines\n");
    ret = SANE_STATUS_NO_MEM;
    goto cleanup;
  }

  for(i=0;i<slopes;i++){
    if(!(lines[i] = calloc(offsets, sizeof(int)))){
      DBG(5,"getLine: cant load lines %d\n",i);
      ret = SANE_STATUS_NO_MEM;
      goto cleanup;
    }
  }

  for(i=0;i<width;i++){
    for(j=i+1;j<width && j<i+width/3;j++){

      /*FIXME: check for invalid (min/max) values?*/
      rise = buff[j] - buff[i];
      run = j-i;

      slope = (double)rise/run;
      if(slope >= maxSlope || slope < minSlope)
        continue;

      /* offset in center of width, not y intercept! */
      offset = slope * hWidth + buff[i] - slope * i;
      if(offset >= maxOffset || offset < minOffset)
        continue;

      sIndex = (slope - minSlope) * slopes/(maxSlope-minSlope);
      if(sIndex >= slopes)
        continue;

      oIndex = (offset - minOffset) * offsets/(maxOffset-minOffset);
      if(oIndex >= offsets)
        continue;

      lines[sIndex][oIndex]++;
    }
  }

  /* go thru array, and find most dense line (highest number) */
  for(i=0;i<slopes;i++){
    for(j=0;j<offsets;j++){
      if(lines[i][j] > maxDensity)
        maxDensity = lines[i][j];
    }
  }
  
  DBG(15,"getLine: maxDensity %d\n",maxDensity);

  *finSlope = 0;
  *finOffset = 0;
  *finDensity = 0;

  /* go thru array, and scale densities to % of maximum, plus adjust for
   * prefered (smaller absolute value) slope and offset */
  for(i=0;i<slopes;i++){
    for(j=0;j<offsets;j++){
      lines[i][j] = lines[i][j] * slopeScale[i] * offsetScale[j] / maxDensity;
      if(lines[i][j] > *finDensity){
        *finDensity = lines[i][j];
        *finSlope = slopeCenter[i];
        *finOffset = offsetCenter[j];
      }
    }
  }
  
  if(0){
    DBG(15,"offsetCenter:       ");
    for(j=0;j<offsets;j++){
      DBG(15," %+04.0f",offsetCenter[j]);
    }
    DBG(15,"\n");
  
    DBG(15,"offsetScale:        ");
    for(j=0;j<offsets;j++){
      DBG(15," %04d",offsetScale[j]);
    }
    DBG(15,"\n");
  
    for(i=0;i<slopes;i++){
      DBG(15,"slope: %02d %+02.2f %03d:",i,slopeCenter[i],slopeScale[i]);
      for(j=0;j<offsets;j++){
        DBG(15,"% 5d",lines[i][j]/100);
      }
      DBG(15,"\n");
    }
  }

  /* dont forget to cleanup */
  cleanup:
  for(i=0;i<10;i++){
    if(lines[i])
      free(lines[i]);
  }
  if(lines)
    free(lines);
  if(slopeCenter)
    free(slopeCenter);
  if(slopeScale)
    free(slopeScale);
  if(offsetCenter)
    free(offsetCenter);
  if(offsetScale)
    free(offsetScale);

  DBG(10,"getLine: finish\n");

  return ret;
}

/* Repeatedly find the best range of slope and offset via Hough transform.
 * Shift the ranges thru 4 different positions to avoid splitting data
 * across multiple bins (false positive). Home-in on the most likely upper
 * line of the paper inside the image. Return the 'best' line. */
SANE_Status
getEdgeIterate (int width, int height, int resolution,
int * buff, double * finSlope, int * finXInter, int * finYInter)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  int slopes = 11;
  int offsets = 11;
  double maxSlope = 1;
  double minSlope = -1;
  int maxOffset = resolution/6;
  int minOffset = -resolution/6;

  double topSlope = 0;
  int topOffset = 0;
  int topDensity = 0;
  
  int i,j;
  int pass = 0;

  DBG(10,"getEdgeIterate: start\n");

  while(pass++ < 7){
    double sStep = (maxSlope-minSlope)/slopes;
    int oStep = (maxOffset-minOffset)/offsets;

    double slope = 0;
    int offset = 0;
    int density = 0;
    int go = 0;

    topSlope = 0;
    topOffset = 0;
    topDensity = 0;

    /* find lines 4 times with slightly moved params,
     * to bypass binning errors, highest density wins */
    for(i=0;i<2;i++){
      double sStep2 = sStep*i/2;
      for(j=0;j<2;j++){
        int oStep2 = oStep*j/2;
        ret = getLine(height,width,buff,slopes,minSlope+sStep2,maxSlope+sStep2,offsets,minOffset+oStep2,maxOffset+oStep2,&slope,&offset,&density);
        if(ret){
          DBG(5,"getEdgeIterate: getLine error %d\n",ret);
          return ret;
        }
        DBG(15,"getEdgeIterate: %d %d %+0.4f %d %d\n",i,j,slope,offset,density);

        if(density > topDensity){
          topSlope = slope;
          topOffset = offset;
          topDensity = density;
        }
      }
    }

    DBG(15,"getEdgeIterate: ok %+0.4f %d %d\n",topSlope,topOffset,topDensity);

    /* did not find anything promising on first pass,
     * give up instead of fixating on some small, pointless feature */
    if(pass == 1 && topDensity < width/5){
      DBG(5,"getEdgeIterate: density too small %d %d\n",topDensity,width);
      topOffset = 0;
      topSlope = 0;
      break;
    }

    /* if slope can zoom in some more, do so. */
    if(sStep >= 0.0001){
      minSlope = topSlope - sStep;
      maxSlope = topSlope + sStep;
      go = 1;
    }

    /* if offset can zoom in some more, do so. */
    if(oStep){
      minOffset = topOffset - oStep;
      maxOffset = topOffset + oStep;
      go = 1;
    }

    /* cannot zoom in more, bail out */
    if(!go){
      break;
    }

    DBG(15,"getEdgeIterate: zoom: %+0.4f %+0.4f %d %d\n",
      minSlope,maxSlope,minOffset,maxOffset);
  }

  /* topOffset is in the center of the image,
   * convert to x and y intercept */
  if(topSlope != 0){
    *finYInter = topOffset - topSlope * width/2;
    *finXInter = *finYInter / -topSlope;
    *finSlope = topSlope;
  }
  else{
    *finYInter = 0;
    *finXInter = 0;
    *finSlope = 0;
  }

  DBG(10,"getEdgeIterate: finish\n");

  return 0;
}

/* find the left side of paper by moving a line 
 * perpendicular to top slope across the image
 * the 'left-most' point on the paper is the
 * one with the smallest X intercept
 * return x and y intercepts */
SANE_Status 
getEdgeSlope (int width, int height, int * top, int * bot,
 double slope, int * finXInter, int * finYInter)
{

  int i;
  int topXInter, topYInter;
  int botXInter, botYInter;
  int leftCount;

  DBG(10,"getEdgeSlope: start\n");

  topXInter = width;
  topYInter = 0;
  leftCount = 0;

  for(i=0;i<width;i++){
    
    if(top[i] < height){
      int tyi = top[i] - (slope * i);
      int txi = tyi/-slope;

      if(topXInter > txi){
        topXInter = txi;
        topYInter = tyi;
      }

      leftCount++;
      if(leftCount > 5){
        break;
      }
    }
    else{
      topXInter = width;
      topYInter = 0;
      leftCount = 0;
    }
  }

  botXInter = width;
  botYInter = 0;
  leftCount = 0;

  for(i=0;i<width;i++){
    
    if(bot[i] > -1){

      int byi = bot[i] - (slope * i);
      int bxi = byi/-slope;

      if(botXInter > bxi){
        botXInter = bxi;
        botYInter = byi;
      }

      leftCount++;
      if(leftCount > 5){
        break;
      }
    }
    else{
      botXInter = width;
      botYInter = 0;
      leftCount = 0;
    }
  }

  if(botXInter < topXInter){
    *finXInter = botXInter;
    *finYInter = botYInter;
  }
  else{
    *finXInter = topXInter;
    *finYInter = topYInter;
  }

  DBG(10,"getEdgeSlope: finish\n");

  return 0;
}

/* function to do a simple rotation by a given slope, around
 * a given point. The point can be outside of image to get
 * proper edge alignment. Unused areas filled with bg color
 * FIXME: Do in-place rotation to save memory */
SANE_Status
rotateOnCenter (struct scanner *s, int side,
  int centerX, int centerY, double slope)
{
  double slopeRad = -atan(slope);
  double slopeSin = sin(slopeRad);
  double slopeCos = cos(slopeRad);

  int bwidth = s->i.Bpl;
  int pwidth = s->i.width;
  int height = s->i.height;
  int depth = 1;
  int bg_color = s->lut[s->bg_color];

  unsigned char * outbuf;
  int i, j, k;

  DBG(10,"rotateOnCenter: start: %d %d\n",centerX,centerY);

  outbuf = malloc(s->i.bytes_tot[side]);
  if(!outbuf){
    DBG(15,"rotateOnCenter: no outbuf\n");
    return SANE_STATUS_NO_MEM;
  }

  switch (s->i.mode){

    case MODE_COLOR:
      depth = 3;

    case MODE_GRAYSCALE:
      memset(outbuf,bg_color,s->i.bytes_tot[side]);

      for (i=0; i<height; i++) {
        int shiftY = centerY - i;
    
        for (j=0; j<pwidth; j++) {
          int shiftX = centerX - j;
          int sourceX, sourceY;
    
          sourceX = centerX - (int)(shiftX * slopeCos + shiftY * slopeSin);
          if (sourceX < 0 || sourceX >= pwidth)
            continue;
    
          sourceY = centerY + (int)(-shiftY * slopeCos + shiftX * slopeSin);
          if (sourceY < 0 || sourceY >= height)
            continue;
    
          for (k=0; k<depth; k++) {
            outbuf[i*bwidth+j*depth+k]
              = s->buffers[side][sourceY*bwidth+sourceX*depth+k];
          }
        }
      }
      break;

    case MODE_LINEART:
    case MODE_HALFTONE:
      memset(outbuf,(bg_color<s->threshold)?0xff:0x00,s->i.bytes_tot[side]);

      for (i=0; i<height; i++) {
        int shiftY = centerY - i;
    
        for (j=0; j<pwidth; j++) {
          int shiftX = centerX - j;
          int sourceX, sourceY;
    
          sourceX = centerX - (int)(shiftX * slopeCos + shiftY * slopeSin);
          if (sourceX < 0 || sourceX >= pwidth)
            continue;
    
          sourceY = centerY + (int)(-shiftY * slopeCos + shiftX * slopeSin);
          if (sourceY < 0 || sourceY >= height)
            continue;

          /* wipe out old bit */
          outbuf[i*bwidth + j/8] &= ~(1 << (7-(j%8)));

          /* fill in new bit */
          outbuf[i*bwidth + j/8] |= 
            ((s->buffers[side][sourceY*bwidth + sourceX/8]
            >> (7-(sourceX%8))) & 1) << (7-(j%8));
        }
      }
      break;
  }

  memcpy(s->buffers[side],outbuf,s->i.bytes_tot[side]);

  free(outbuf);

  DBG(10,"rotateOnCenter: finish\n");

  return 0;
}

/* Function to build a lookup table (LUT), often
   used by scanners to implement brightness/contrast/gamma
   or by backends to speed binarization/thresholding

   offset and slope inputs are -127 to +127 

   slope rotates line around central input/output val,
   0 makes horizontal line

       pos           zero          neg
       .       x     .             .  x
       .      x      .             .   x
   out .     x       .xxxxxxxxxxx  .    x
       .    x        .             .     x
       ....x.......  ............  .......x....
            in            in            in

   offset moves line vertically, and clamps to output range
   0 keeps the line crossing the center of the table

       pos            zero          neg 
       .   xxxxxxxx   .        xx   .
       . x            .      x      . 
   out x              .    x        .          x
       .              .  x          .        x
       ............   xx..........  xxxxxxxx....
            in             in

   out_min/max provide bounds on output values,
   useful when building thresholding lut.
   0 and 255 are good defaults otherwise.
  */
static SANE_Status
load_lut (unsigned char * lut,
  int in_bits, int out_bits,
  int out_min, int out_max,
  int slope, int offset)
{
  SANE_Status ret = SANE_STATUS_GOOD;
  int i, j;
  double shift, rise;
  int max_in_val = (1 << in_bits) - 1;
  int max_out_val = (1 << out_bits) - 1;
  unsigned char * lut_p = lut;

  DBG (10, "load_lut: start %d %d\n", slope, offset);

  /* slope is converted to rise per unit run:
   * first [-127,127] to [-.999,.999]
   * then to [-PI/4,PI/4] then [0,PI/2]
   * then take the tangent (T.O.A)
   * then multiply by the normal linear slope 
   * because the table may not be square, i.e. 1024x256*/
  rise = tan((double)slope/128 * M_PI_4 + M_PI_4) * max_out_val / max_in_val;

  /* line must stay vertically centered, so figure
   * out vertical offset at central input value */
  shift = (double)max_out_val/2 - (rise*max_in_val/2);

  /* convert the user offset setting to scale of output
   * first [-127,127] to [-1,1]
   * then to [-max_out_val/2,max_out_val/2]*/
  shift += (double)offset / 127 * max_out_val / 2;

  for(i=0;i<=max_in_val;i++){
    j = rise*i + shift;

    if(j<out_min){
      j=out_min;
    }
    else if(j>out_max){
      j=out_max;
    }

    *lut_p=j;
    lut_p++;
  }

  hexdump(5, "load_lut: ", lut, max_in_val+1);

  DBG (10, "load_lut: finish\n");
  return ret;
}

