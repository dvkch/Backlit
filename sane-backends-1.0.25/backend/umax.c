/* --------------------------------------------------------------------------------------------------------- */

/* sane - Scanner Access Now Easy.

   umax.c 

   (C) 1997-2007 Oliver Rauch

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

   This file implements a SANE backend for UMAX flatbed scanners.  */


/* --------------------------------------------------------------------------------------------------------- */

#define BUILD 45

/* --------------------------------------------------------------------------------------------------------- */


/* SANE-FLOW-DIAGRAMM

	- sane_init() : initialize backend, attach scanners(devicename,0)
	. - sane_get_devices() : query list of scanner-devices
	. - sane_open() : open a particular scanner-device and attach_scanner(devicename,&dev)
	. . - sane_set_io_mode : set blocking-mode
	. . - sane_get_select_fd : get scanner-fd
	. . - sane_get_option_descriptor() : get option information
	. . - sane_control_option() : change option values
	. .
	. . - sane_start() : start image aquisition
	. .   - sane_get_parameters() : returns actual scan-parameters
	. .   - sane_read() : read image-data (from pipe)
in ADF mode this is done often:
	. . - sane_start() : start image aquisition
	. .   - sane_get_parameters() : returns actual scan-parameters
	. .   - sane_read() : read image-data (from pipe)

	. . - sane_cancel() : cancel operation, kill reader_process

	. - sane_close() : close opened scanner-device, do_cancel, free buffer and handle
	- sane_exit() : terminate use of backend, free devicename and device-struture
*/


/* ------------------------------------------------------------ DBG OUTPUT LEVELS -------------------------- */


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


/* ------------------------------------------------------------ SANE DEFINES ------------------------------- */

#define BACKEND_NAME     umax
#define UMAX_CONFIG_FILE "umax.conf"

/* ------------------------------------------------------------ SANE INTERNATIONALISATION ------------------ */

#ifndef SANE_I18N
#define SANE_I18N(text) text
#endif 

/* ------------------------------------------------------------ INCLUDES ----------------------------------- */

#include "../include/sane/config.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_scsi.h"
#include "../include/sane/sanei_debug.h"
#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_thread.h"

#ifdef UMAX_ENABLE_USB
# include "sane/sanei_usb.h"
#endif

#include <math.h>
#include <string.h>

#include "umax-scsidef.h"
#include "umax-scanner.c"

#ifdef UMAX_ENABLE_USB
# include "umax-usb.c"
#endif

#include "umax.h"

/* ------------------------------------------------------------ SANE DEFINES ------------------------------- */

#ifndef PATH_MAX
#define PATH_MAX         1024
#endif

/* ------------------------------------------------------------ OPTION DEFINITIONS ------------------------- */

#define SANE_NAME_BATCH_SCAN_START	"batch-scan-start"
#define SANE_TITLE_BATCH_SCAN_START	"Batch scan start"
#define SANE_DESC_BATCH_SCAN_START	"set for first scan of batch"

#define SANE_NAME_BATCH_SCAN_LOOP	"batch-scan-loop"
#define SANE_TITLE_BATCH_SCAN_LOOP	"Batch scan loop"
#define SANE_DESC_BATCH_SCAN_LOOP	"set for middle scans of batch"

#define SANE_NAME_BATCH_SCAN_END	"batch-scan-end"
#define SANE_TITLE_BATCH_SCAN_END	"Batch scan end"
#define SANE_DESC_BATCH_SCAN_END	"set for last scan of batch"

#define SANE_NAME_BATCH_NEXT_TL_Y	"batch-scan-next-tl-y"
#define SANE_TITLE_BATCH_NEXT_TL_Y	"Batch scan next top left Y"
#define SANE_DESC_BATCH_NEXT_TL_Y	"Set top left Y position for next scan"

#define SANE_UMAX_NAME_SELECT_CALIBRATON_EXPOSURE_TIME		"select-calibration-exposure-time"
#define SANE_UMAX_TITLE_SELECT_CALIBRATION_EXPOSURE_TIME	"Set calibration exposure time"
#define SANE_UMAX_DESC_SELECT_CALIBRATION_EXPOSURE_TIME		"Allow different settings for calibration and scan exposure times"

/* ------------------------------------------------------------ STRING DEFINITIONS ------------------------- */

#define FLB_STR			SANE_I18N("Flatbed")
#define UTA_STR			SANE_I18N("Transparency Adapter")
#define ADF_STR 		SANE_I18N("Automatic Document Feeder")

#define LINEART_STR		SANE_VALUE_SCAN_MODE_LINEART
#define HALFTONE_STR		SANE_VALUE_SCAN_MODE_HALFTONE
#define GRAY_STR		SANE_VALUE_SCAN_MODE_GRAY
#define COLOR_LINEART_STR	SANE_VALUE_SCAN_MODE_COLOR_LINEART
#define COLOR_HALFTONE_STR	SANE_VALUE_SCAN_MODE_COLOR_HALFTONE
#define COLOR_STR		SANE_VALUE_SCAN_MODE_COLOR

/* ------------------------------------------------------------ DEFINITIONS -------------------------------- */

#define P_200_TO_255(per) ( (SANE_UNFIX(per) + 100) * 255/200 )
#define P_100_TO_255(per) SANE_UNFIX(per * 255/100 )
#define P_100_TO_254(per) 1+SANE_UNFIX(per * 254/100 )

/* ------------------------------------------------------------ GLOBAL VARIABLES --------------------------- */


static SANE_String scan_mode_list[7];
static SANE_String_Const source_list[4];
static SANE_Int bit_depth_list[9];
static SANE_Auth_Callback frontend_authorize_callback;

static int umax_scsi_buffer_size_min = 32768;  /* default: minimum scsi buffer size: 32 KB */
static int umax_scsi_buffer_size_max = 131072; /* default: maximum scsi buffer size: 128 KB */

/* number of lines that shall be scanned in one buffer for preview if possible */
/* this value should not be too large because it defines the step size in which */
/* the scanned parts are displayed while preview scan is in progress */
static int umax_preview_lines                  = 10; /* default: 10 preview lines */
/* number of lines that shall be scanned in one buffer for scan if possible */
static int umax_scan_lines                     = 40; /* default: 40 scan lines */
static int umax_scsi_maxqueue                  = 2; /* use command queueing depth 2 as default */
static int umax_handle_bad_sense_error         = 0; /* default: handle bad sense error code as device busy */
static int umax_execute_request_sense          = 0; /* default: do not use request sense in do_calibration */
static int umax_force_preview_bit_rgb          = 0; /* default: do not force preview bit in real color scan */
static int umax_slow                           = -1; /* don`t use slow scanning speed */
static int umax_smear                          = -1; /* don`t care about image smearing problem */
static int umax_calibration_area               = -1;   /* -1=auto, 0=calibration on image, 1=calibration for full ccd */
static int umax_calibration_width_offset       = -99999; /* -99999=auto */
static int umax_calibration_width_offset_batch = -99999; /* -99999=auto */
static int umax_calibration_bytespp            = -1;   /* -1=auto */
static int umax_exposure_time_rgb_bind         = -1;   /* -1=auto */
static int umax_invert_shading_data            = -1;   /* -1=auto */
static int umax_lamp_control_available         = 0;    /* 0=disabled */
static int umax_gamma_lsb_padded               = -1;   /* -1=auto */
static int umax_connection_type                = 1;    /* 1=scsi, 2=usb */

/* ------------------------------------------------------------ CALIBRATION MODE --------------------------- */

#ifdef UMAX_CALIBRATION_MODE_SELECTABLE

#define CALIB_MODE_0000		SANE_I18N("Use Image Composition")
#define CALIB_MODE_1111		SANE_I18N("Bi-level black and white (lineart mode)")
#define CALIB_MODE_1110		SANE_I18N("Dithered/halftone black & white (halftone mode)")
#define CALIB_MODE_1101		SANE_I18N("Multi-level black & white (grayscale mode)")
#define CALIB_MODE_1010		SANE_I18N("Multi-level RGB color (one pass color)")
#define CALIB_MODE_1001		SANE_I18N("Ignore calibration")

static SANE_String_Const calibration_list[] =
{
    CALIB_MODE_0000,
    CALIB_MODE_1111,
    CALIB_MODE_1110,
    CALIB_MODE_1101,
    CALIB_MODE_1010,
    CALIB_MODE_1001,
    0
};

#endif

/* --------------------------------------------------------------------------------------------------------- */

enum
{
    UMAX_CALIBRATION_AREA_IMAGE = 0,
    UMAX_CALIBRATION_AREA_CCD
};

static const SANE_Int pattern_dim_list[] =
{
  5, /* # of elements */
  2, 4, 6, 8, 12
};

static const SANE_Range u8_range =
{
    0, /* minimum */
  255, /* maximum */
    0  /* quantization */
};

static const SANE_Range percentage_range =
{
  -100 << SANE_FIXED_SCALE_SHIFT, /* minimum */
   100 << SANE_FIXED_SCALE_SHIFT, /* maximum */
     1 << SANE_FIXED_SCALE_SHIFT  /* quantization */
};

static const SANE_Range percentage_range_100 =
{
     0 << SANE_FIXED_SCALE_SHIFT, /* minimum */
   100 << SANE_FIXED_SCALE_SHIFT, /* maximum */
     0 << SANE_FIXED_SCALE_SHIFT  /* quantization */
};

static int num_devices = 0;
static const SANE_Device **devlist = NULL;
static Umax_Device *first_dev      = NULL;
static Umax_Scanner *first_handle  = NULL;


/* ------------------------------------------------------------ MATH-HELPERS ------------------------------- */


#define min(a, b) (((a)<(b))?(a):(b))
#define max(a, b) (((a)>(b))?(a):(b))
#define inrange(minimum, val, maximum) (min(maximum, max(val, minimum)))


/* ------------------------------------------------------------ umax_test_little_endian -------------------- */

static SANE_Bool umax_test_little_endian(void)
{
  SANE_Int testvalue = 255;
  unsigned char *firstbyte = (unsigned char *) &testvalue;

  if (*firstbyte == 255)
  {
    return SANE_TRUE;
  }

  return SANE_FALSE;
}

/* ------------------------------------------------------------ DBG_inq_nz --------------------------------- */


static void DBG_inq_nz(char *text, int flag)
{
  if (flag != 0) { DBG(DBG_inquiry,"%s", text); }
}


/*------------------------------------------------------------- DBG_sense_nz ------------------------------- */


static void DBG_sense_nz(char *text, int flag)
{
  if (flag != 0) { DBG(DBG_sense,"%s", text); }
}


/* ------------------------------------------------------------ UMAX PRINT INQUIRY ------------------------- */


static void umax_print_inquiry(Umax_Device *dev)
{
 unsigned char *inquiry_block;
 int i;

  inquiry_block=dev->buffer[0];

  DBG(DBG_inquiry,"INQUIRY:\n");
  DBG(DBG_inquiry,"========\n");
  DBG(DBG_inquiry,"\n");
  DBG(DBG_inquiry,"vendor........................: '%s'\n", dev->vendor);
  DBG(DBG_inquiry,"product.......................: '%s'\n", dev->product);
  DBG(DBG_inquiry,"version.......................: '%s'\n", dev->version);

  DBG(DBG_inquiry,"peripheral qualifier..........: %d\n", get_inquiry_periph_qual(inquiry_block));
  DBG(DBG_inquiry,"peripheral device type........: %d\n", get_inquiry_periph_devtype(inquiry_block));

  DBG_inq_nz("RMB bit set (reserved)\n", get_inquiry_rmb(inquiry_block));
  DBG_inq_nz("0x01 bit 6\n", get_inquiry_0x01_bit6(inquiry_block));
  DBG_inq_nz("0x01 bit 5\n", get_inquiry_0x01_bit5(inquiry_block));

  DBG(DBG_inquiry,"\n");
  DBG(DBG_inquiry,"CBHS value range..............: %s\n", cbhs_str[dev->inquiry_cbhs]);
  DBG(DBG_inquiry,"scanmode......................: %s\n", scanmode_str[get_inquiry_scanmode(inquiry_block)]);
  if (dev->inquiry_transavail)
  {
    DBG(DBG_inquiry,"UTA (transparency)............: available\n");

    if (get_inquiry_translamp(inquiry_block) == 0)
    { DBG(DBG_inquiry,"UTA lamp status ..............: off\n"); }
    else
    { DBG(DBG_inquiry,"UTA lamp status ..............: on\n"); }

    DBG(DBG_inquiry,"\n");
  }

  DBG(DBG_inquiry,"inquiry block length..........: %d bytes\n", dev->inquiry_len);
  if (dev->inquiry_len<=0x8e)
  {
    DBG(DBG_inquiry, "Inquiry block is unexpected short, should be at least 147 bytes\n");
  } 

  DBG(DBG_inquiry,"\n");
  DBG(DBG_inquiry,"ISO  Version (reserved).......: %d\n", get_inquiry_iso_version(inquiry_block));
  DBG(DBG_inquiry,"ECMA Version (reserved).......: %d\n", get_inquiry_ecma_version(inquiry_block));
  DBG(DBG_inquiry,"ANSI Version .................: %d\n", get_inquiry_ansi_version(inquiry_block));
  DBG(DBG_inquiry,"\n");

  DBG_inq_nz("AENC bit set (reserved)\n",  get_inquiry_aenc(inquiry_block));
  DBG_inq_nz("TmIOP bit set (reserved)\n", get_inquiry_tmiop(inquiry_block));
  DBG_inq_nz("0x03 bit 5\n",               get_inquiry_0x03_bit5(inquiry_block));
  DBG_inq_nz("0x03 bit 4\n",               get_inquiry_0x03_bit4(inquiry_block));

  DBG(DBG_inquiry,"reserved byte 0x05 = %d\n", get_inquiry_0x05(inquiry_block));
  DBG(DBG_inquiry,"reserved byte 0x06 = %d\n", get_inquiry_0x06(inquiry_block));

  DBG(DBG_inquiry,"\n");
  DBG(DBG_inquiry,"scsi features (%02x):\n", get_inquiry_scsi_byte(inquiry_block));
  DBG(DBG_inquiry,"-------------------\n");
  DBG_inq_nz(" - relative address\n", get_inquiry_scsi_reladr(inquiry_block));
  DBG_inq_nz(" - wide bus 32 bit\n",  get_inquiry_scsi_wbus32(inquiry_block));
  DBG_inq_nz(" - wide bus 16 bit\n",  get_inquiry_scsi_wbus16(inquiry_block));
  DBG_inq_nz(" - syncronous neg.\n",  get_inquiry_scsi_sync(inquiry_block));
  DBG_inq_nz(" - linked commands\n",  get_inquiry_scsi_linked(inquiry_block));
  DBG_inq_nz(" - (reserved)\n",       get_inquiry_scsi_R(inquiry_block));
  DBG_inq_nz(" - command queueing\n", get_inquiry_scsi_cmdqueue(inquiry_block));
  DBG_inq_nz(" - sftre\n",            get_inquiry_scsi_sftre(inquiry_block));

  /* 0x24 */
  if (dev->inquiry_len<=0x24)
  {
    return;
  }

  DBG(DBG_inquiry,"\n");
  DBG(DBG_inquiry,"f/w support function:\n");
  DBG(DBG_inquiry,"---------------------\n");
  DBG_inq_nz(" - quality calibration\n", dev->inquiry_quality_ctrl);
  DBG_inq_nz(" - fast preview function\n", dev->inquiry_preview);
  DBG_inq_nz(" - shadow compensation by f/w\n", get_inquiry_fw_shadow_comp(inquiry_block));
  DBG_inq_nz(" - reselection phase\n", get_inquiry_fw_reselection(inquiry_block));
  DBG_inq_nz(" - lamp intensity control\n", dev->inquiry_lamp_ctrl);
  DBG_inq_nz(" - batch scan function\n", get_inquiry_fw_batch_scan(inquiry_block));
  DBG_inq_nz(" - calibration mode control by driver\n", get_inquiry_fw_calibration(inquiry_block));

  /* 0x36, 0x37 */
  DBG(DBG_inquiry,"\n");
  DBG(DBG_inquiry,"reserved byte 0x36 = %d\n", get_inquiry_0x36(inquiry_block));
  DBG(DBG_inquiry,"reserved byte 0x37 = %d\n", get_inquiry_0x37(inquiry_block));

  if (get_inquiry_fw_adjust_exposure_tf(inquiry_block))
  {
    int unit;

    DBG(DBG_inquiry,"\n");
    DBG(DBG_inquiry,"adjust exposure time function\n");
    unit=get_inquiry_exposure_time_step_unit(inquiry_block);
    DBG(DBG_inquiry,"exposure time step units......: %d micro-sec\n", unit);
    DBG(DBG_inquiry,"exposure time maximum.........: %d micro-sec\n",
            unit*get_inquiry_exposure_time_max(inquiry_block));
    DBG(DBG_inquiry,"exposure time minimum (LHG)...: %d micro-sec\n",
            unit*get_inquiry_exposure_time_lhg_min(inquiry_block));
    DBG(DBG_inquiry,"exposure time minimum color...: %d micro-sec\n",
            unit*get_inquiry_exposure_time_color_min(inquiry_block));
    DBG(DBG_inquiry,"exposure time default FB (LH).: %d micro-sec\n",
            unit*get_inquiry_exposure_time_lh_def_fb(inquiry_block));
    DBG(DBG_inquiry,"exposure time default UTA (LH): %d micro-sec\n",
            unit*get_inquiry_exposure_time_lh_def_uta(inquiry_block));
    DBG(DBG_inquiry,"exposure time default FB gray.: %d micro-sec\n",
            unit*get_inquiry_exposure_time_gray_def_fb(inquiry_block));
    DBG(DBG_inquiry,"exposure time default UTA gray: %d micro-sec\n",
            unit*get_inquiry_exposure_time_gray_def_uta(inquiry_block));
    DBG(DBG_inquiry,"exposure time default FB red..: %d micro-sec\n",
            unit*get_inquiry_exposure_time_def_r_fb(inquiry_block));
    DBG(DBG_inquiry,"exposure time default FB grn..: %d micro-sec\n",
            unit*get_inquiry_exposure_time_def_g_fb(inquiry_block));
    DBG(DBG_inquiry,"exposure time default FB blue.: %d micro-sec\n",
            unit*get_inquiry_exposure_time_def_b_fb(inquiry_block));
    DBG(DBG_inquiry,"exposure time default UTA red.: %d micro-sec\n",
            unit*get_inquiry_exposure_time_def_r_uta(inquiry_block));
    DBG(DBG_inquiry,"exposure time default UTA grn.: %d micro-sec\n",
            unit*get_inquiry_exposure_time_def_g_uta(inquiry_block));
    DBG(DBG_inquiry,"exposure time default UTA blue: %d micro-sec\n",
            unit*get_inquiry_exposure_time_def_b_uta(inquiry_block));
  }


  /* 0x60 */
  if (dev->inquiry_len<=0x60)
  {
    return;
  }

  DBG(DBG_inquiry,"\n");
  DBG(DBG_inquiry,"scan modes (%02x):\n", get_inquiry_sc_feature_byte0(inquiry_block));
  DBG(DBG_inquiry,"----------------\n");
  DBG_inq_nz(" - three passes color mode\n",		get_inquiry_sc_three_pass_color(inquiry_block));
  DBG_inq_nz(" - single pass color mode\n",		get_inquiry_sc_one_pass_color(inquiry_block));
  DBG_inq_nz(" - lineart mode\n",			dev->inquiry_lineart);
  DBG_inq_nz(" - halftone mode\n",			dev->inquiry_halftone);
  DBG_inq_nz(" - gray mode\n",				dev->inquiry_gray);
  DBG_inq_nz(" - color mode\n",				dev->inquiry_color);
  DBG_inq_nz(" - transparency (UTA)\n",			dev->inquiry_uta);
  DBG_inq_nz(" - automatic document feeder (ADF)\n",	dev->inquiry_adf);

  /* 0x61 */
  if (dev->inquiry_len<=0x61)
  {
    return;
  }

  DBG(DBG_inquiry,"\n");
  DBG(DBG_inquiry,"scanner capability (%02x, %02x, %02x):\n",
      get_inquiry_sc_feature_byte1(inquiry_block),
      get_inquiry_sc_feature_byte2(inquiry_block),
      get_inquiry_sc_feature_byte3(inquiry_block));
  DBG(DBG_inquiry,"--------------------------------\n");
  DBG_inq_nz(" - double resolution\n",			dev->inquiry_dor);
  DBG_inq_nz(" - send high byte first\n",		get_inquiry_sc_high_byte_first(inquiry_block));
  DBG_inq_nz(" - bi-level image reverse\n",		dev->inquiry_reverse);
  DBG_inq_nz(" - multi-level image reverse\n",		dev->inquiry_reverse_multi);
  DBG_inq_nz(" - support shadow function\n",		dev->inquiry_shadow);
  DBG_inq_nz(" - support highlight function\n",		dev->inquiry_highlight);
  DBG_inq_nz(" - f/w downloadable\n",			get_inquiry_sc_downloadable_fw(inquiry_block));
  DBG_inq_nz(" - paper length can reach to 14 inch\n",	get_inquiry_sc_paper_length_14(inquiry_block));

  /* 0x62 */
  if (dev->inquiry_len<=0x62)
  {
    return;
  }

  DBG_inq_nz(" - shading data/gain uploadable\n",	get_inquiry_sc_uploadable_shade(inquiry_block));
  DBG_inq_nz(" - analog gamma correction\n",		dev->inquiry_analog_gamma);
  DBG_inq_nz(" - x, y coordinate base\n",		get_inquiry_xy_coordinate_base(inquiry_block));
  DBG_inq_nz(" - lineart starts with LSB\n",		get_inquiry_lineart_order(inquiry_block));
  DBG_inq_nz(" - start density \n",			get_inquiry_start_density(inquiry_block));
  DBG_inq_nz(" - hardware x scaling\n",			get_inquiry_hw_x_scaling(inquiry_block));
  DBG_inq_nz(" - hardware y scaling\n",			get_inquiry_hw_y_scaling(inquiry_block));

  /* 0x63 */
  if (dev->inquiry_len<=0x63)
  {
    return;
  }

  DBG_inq_nz(" + ADF: no paper\n",			get_inquiry_ADF_no_paper(inquiry_block));
  DBG_inq_nz(" + ADF: cover open\n",			get_inquiry_ADF_cover_open(inquiry_block));
  DBG_inq_nz(" + ADF: paper jam\n",			get_inquiry_ADF_paper_jam(inquiry_block));
  DBG_inq_nz(" - unknwon flag; 0x63 bit 3\n",		get_inquiry_0x63_bit3(inquiry_block));
  DBG_inq_nz(" - unknown lfag: 0x63 bit 4\n",		get_inquiry_0x63_bit4(inquiry_block));
  DBG_inq_nz(" - lens calib in doc pos\n",		get_inquiry_lens_cal_in_doc_pos(inquiry_block));
  DBG_inq_nz(" - manual focus\n",			get_inquiry_manual_focus(inquiry_block));
  DBG_inq_nz(" - UTA lens calib pos selectable\n",	get_inquiry_sel_uta_lens_cal_pos(inquiry_block));

  /* 0x64 - 0x68*/
  if (dev->inquiry_len<=0x68)
  {
    return;
  }

  if (dev->inquiry_gamma_dwload)
  {
    DBG(DBG_inquiry,"\n");
    DBG(DBG_inquiry,"gamma download available\n");
    DBG_inq_nz("gamma download type 2\n", get_inquiry_gamma_type_2(inquiry_block));
    DBG(DBG_inquiry,"lines of gamma curve: %s\n", gamma_lines_str[get_inquiry_gamma_lines(inquiry_block)]);

    /* 0x66 */
    DBG_inq_nz("gamma input   8 bits/pixel support\n", get_inquiry_gib_8bpp(inquiry_block));
    DBG_inq_nz("gamma input   9 bits/pixel support\n", get_inquiry_gib_9bpp(inquiry_block));
    DBG_inq_nz("gamma input  10 bits/pixel support\n", get_inquiry_gib_10bpp(inquiry_block));
    DBG_inq_nz("gamma input  12 bits/pixel support\n", get_inquiry_gib_12bpp(inquiry_block));
    DBG_inq_nz("gamma input  14 bits/pixel support\n", get_inquiry_gib_14bpp(inquiry_block));
    DBG_inq_nz("gamma input  16 bits/pixel support\n", get_inquiry_gib_16bpp(inquiry_block));
    DBG_inq_nz("0x66 bit 6\n", get_inquiry_0x66_bit6(inquiry_block));
    DBG_inq_nz("0x66 bit 7\n", get_inquiry_0x66_bit7(inquiry_block));

    /* 0x68 */
    DBG_inq_nz("gamma output  8 bits/pixel support\n", get_inquiry_gob_8bpp(inquiry_block));
    DBG_inq_nz("gamma output  9 bits/pixel support\n", get_inquiry_gob_9bpp(inquiry_block));
    DBG_inq_nz("gamma output 10 bits/pixel support\n", get_inquiry_gob_10bpp(inquiry_block));
    DBG_inq_nz("gamma output 12 bits/pixel support\n", get_inquiry_gob_12bpp(inquiry_block));
    DBG_inq_nz("gamma output 14 bits/pixel support\n", get_inquiry_gob_14bpp(inquiry_block));
    DBG_inq_nz("gamma output 16 bits/pixel support\n", get_inquiry_gob_16bpp(inquiry_block));
    DBG_inq_nz("0x68 bit 6\n", get_inquiry_0x68_bit6(inquiry_block));
    DBG_inq_nz("0x68 bit 7\n", get_inquiry_0x68_bit7(inquiry_block));
  }

  /* 0x64 - 0x68 reserved bits */
  DBG_inq_nz("0x64 bit 2\n", get_inquiry_0x64_bit2(inquiry_block));
  DBG_inq_nz("0x64 bit 3\n", get_inquiry_0x64_bit3(inquiry_block));
  DBG_inq_nz("0x64 bit 4\n", get_inquiry_0x64_bit4(inquiry_block));
  DBG_inq_nz("0x64 bit 6\n", get_inquiry_0x64_bit6(inquiry_block));

  DBG(DBG_inquiry,"\n");
  DBG(DBG_inquiry,"reserved byte 0x65 = %d\n", get_inquiry_0x65(inquiry_block));
  DBG(DBG_inquiry,"reserved byte 0x67 = %d\n", get_inquiry_0x67(inquiry_block));


  /* 0x69 */
  if (dev->inquiry_len<=0x69)
  {
    return;
  }

  DBG(DBG_inquiry,"\n");
  if (get_inquiry_hda(inquiry_block))
  {
    DBG(DBG_inquiry,"halftone download available\n");
    DBG(DBG_inquiry,"halftone pattern download max matrix %dx%d\n",
                    get_inquiry_max_halftone_matrix(inquiry_block),
                    get_inquiry_max_halftone_matrix(inquiry_block));
  }

  /* 0x6a */
  if (dev->inquiry_len<=0x6a)
  {
    return;
  }

  DBG_inq_nz("built-in halftone patterns:\n", get_inquiry_halftones_supported(inquiry_block));
  DBG_inq_nz("built-in halftone pattern size ............: 2x2\n", get_inquiry_halftones_2x2(inquiry_block));
  DBG_inq_nz("built-in halftone pattern size ............: 4x4\n", get_inquiry_halftones_4x4(inquiry_block));
  DBG_inq_nz("built-in halftone pattern size ............: 6x6\n", get_inquiry_halftones_6x6(inquiry_block));
  DBG_inq_nz("built-in halftone pattern size ............: 8x8\n", get_inquiry_halftones_8x8(inquiry_block));
  DBG_inq_nz("built-in halftone pattern size ............: 12x12\n", get_inquiry_halftones_12x12(inquiry_block));

  /* 0x6b, 0x6c */
  DBG(DBG_inquiry,"reserved byte 0x6b = %d\n", get_inquiry_0x6b(inquiry_block));
  DBG(DBG_inquiry,"reserved byte 0x6c = %d\n", get_inquiry_0x6c(inquiry_block));


  /* 0x6d */
  if (dev->inquiry_len<=0x6d)
  {
    return;
  }

  DBG(DBG_inquiry,"\n");
  DBG(DBG_inquiry,"color sequence............................: %s\n",
      color_sequence_str[get_inquiry_colorseq(inquiry_block)]);
  DBG_inq_nz("color ordering support....................: pixel\n",
             get_inquiry_color_order_pixel(inquiry_block));
  DBG_inq_nz("color ordering support....................: line without CCD distance\n",
             get_inquiry_color_order_line_no_ccd(inquiry_block));
  DBG_inq_nz("color ordering support....................: plane\n",
             get_inquiry_color_order_plane(inquiry_block));
  DBG_inq_nz("color ordering support....................: line with CCD distance\n",
             get_inquiry_color_order_line_w_ccd(inquiry_block));
  DBG_inq_nz("color ordering support....................: (reserved)\n",
             get_inquiry_color_order_reserved(inquiry_block));

  /* 0x6e */
  if (dev->inquiry_len<=0x71)
  {
    return;
  }

  DBG(DBG_inquiry,"\n");
  DBG(DBG_inquiry,"maximum video memory......................: %d KB\n", dev->inquiry_vidmem/1024);

  /* 0x72 */
  DBG(DBG_inquiry,"\n");
  DBG(DBG_inquiry,"reserved byte 0x72 = %d\n", get_inquiry_0x72(inquiry_block));
  DBG(DBG_inquiry,"\n");

  /* 0x73/0x94 - 0x75/0x96 */
  if (dev->inquiry_len<=0x75)
  {
    return;
  }

  DBG(DBG_inquiry,"optical resolution........................: %d dpi\n", dev->inquiry_optical_res);
  DBG(DBG_inquiry,"maximum x-resolution......................: %d dpi\n", dev->inquiry_x_res);
  DBG(DBG_inquiry,"maximum y-resolution......................: %d dpi\n", dev->inquiry_y_res);

  /* ---------- */

  /* 0x76 0x77 */
  if (dev->inquiry_len<=0x77)
  {
    return;
  }

  DBG(DBG_inquiry,"\n");
  DBG(DBG_inquiry,"FB (flatbed-mode):\n");
  DBG(DBG_inquiry,"FB maximum scan width.....................: %2.2f inch\n", dev->inquiry_fb_width);
  DBG(DBG_inquiry,"FB maximum scan length....................: %2.2f inch\n", dev->inquiry_fb_length);

  /* ---------- */
  
  /* 0x7a - 0x81 */
  if (dev->inquiry_len<=0x81)
  {
    return;
  }

  DBG(DBG_inquiry,"\n");
  DBG(DBG_inquiry,"UTA (transparency-mode):\n");
  DBG(DBG_inquiry,"UTA x-original point......................: %2.2f inch\n", dev->inquiry_uta_x_off);
  DBG(DBG_inquiry,"UTA y-original point......................: %2.2f inch\n", dev->inquiry_uta_y_off);
  DBG(DBG_inquiry,"UTA maximum scan width....................: %2.2f inch\n", dev->inquiry_uta_width); 
  DBG(DBG_inquiry,"UTA maximum scan length...................: %2.2f inch\n", dev->inquiry_uta_length);
 
  /* ---------- */

  /* 0x82-0x85 */
  DBG(DBG_inquiry,"\n");
  DBG(DBG_inquiry,"reserved byte 0x82 = %d\n", get_inquiry_0x82(inquiry_block));

  /* ---------- */

  /* 0x83/0xa0 - 0x85/0xa2 */
  if (dev->inquiry_len<=0x85)
  {
    return;
  }

  DBG(DBG_inquiry,"\n");
  DBG(DBG_inquiry,"DOR (double optical resolution-mode):\n");
  DBG(DBG_inquiry,"DOR optical resolution....................: %d dpi\n", dev->inquiry_dor_optical_res);
  DBG(DBG_inquiry,"DOR maximum x-resolution..................: %d dpi\n", dev->inquiry_dor_x_res);
  DBG(DBG_inquiry,"DOR maximum y-resolution..................: %d dpi\n", dev->inquiry_dor_y_res);

  /* 0x86 - 0x8d */
  if (dev->inquiry_len<=0x8d)
  {
    return;
  }

  DBG(DBG_inquiry,"DOR x-original point......................: %2.2f inch\n", dev->inquiry_dor_x_off);
  DBG(DBG_inquiry,"DOR y-original point......................: %2.2f inch\n", dev->inquiry_dor_y_off);
  DBG(DBG_inquiry,"DOR maximum scan width....................: %2.2f inch\n", dev->inquiry_dor_width);
  DBG(DBG_inquiry,"DOR maximum scan length...................: %2.2f inch\n", dev->inquiry_dor_length);
  DBG(DBG_inquiry,"\n");

  /* ---------- */

  /* 0x8e */
  DBG(DBG_inquiry,"reserved byte 0x8e = %d\n", get_inquiry_0x8e(inquiry_block));
  DBG(DBG_inquiry,"\n");

  /* ---------- */

  /* 0x8f */
  if (dev->inquiry_len<=0x8f)
  {
    return;
  }

  DBG(DBG_inquiry,"last calibration lamp density.............: %d\n",
      get_inquiry_last_calibration_lamp_density(inquiry_block));

  /* 0x90 */
  DBG(DBG_inquiry,"\n");
  DBG(DBG_inquiry,"reserved byte 0x90 = %d\n", get_inquiry_0x90(inquiry_block));
  DBG(DBG_inquiry,"\n");

  /* 0x91 */
  if (dev->inquiry_len<=0x91)
  {
    return;
  }

  DBG(DBG_inquiry,"lamp warmup maximum time..................: %d sec\n", dev->inquiry_max_warmup_time);
 
  /* 0x92 0x93 */
  if (dev->inquiry_len<=0x93)
  {
    return;
  }

  DBG(DBG_inquiry,"window descriptor block length............: %d bytes\n", get_inquiry_wdb_length(inquiry_block));

  /* ----------------- */

  /* 0x97 */
  if (dev->inquiry_len<=0x97)
  {
    return;
  }

  if (get_inquiry_analog_gamma_table(inquiry_block) == 0)
  {
    DBG(DBG_inquiry,"no analog gamma function\n");
  }
  else
  {
    DBG(DBG_inquiry,"mp 8832 analog gamma table\n");
  }

  /* 0x98, 0x99 */
  DBG(DBG_inquiry,"\n");
  DBG(DBG_inquiry,"reserved byte 0x98 = %d\n", get_inquiry_0x98(inquiry_block));
  DBG(DBG_inquiry,"reserved byte 0x99 = %d\n", get_inquiry_0x99(inquiry_block));
  DBG(DBG_inquiry,"\n");

  /* 0x9a */
  if (dev->inquiry_len<=0x9a)
  {
    return;
  }

  DBG(DBG_inquiry,"maximum calibration data lines for shading: %d\n",
      get_inquiry_max_calibration_data_lines(inquiry_block));

  /* 0x9b */
  if (dev->inquiry_len<=0x9b)
  {
    return;
  }

  DBG(DBG_inquiry,"fb/uta: color line arrangement mode.......: %d\n",
      get_inquiry_fb_uta_line_arrangement_mode(inquiry_block));

  /* 0x9c */
  if (dev->inquiry_len<=0x9c)
  {
    return;
  }

  DBG(DBG_inquiry,"adf:    color line arrangement mode.......: %d\n",
      get_inquiry_adf_line_arrangement_mode(inquiry_block));

  /* 0x9d */
  if (dev->inquiry_len<=0x9d)
  {
    return;
  }

  DBG(DBG_inquiry,"CCD line distance.........................: %d\n",
      get_inquiry_CCD_line_distance(inquiry_block));

  DBG(DBG_inquiry,"\n");
  DBG(DBG_inquiry,"reserved byte 0x9e = %d\n", get_inquiry_0x9e(inquiry_block));

  /* 0xa2 following */
  if (dev->inquiry_len<=0xa2)
  {
    return;
  }

  DBG(DBG_inquiry,"\n");
  for(i=0xa3; i<dev->inquiry_len; i++)
  {
    DBG(DBG_inquiry,"unknown reserved byte 0x%x = %d\n", i, inquiry_block[i]);
  }
}


/* ------------------------------------------------------------ CBHS_CORRECT ------------------------------- */


static int umax_cbhs_correct(int minimum, int cbhs, int maximum)
{
 int range = maximum - minimum + 1;

  if (range == 256)
  {
    return cbhs;
  }

 return (int)( (cbhs/256.0)*range + minimum );
}


/* ------------------------------------------------------------ SENSE_HANDLER ------------------------------ */


static SANE_Status sense_handler(int scsi_fd, unsigned char *result, void *arg)	  /* is called by sanei_scsi */
{
 unsigned char asc, ascq, sensekey;
 int           asc_ascq, len;
 Umax_Device   *dev = arg;

  DBG(DBG_proc, "check condition sense handler (scsi_fd = %d)\n", scsi_fd);

  sensekey = get_RS_sense_key(result);
  asc      = get_RS_ASC(result);
  ascq     = get_RS_ASCQ(result);
  asc_ascq = (int)(256 * asc + ascq);
  len      = 7 + get_RS_additional_length(result);

  if ( get_RS_error_code(result) != 0x70 ) 
  {
    DBG(DBG_error, "invalid sense key error code (%d)\n", get_RS_error_code(result));

    switch (dev->handle_bad_sense_error)
    {
      default:
      case 0:
        DBG(DBG_error, "=> handled as DEVICE BUSY!\n");
       return SANE_STATUS_DEVICE_BUSY;

      case 1:
        DBG(DBG_error, "=> handled as ok!\n");
       return SANE_STATUS_GOOD;

      case 2:
        DBG(DBG_error, "=> handled as i/o error!\n");
       return SANE_STATUS_IO_ERROR;

      case 3:
        DBG(DBG_error, "=> ignored, sense handler does continue\n");
    }
  }

  DBG(DBG_sense, "check condition sense: %s\n", sense_str[sensekey]);

  /* when we reach here then we have no valid data in buffer[0] */
  /* but it may be helpful to have the result data in buffer[0] */
  memset(dev->buffer[0], 0, rs_return_block_size);  /* clear sense data buffer */
  memcpy(dev->buffer[0], result, len+1); /* copy sense data to buffer */

  if (len > 0x15)
  {
   int scanner_error = get_RS_scanner_error_code(result);

    if (scanner_error < 100)
    {
      DBG(DBG_sense, "-> %s (#%d)\n", scanner_error_str[scanner_error], scanner_error);
    }
    else
    {
      DBG(DBG_sense, "-> error %d\n", scanner_error);
    }
  }

  if (get_RS_ILI(result) != 0)
  {
    DBG(DBG_sense, "-> ILI-ERROR: requested data length is larger than actual length\n");
  }

  switch(sensekey)
  {
    case 0x00:											 /* no sense */
      return SANE_STATUS_GOOD;
     break;


    case 0x03:										     /* medium error */
      if (asc_ascq == 0x1400)
      {
        DBG(DBG_sense, "-> misfeed, paper jam\n");
        return SANE_STATUS_JAMMED;
      }
      else if (asc_ascq == 0x1401)
      {
        DBG(DBG_sense, "-> adf not ready\n");
        return SANE_STATUS_NO_DOCS;
      }
      else
      {
        DBG(DBG_sense, "-> unknown medium error: asc=%d, ascq=%d\n", asc, ascq);
      }
     break;


    case 0x04:										   /* hardware error */
      if (asc_ascq == 0x4000)
      {
        DBG(DBG_sense, "-> diagnostic error:\n");
        if (len >= 0x13)
	{
	  DBG_sense_nz("   dim light\n",			get_RS_asb_dim_light(result));
	  DBG_sense_nz("   no light\n",				get_RS_asb_no_light(result));
	  DBG_sense_nz("   sensor or motor error\n",		get_RS_asb_sensor_motor(result));
	  DBG_sense_nz("   too light\n",			get_RS_asb_too_light(result));
	  DBG_sense_nz("   calibration error\n",		get_RS_asb_calibration(result));
	  DBG_sense_nz("   rom error\n",			get_RS_asb_rom(result));
	  DBG_sense_nz("   ram error\n",			get_RS_asb_ram(result));
	  DBG_sense_nz("   cpu error\n",			get_RS_asb_cpu(result));
	  DBG_sense_nz("   scsi error\n",			get_RS_asb_scsi(result));
	  DBG_sense_nz("   timer error\n",			get_RS_asb_timer(result));
	  DBG_sense_nz("   filter motor error\n",		get_RS_asb_filter_motor(result));
	  DBG_sense_nz("   dc adjust error\n",			get_RS_asb_dc_adjust(result));
	  DBG_sense_nz("   uta home sensor or motor error\n",	get_RS_asb_uta_sensor(result));
	}
      }
      else
      {
        DBG(DBG_sense, "-> unknown hardware error: asc=%d, ascq=%d\n", asc, ascq);
      }
      return SANE_STATUS_IO_ERROR;
     break;


    case 0x05:										  /* illegal request */
      if (asc_ascq == 0x2000)
      {
        DBG(DBG_sense, "-> invalid command operation code\n");
      }
      else if (asc_ascq == 0x2400)
      {
        DBG(DBG_sense, "-> illegal field in CDB\n");
       }
      else if (asc_ascq == 0x2500)
      {
        DBG(DBG_sense, "-> logical unit not supported\n");
      }
      else if (asc_ascq == 0x2600)
      {
        DBG(DBG_sense, "-> invalid field in parameter list\n");
      }
      else if (asc_ascq == 0x2c01)
      {
        DBG(DBG_sense, "-> too many windows specified\n");
      }
      else if (asc_ascq == 0x2c02)
      {
        DBG(DBG_sense, "-> invalid combination of windows specified\n");
      }
      else
      {
        DBG(DBG_sense, "-> illegal request: asc=%d, ascq=%d\n", asc, ascq);
      }

      if (len >= 0x11)
      {
        if (get_RS_SKSV(result) != 0)
        {
          if (get_RS_CD(result) == 0)
          {
            DBG(DBG_sense, "-> illegal parameter in CDB\n");
          }
          else
          {
            DBG(DBG_sense, "-> illegal parameter is in the data parameters sent during data out phase\n");
          }

          DBG(DBG_sense, "-> error detected in byte %d\n", get_RS_field_pointer(result));
         }
      }
      return SANE_STATUS_IO_ERROR;
     break;


    case 0x06:										   /* unit attention */
      if (asc_ascq == 0x2900)
      {
        DBG(DBG_sense, "-> power on, reset or bus device reset\n");
      }
      else if (asc_ascq == 0x3f01)
      {
        DBG(DBG_sense, "-> microcode has been changed\n");
       }
      else
      {
        DBG(DBG_sense, "-> unit attention: asc=%d, ascq=%d\n", asc, ascq);
      }
     break;


    case 0x09:										  /* vendor specific */

      if (asc == 0x00)
      {
        DBG(DBG_sense, "-> button protocol\n");
        if (ascq & 1)
        {
          dev->button0_pressed = 1;
          DBG(DBG_sense, "-> button 0 pressed\n");
        }

        if (ascq & 2)
        {
          dev->button1_pressed = 1;
          DBG(DBG_sense, "-> button 1 pressed\n");
        }

        if (ascq & 4)
        {
          dev->button2_pressed = 1;
          DBG(DBG_sense, "-> button 2 pressed\n");
        }
        return SANE_STATUS_GOOD;
      }
      else if (asc_ascq == 0x8001)
      {
        DBG(DBG_sense, "-> lamp warmup\n");
        return SANE_STATUS_DEVICE_BUSY;
      }
      else if (asc_ascq == 0x8002)
      {
        DBG(DBG_sense, "-> calibration by driver\n");
        if (dev)
	{
          dev->do_calibration = 1;				       /* set flag for calibration by driver */
        }
        return SANE_STATUS_GOOD;
      }
      else
      {
        DBG(DBG_sense, "-> vendor specific sense-code: asc=%d, ascq=%d\n", asc, ascq);
      }
     break;

  }
 return SANE_STATUS_GOOD;
}

/* ------------------------------------------------------------ UMAX SET RGB BIND -------------------------- */

static void umax_set_rgb_bind(Umax_Scanner *scanner)
{
  if ( (scanner->val[OPT_RGB_BIND].w == SANE_FALSE) &&
       (strcmp(scanner->val[OPT_MODE].s, COLOR_STR) == 0) ) /* enable rgb options */
  {
    if (scanner->device->inquiry_analog_gamma)
    {
      scanner->opt[OPT_ANALOG_GAMMA].cap   |= SANE_CAP_INACTIVE;
      scanner->opt[OPT_ANALOG_GAMMA_R].cap &= ~SANE_CAP_INACTIVE;
      scanner->opt[OPT_ANALOG_GAMMA_G].cap &= ~SANE_CAP_INACTIVE;
      scanner->opt[OPT_ANALOG_GAMMA_B].cap &= ~SANE_CAP_INACTIVE;
    }
    if (scanner->device->inquiry_highlight)
    {
      scanner->opt[OPT_HIGHLIGHT].cap   |= SANE_CAP_INACTIVE;
      scanner->opt[OPT_HIGHLIGHT_R].cap &= ~SANE_CAP_INACTIVE;
      scanner->opt[OPT_HIGHLIGHT_G].cap &= ~SANE_CAP_INACTIVE;
      scanner->opt[OPT_HIGHLIGHT_B].cap &= ~SANE_CAP_INACTIVE;
    }
    if (scanner->device->inquiry_shadow)
    {
      scanner->opt[OPT_SHADOW].cap   |= SANE_CAP_INACTIVE;
      scanner->opt[OPT_SHADOW_R].cap &= ~SANE_CAP_INACTIVE;
      scanner->opt[OPT_SHADOW_G].cap &= ~SANE_CAP_INACTIVE;
      scanner->opt[OPT_SHADOW_B].cap &= ~SANE_CAP_INACTIVE;
    }
  }
  else /* only show gray options */
  {
    if (scanner->device->inquiry_analog_gamma)
    {
      scanner->opt[OPT_ANALOG_GAMMA].cap   &= ~SANE_CAP_INACTIVE;
      scanner->opt[OPT_ANALOG_GAMMA_R].cap |= SANE_CAP_INACTIVE;
      scanner->opt[OPT_ANALOG_GAMMA_G].cap |= SANE_CAP_INACTIVE;
      scanner->opt[OPT_ANALOG_GAMMA_B].cap |= SANE_CAP_INACTIVE;
    }
    if (scanner->device->inquiry_highlight)
    {
      scanner->opt[OPT_HIGHLIGHT].cap   &= ~SANE_CAP_INACTIVE;
      scanner->opt[OPT_HIGHLIGHT_R].cap |= SANE_CAP_INACTIVE;
      scanner->opt[OPT_HIGHLIGHT_G].cap |= SANE_CAP_INACTIVE;
      scanner->opt[OPT_HIGHLIGHT_B].cap |= SANE_CAP_INACTIVE;
    }
    if (scanner->device->inquiry_shadow)
    {
      scanner->opt[OPT_SHADOW].cap   &= ~SANE_CAP_INACTIVE;
      scanner->opt[OPT_SHADOW_R].cap |= SANE_CAP_INACTIVE;
      scanner->opt[OPT_SHADOW_G].cap |= SANE_CAP_INACTIVE;
      scanner->opt[OPT_SHADOW_B].cap |= SANE_CAP_INACTIVE;
    }
  }

  if ( (scanner->device->inquiry_exposure_adj) && (scanner->val[OPT_SELECT_EXPOSURE_TIME].w) )
  {
    if ( (scanner->val[OPT_RGB_BIND].w == SANE_FALSE) &&
         (!scanner->device->exposure_time_rgb_bind) &&
         (strcmp(scanner->val[OPT_MODE].s, COLOR_STR) == 0) ) /* enable rgb exposure time options */
    {
      if (scanner->val[OPT_SELECT_CAL_EXPOSURE_TIME].w) /* exposure time setting for calibration enabled */
      {
        scanner->opt[OPT_CAL_EXPOS_TIME].cap    |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_CAL_EXPOS_TIME_R].cap  &= ~SANE_CAP_INACTIVE;
        scanner->opt[OPT_CAL_EXPOS_TIME_G].cap  &= ~SANE_CAP_INACTIVE;
        scanner->opt[OPT_CAL_EXPOS_TIME_B].cap  &= ~SANE_CAP_INACTIVE;
      }
      else /* no separate settings for calibration exposure times */
      {
        scanner->opt[OPT_CAL_EXPOS_TIME].cap    |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_CAL_EXPOS_TIME_R].cap  |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_CAL_EXPOS_TIME_G].cap  |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_CAL_EXPOS_TIME_B].cap  |= SANE_CAP_INACTIVE;
      }

      scanner->opt[OPT_SCAN_EXPOS_TIME].cap   |= SANE_CAP_INACTIVE;
      scanner->opt[OPT_SCAN_EXPOS_TIME_R].cap &= ~SANE_CAP_INACTIVE;
      scanner->opt[OPT_SCAN_EXPOS_TIME_G].cap &= ~SANE_CAP_INACTIVE;
      scanner->opt[OPT_SCAN_EXPOS_TIME_B].cap &= ~SANE_CAP_INACTIVE;
    }
    else /* enable gray exposure time options */
    {
      if (scanner->val[OPT_SELECT_CAL_EXPOSURE_TIME].w) /* exposure time setting for calibration enabled */
      {
        scanner->opt[OPT_CAL_EXPOS_TIME].cap  &= ~SANE_CAP_INACTIVE;
      }
      else /* no separate settings for calibration exposure times */
      {
        scanner->opt[OPT_CAL_EXPOS_TIME].cap  |= SANE_CAP_INACTIVE;
      }
      scanner->opt[OPT_CAL_EXPOS_TIME_R].cap  |= SANE_CAP_INACTIVE;
      scanner->opt[OPT_CAL_EXPOS_TIME_G].cap  |= SANE_CAP_INACTIVE;
      scanner->opt[OPT_CAL_EXPOS_TIME_B].cap  |= SANE_CAP_INACTIVE;

      scanner->opt[OPT_SCAN_EXPOS_TIME].cap   &= ~SANE_CAP_INACTIVE;
      scanner->opt[OPT_SCAN_EXPOS_TIME_R].cap |= SANE_CAP_INACTIVE;
      scanner->opt[OPT_SCAN_EXPOS_TIME_G].cap |= SANE_CAP_INACTIVE;
      scanner->opt[OPT_SCAN_EXPOS_TIME_B].cap |= SANE_CAP_INACTIVE;
    }
  }
}

/* ------------------------------------------------------------ UMAX CALCULATE PIXELS ---------------------- */

static int umax_calculate_pixels(int scansize_pixel, int resolution, int resolution_base, int coordinate_base)
/* scansize_pixel	= size in pixels at 1200 dpi */
/* resolution		= scan resolution */
/* resolution_base	= this is the optical resolution * 1 or * 2 */
/* coordinate_base	= this is 1200 dpi */
{
 unsigned int intsize_inch, intsize_pixel, diffsize_pixel, missing_pixels, del_pixel_1, pix;
 int toomuch;

  intsize_inch   = scansize_pixel / coordinate_base;	/* full inches */
  intsize_pixel  = intsize_inch * resolution;		/* pixels in full inches */

  diffsize_pixel  = scansize_pixel % coordinate_base;	/* missing pixels in last inch at 1200 dpi */
  missing_pixels  = diffsize_pixel * resolution_base / coordinate_base; /* missing pixels at resolution_base dpi */
  del_pixel_1     = resolution_base - resolution;	/* pixels to erase in one inch */
  toomuch         = 0;					/* number of pixels that must be deleted in last inch  */

  if (del_pixel_1 != 0)					/* search the number of pixels that must deleted */
  {
    pix = 0;
    while (pix <= missing_pixels)
    {
      toomuch++;
      pix = toomuch * resolution_base/del_pixel_1;
    }

    if (pix > missing_pixels)
    {
      toomuch--;
    }
  }

  return (intsize_pixel + missing_pixels - toomuch);
}

/* ------------------------------------------------------------ UMAX FORGET LINE --------------------------- */


static int umax_forget_line(Umax_Device *dev, int color)
/* tests if line related to optical resolution has to be skipped for selected resolution */
/* returns 0 if line is ok, -1 if line has to be skipped */
{
 unsigned int opt_res = dev->relevant_optical_res * dev->scale_y;
 unsigned int forget;

  dev->pixelline_opt_res++;					 /* increment number of lines in optical res */

  if (opt_res != dev->y_resolution)					    /* are there any lines to skip ? */
  {

    forget = (dev->pixelline_del[color] * opt_res)/(opt_res - dev->y_resolution);

    if (dev->pixelline_optic[color]++ == forget)
    {
      dev->pixelline_del[color]++;					 /* inc pointer to next line to skip */
      return(-1);										/* skip line */
    }
  }

 return(0);									      /* ok, order this line */
}


/* ------------------------------------------------------------ UMAX ORDER LINE TO PIXEL ------------------- */


static void umax_order_line_to_pixel(Umax_Device *dev, unsigned char *source, int color)
/* reads a one-color line and writes it into a pixel-ordered-buffer if line */
/* is not skipped */
/* color = 0:red, 1:green, 2:blue */
{
 unsigned int i;
 unsigned int line = dev->pixelline_next[color];					 /* bufferlinenumber */
 unsigned char *dest = dev->pixelbuffer;

  if (dest != NULL)
  {
    if (dev->bits_per_pixel_code == 1)								   /* 24 bpp */
    {
      dest += line * dev->width_in_pixels * 3 + color;

      for (i=0; i<dev->width_in_pixels; i++)				   /* cp each pixel into pixelbuffer */
      {
        *dest++ = *source++;
	dest++;
	dest++;
      }
    }
    else											 /* > 24 bpp */
    {
      dest += line * dev->width_in_pixels * 6 + color * 2;

      for(i=0; i<dev->width_in_pixels; i++)				   /* cp each pixel into pixelbuffer */
      {
        *dest++ = *source++;					      /* byte order correct ? , don't know ! */
        *dest++ = *source++;

        dest++; dest++;
        dest++; dest++;
      }
    }

    line++;
    if (line >= dev->pixelline_max)
    {
      line = 0;
    }

    dev->pixelline_next[color] = line;						  /* next line of this color */
    dev->pixelline_ready[color]++;					  /* number of ready lines for color */

    DBG(DBG_read, "merged line as color %d to line %d\n", color, dev->pixelline_ready[color]);
  }
}


/* ------------------------------------------------------------ UMAX ORDER LINE ---------------------------- */


static void umax_order_line(Umax_Device *dev, unsigned char *source)
{
 unsigned int CCD_distance = dev->CCD_distance * dev->scale_y;
 unsigned int length = (dev->scanlength * dev->scale_y * dev->relevant_optical_res) / dev->y_coordinate_base;
 unsigned int color;

  do										   /* search next valid line */
  {
    if (dev->pixelline_opt_res < CCD_distance)
    {
      color = dev->CCD_color[0];								  /* color 0 */
    }
    else if (dev->pixelline_opt_res < CCD_distance * 3)
    {
      color = dev->CCD_color[1 + ((dev->pixelline_opt_res - CCD_distance) % 2)];	 	/* color 1,2 */
    }
    else if (dev->pixelline_opt_res < length * 3 - CCD_distance * 3) 
    {
      color = dev->CCD_color[3 + (dev->pixelline_opt_res % 3)];				      /* color 3,4,5 */
    }
    else if (dev->pixelline_opt_res < length * 3 - CCD_distance) 
    {
      color = dev->CCD_color[6 + ((dev->pixelline_opt_res - length*3 + CCD_distance*3) % 2)];	/* color 6,7 */
    }
    else 
    {
      color = dev->CCD_color[8];								  /* color 8 */
    } 
  } while(umax_forget_line(dev, color) != 0);					 /* until found correct line */

  umax_order_line_to_pixel(dev, source, color);
}


/* ------------------------------------------------------------ UMAX GET PIXEL LINE ------------------------ */


static unsigned char * umax_get_pixel_line(Umax_Device *dev)
{
 unsigned char *source = NULL;

  if (dev->pixelbuffer != NULL)
  {
    if ( (dev->pixelline_ready[0] > dev->pixelline_written) &&
         (dev->pixelline_ready[1] > dev->pixelline_written) &&
         (dev->pixelline_ready[2] > dev->pixelline_written) )
    {
      source = dev->pixelbuffer + dev->pixelline_read * dev->width_in_pixels * 3;

      dev->pixelline_written++;
      dev->pixelline_read++;

      if (dev->pixelline_read >= dev->pixelline_max)
      {
        dev->pixelline_read = 0;
      }
    }
  }

  return source;
}


/* ============================================================ Switches between the SCSI and USB commands = */

/* ------------------------------------------------------------ UMAX SCSI CMD ------------------------------ */

static SANE_Status umax_scsi_cmd(Umax_Device *dev, const void *src, size_t src_size, void *dst, size_t * dst_size)
{
  switch (dev->connection_type)
  {
    case SANE_UMAX_SCSI:
      return sanei_scsi_cmd(dev->sfd, src, src_size, dst, dst_size);
     break;

#ifdef UMAX_ENABLE_USB
    case SANE_UMAX_USB:
      return sanei_umaxusb_cmd(dev->sfd, src, src_size, dst, dst_size);
     break;
#endif

    default:
      return(SANE_STATUS_INVAL);
  }
}

/* ------------------------------------------------------------ UMAX SCSI OPEN EXTENDED -------------------- */

static SANE_Status umax_scsi_open_extended(const char *devicename, Umax_Device *dev,
                                           SANEI_SCSI_Sense_Handler handler, void *handler_arg, int *buffersize)
{
  switch (dev->connection_type) 
  {
    case SANE_UMAX_SCSI:
      return sanei_scsi_open_extended(devicename, &dev->sfd, handler, handler_arg, buffersize);
     break;

#ifdef UMAX_ENABLE_USB
    case SANE_UMAX_USB:
      return sanei_umaxusb_open_extended(devicename, &dev->sfd, handler, handler_arg, buffersize);
     break;
#endif

    default:
      return(SANE_STATUS_INVAL);
  }
}

/* ------------------------------------------------------------ UMAX SCSI OPEN ----------------------------- */

static SANE_Status umax_scsi_open(const char *devicename, Umax_Device *dev,
                                  SANEI_SCSI_Sense_Handler handler, void *handler_arg)
{
  switch (dev->connection_type)
  {
    case SANE_UMAX_SCSI:
      return sanei_scsi_open(devicename, &dev->sfd, handler, handler_arg);
     break;

#ifdef UMAX_ENABLE_USB
    case SANE_UMAX_USB:
      return sanei_umaxusb_open(devicename, &dev->sfd, handler, handler_arg);
     break;
#endif

    default:
      return(SANE_STATUS_INVAL);
  }
}

/* ------------------------------------------------------------ UMAX SCSI CLOSE ---------------------------- */

static void umax_scsi_close(Umax_Device *dev)
{
  switch (dev->connection_type)
  {
    case SANE_UMAX_SCSI:
      sanei_scsi_close(dev->sfd);
      dev->sfd=-1;
     break;

#ifdef UMAX_ENABLE_USB
    case SANE_UMAX_USB:
      sanei_umaxusb_close(dev->sfd);
      dev->sfd=-1;
     break;
#endif
  }	
}

/* ------------------------------------------------------------ UMAX SCSI REQ ENTER ------------------------ */

static SANE_Status umax_scsi_req_enter(Umax_Device *dev, const void *src, size_t src_size,
                                       void *dst, size_t * dst_size, void **idp)
{
  switch (dev->connection_type)
  {
    case SANE_UMAX_SCSI:
      return sanei_scsi_req_enter (dev->sfd, src, src_size, dst, dst_size, idp);
     break;

#ifdef UMAX_ENABLE_USB
    case SANE_UMAX_USB:
      return sanei_umaxusb_req_enter (dev->sfd, src, src_size, dst, dst_size, idp);
     break;
#endif

    default:
      return(SANE_STATUS_INVAL);
  }
}

/* ------------------------------------------------------------ UMAX SCSI REQ WAIT ------------------------- */

static SANE_Status umax_scsi_req_wait(Umax_Device *dev, void *id)
{
  switch (dev->connection_type)
  {
    case SANE_UMAX_SCSI:
      return sanei_scsi_req_wait(id);
     break;

#ifdef UMAX_ENABLE_USB
    case SANE_UMAX_USB:
      return sanei_umaxusb_req_wait(id);
     break;
#endif

    default:
      return(SANE_STATUS_INVAL);
  }
}

/* ------------------------------------------------------------ UMAX SCSI GET LAMP STATUS ------------------ */


static SANE_Status umax_scsi_get_lamp_status(Umax_Device *dev, int *lamp_on)
{
 SANE_Status status;
 size_t size = 1;

  DBG(DBG_proc, "umax_scsi_get_lamp_status\n");

  status = umax_scsi_cmd(dev, get_lamp_status.cmd, get_lamp_status.size, dev->buffer[0], &size);
  if (status)
  {
    DBG(DBG_error, "umax_scsi_get_lamp_status: command returned status %s\n", sane_strstatus(status));
   return status;
  }

  *lamp_on = get_lamp_status_lamp_on(dev->buffer[0]);

  DBG(DBG_info, "lamp_status = %d\n", *lamp_on);

 return status;
}

/* ------------------------------------------------------------ UMAX SCSI SET LAMP STATUS ------------------ */


static SANE_Status umax_scsi_set_lamp_status(Umax_Device *dev, int lamp_on)
{
 SANE_Status status;

  DBG(DBG_proc, "umax_scsi_set_lamp_status\n");
  DBG(DBG_info, "lamp_status=%d\n", lamp_on);

  set_lamp_status_lamp_on(set_lamp_status.cmd, lamp_on);
  status = umax_scsi_cmd(dev, set_lamp_status.cmd, set_lamp_status.size, NULL, NULL);

  if (status)
  {
    DBG(DBG_error, "umax_scsi_set_lamp_status: command returned status %s\n", sane_strstatus(status));
  }

 return status;
}

/* ------------------------------------------------------------ UMAX SET LAMP STATUS ----------------------- */

static SANE_Status umax_set_lamp_status(SANE_Handle handle, int lamp_on)
{
 Umax_Scanner *scanner = handle;
 int lamp_status;
 SANE_Status status;

  DBG(DBG_proc, "umax_set_lamp_status\n");

  if (umax_scsi_open(scanner->device->sane.name, scanner->device, sense_handler,
                     scanner->device) != SANE_STATUS_GOOD )
  {
     DBG(DBG_error, "ERROR: umax_set_lamp_status: open of %s failed:\n", scanner->device->sane.name);
     return SANE_STATUS_INVAL;
  }

  status = umax_scsi_get_lamp_status(scanner->device, &lamp_status);

  if (!status)
  {
    status = umax_scsi_set_lamp_status(scanner->device, lamp_on);
  }

  umax_scsi_close(scanner->device);

 return status;
}

/* ------------------------------------------------------------ UMAX GET DATA BUFFER STATUS ---------------- */


#ifndef UMAX_HIDE_UNUSED									 /* NOT USED */
static SANE_Status umax_get_data_buffer_status(Umax_Device *dev)
{
 SANE_Status status;

  DBG(DBG_proc, "get_data_buffer_status\n");
  set_GDBS_wait(get_data_buffer_status.cmd,1);					    /* wait for scanned data */
  status = umax_scsi_cmd(dev, get_data_buffer_status.cmd, get_data_buffer_status.size, NULL, NULL);
  if (status)
  {
    DBG(DBG_error, "umax_get_data_buffer_status: command returned status %s\n", sane_strstatus(status));
  }  

 return status;
}
#endif


/* ------------------------------------------------------------ UMAX DO REQUEST SENSE ---------------------- */


static void umax_do_request_sense(Umax_Device *dev)
{
 size_t size = rs_return_block_size;
 SANE_Status status;

  DBG(DBG_proc, "do_request_sense\n");
  set_RS_allocation_length(request_sense.cmd, rs_return_block_size); 
  status = umax_scsi_cmd(dev, request_sense.cmd, request_sense.size, dev->buffer[0], &size);
  if (status)
  {
    DBG(DBG_error, "umax_do_request_sense: command returned status %s\n", sane_strstatus(status));
  }  
}


/* ------------------------------------------------------------ UMAX WAIT SCANNER -------------------------- */


static SANE_Status umax_wait_scanner(Umax_Device *dev)
{
 SANE_Status status;
 int cnt = 0;

  DBG(DBG_proc, "wait_scanner\n");

  do
  {
    if (cnt > 100)							   /* maximal 100 * 0.5 sec = 50 sec */
    {
      DBG(DBG_warning, "scanner does not get ready\n");
      return -1;
    }
											  /* test unit ready */
    status = umax_scsi_cmd(dev, test_unit_ready.cmd, test_unit_ready.size, NULL, NULL);
    cnt++;

    if (status)
    {
      if (cnt == 1)
      {
        DBG(DBG_info2, "scanner reports %s, waiting ...\n", sane_strstatus(status));
      }

      usleep(500000);									 /* wait 0.5 seconds */
    }
  } while (status != SANE_STATUS_GOOD );

  DBG(DBG_info, "scanner ready\n");

  return status;
}

#define WAIT_SCANNER { int status = umax_wait_scanner(dev); if (status) return status; }


/* ------------------------------------------------------------ UMAX GRAB SCANNER -------------------------- */


static int umax_grab_scanner(Umax_Device *dev)
{
 int status;

  DBG(DBG_proc, "grab_scanner\n");

  WAIT_SCANNER;									   /* wait for scanner ready */
  status = umax_scsi_cmd(dev, reserve_unit.cmd, reserve_unit.size, NULL, NULL);

  if (status)
  {
    DBG(DBG_error, "umax_grab_scanner: command returned status %s\n", sane_strstatus(status));
  }  
  else
  {
    DBG(DBG_info, "scanner reserved\n");
  }

  return status;
}


/* ------------------------------------------------------------ UMAX REPOSITION SCANNER -------------------- */


static int umax_reposition_scanner(Umax_Device *dev)
{
 int status;
 int pause;

  pause = dev->pause_after_reposition + dev->pause_for_moving * (dev->upper_left_y + dev->scanlength) / 
                                        ( (dev->inquiry_fb_length * dev->y_coordinate_base) );

  DBG(DBG_info2, "trying to reposition scanner ...\n");
  status = umax_scsi_cmd(dev, object_position.cmd, object_position.size, NULL, NULL);
  if (status)
  {
    DBG(DBG_error, "umax_reposition_scanner: command returned status %s\n", sane_strstatus(status));
    return status;
  }

  if (pause > 0) /* predefined time to wait (Astra 2400S) */
  {
    DBG(DBG_info2, "pause for repositioning %d msec ...\n", pause);
    usleep(((long) pause) * 1000);
    DBG(DBG_info, "repositioning pause done\n");
  }
  else if (pause == 0) /* use TEST UNIT READY */
  {
    WAIT_SCANNER;
    DBG(DBG_info, "scanner repositioned\n");
  }
  else /* pause < 0 : return without any pause */
  {
    DBG(DBG_info, "not waiting for finishing reposition scanner\n");
  }

  return SANE_STATUS_GOOD;
}


/* ------------------------------------------------------------ UMAX GIVE SCANNER -------------------------- */


static int umax_give_scanner(Umax_Device *dev)
{
 int status;

  DBG(DBG_info2, "trying to release scanner ...\n");
  status = umax_scsi_cmd(dev, release_unit.cmd, release_unit.size, NULL, NULL);
  if (status)
  {
    DBG(DBG_error, "umax_give_scanner: command returned status %s\n", sane_strstatus(status));
  }  
  else
  {
    DBG(DBG_info, "scanner released\n");
  }

  if (!dev->batch_scan || dev->batch_end)
  {
    umax_reposition_scanner(dev);
  }
  else
  {
    usleep(200000); /* 200 ms pause to make sure program does not exit before scanner is ready */
  }

  return status;
}


/* ------------------------------------------------------------ UMAX SEND GAMMA DATA ----------------------- */


static void umax_send_gamma_data(Umax_Device *dev, void *gamma_data, int color)
{
 unsigned char *data = gamma_data;
 unsigned char *dest;
 int length;
 SANE_Status status;

  DBG(DBG_proc, "send_gamma_data\n");

  if (dev->inquiry_gamma_dwload == 0)
  {
    DBG(DBG_error, "ERROR: gamma download not available\n");
    return;
  }
  
  memcpy(dev->buffer[0], send.cmd, send.size);							     /* send */
  set_S_datatype_code(dev->buffer[0], S_datatype_gamma);					      /* gamma curve */

  dest = dev->buffer[0] + send.size;

  if (dev->inquiry_gamma_DCF == 0)						      /* gamma format type 0 */
  {
    DBG(DBG_info, "using gamma download curve format type 0\n");

    memcpy(dest, gamma_DCF0.cmd, gamma_DCF0.size);

    if (color == 1)										/* one color */
    {
      set_DCF0_gamma_lines(dest, DCF0_gamma_one_line);

      set_DCF0_gamma_color(dest, 0, DCF0_gamma_color_gray);				        /* grayscale */
      if ( (dev->colormode == RGB) && (dev->three_pass != 0) )				     /* 3 pass color */
      {
        set_DCF0_gamma_color(dest, 0,  dev->three_pass_color);					/* set color */
      }

      dest = dest + 2;
      memcpy(dest, data, 1024);								/* copy data */

      set_S_xfer_length(dev->buffer[0], 1026);						       /* set length */
      status = umax_scsi_cmd(dev, dev->buffer[0], send.size + 1026, NULL, NULL);
      if (status)
      {
        DBG(DBG_error, "umax_send_gamma_data(DCF=0, one color): command returned status %s\n", sane_strstatus(status));
      }  
    }
    else										     /* three colors */
    {
      set_DCF0_gamma_lines(dest, DCF0_gamma_three_lines);

      set_DCF0_gamma_color(dest, 0, DCF0_gamma_color_red);					      /* red */
      set_DCF0_gamma_color(dest, 1, DCF0_gamma_color_green);					    /* green */
      set_DCF0_gamma_color(dest, 2, DCF0_gamma_color_blue);					     /* blue */

      dest = dest + 2;
      memcpy(dest, data, 1024);								    /* copy red data */

      dest = dest + 1025;
      data = data + 1024;
      memcpy(dest, data, 1024);								  /* copy green data */

      dest = dest + 1025;
      data = data + 1024;
      memcpy(dest, data, 1024);								   /* copy blue data */

      set_S_xfer_length(dev->buffer[0], 3076);						       /* set length */
      status = umax_scsi_cmd(dev, dev->buffer[0], send.size + 3076, NULL, NULL);
      if (status)
      {
        DBG(DBG_error, "umax_send_gamma_data(DCF=0, RGB): command returned status %s\n", sane_strstatus(status));
      }  
    }
  }
  else if (dev->inquiry_gamma_DCF == 1)						      /* gamma format type 1 */
  {
    DBG(DBG_info, "using gamma download curve format type 1\n");

    memcpy(dest, gamma_DCF1.cmd, gamma_DCF1.size);

    set_DCF1_gamma_color(dest, DCF1_gamma_color_gray);					        /* grayscale */
    if ( (dev->colormode == RGB) && (dev->three_pass != 0) )				     /* 3 pass color */
    {
      set_DCF1_gamma_color(dest,  dev->three_pass_color);					/* set color */
    }

    dest = dest + 2;
    memcpy(dest, data, 256);									/* copy data */

    set_S_xfer_length(dev->buffer[0], 258);						       /* set length */
    status = umax_scsi_cmd(dev, dev->buffer[0], send.size + 258, NULL, NULL);
    if (status)
    {
      DBG(DBG_error, "umax_send_gamma_data(DCF=1): command returned status %s\n", sane_strstatus(status));
    }  
  }
  else if (dev->inquiry_gamma_DCF == 2)						      /* gamma format type 2 */
  {
    DBG(DBG_info, "using gamma download curve format type 2\n");

    memcpy(dest, gamma_DCF2.cmd, gamma_DCF2.size);

    set_DCF2_gamma_color(dest, DCF2_gamma_color_gray);					        /* grayscale */
    if ( (dev->colormode == RGB) && (dev->three_pass != 0) )				     /* 3 pass color */
    { set_DCF2_gamma_color(dest, dev->three_pass_color); }				        /* set color */

    if (color == 1)
    {
      set_DCF2_gamma_lines(dest, DCF2_gamma_one_line);
    }
    else
    {
      set_DCF2_gamma_lines(dest, DCF2_gamma_three_lines);
    }

    set_DCF2_gamma_input_bits(dest, dev->gamma_input_bits_code);
    set_DCF2_gamma_output_bits(dest, dev->bits_per_pixel_code);

    dest = dev->buffer[0] + send.size + gamma_DCF2.size;					    /* write to dest */

    if (dev->gamma_input_bits_code & 32)
    {
      length = 65536; /* 16 input bits */
    }
    else if (dev->gamma_input_bits_code & 16)
    {
      length = 16384; /* 14 input bits */
    }
    else if (dev->gamma_input_bits_code & 8)
    {
      length = 4096; /* 12 input bits */
    }
    else if (dev->gamma_input_bits_code & 4)
    {
      length = 1024; /* 10 input bits */
    }
    else if (dev->gamma_input_bits_code & 2)
    {
      length = 512; /* 9 input bits */
    }
    else
    {
      length = 256; /* 8 input bits */
    }

    if (dev->bits_per_pixel_code != 1)					/* more than 8 output bits per pixel */
    {
      length = length * 2; /* = 2 output bytes */
    }

    if (dev->bufsize >= color*length+gamma_DCF2.size)
    {
      set_S_xfer_length(dev->buffer[0], color*length+gamma_DCF2.size);			       /* set length */
      memcpy(dest, data, color*length);								/* copy data */

      status = umax_scsi_cmd(dev, dev->buffer[0], send.size+gamma_DCF2.size + length * color, NULL, NULL);
      if (status)
      {
        DBG(DBG_error, "umax_send_gamma_data(DCF=2): command returned status %s\n", sane_strstatus(status));
      }  
    }
    else
    {
      DBG(DBG_error, "ERROR: too small scsi buffer (%d bytes) to send gamma data\n", dev->bufsize);
    }
  }
  else
  {
    DBG(DBG_error, "ERROR: unknown gamma download curve type for this scanner\n");
  }
}


/* ------------------------------------------------------------ UMAX SEND DATA  ---------------------------- */


static void umax_send_data(Umax_Device *dev, void *data, int size, int datatype)
{
 unsigned char *dest;
 SANE_Status status;

  memcpy(dev->buffer[0], send.cmd, send.size);							     /* send */
  set_S_datatype_code(dev->buffer[0], datatype);						     /* set datatype */
  set_S_xfer_length(dev->buffer[0], size);								    /* bytes */

  dest=dev->buffer[0] + send.size;
  memcpy(dest, data, size);									/* copy data */

  status = umax_scsi_cmd(dev, dev->buffer[0], send.size + size, NULL, NULL);
  if (status)
  {
    DBG(DBG_error, "umax_send_data: command returned status %s\n", sane_strstatus(status));
  }  
}


/* ------------------------------------------------------------ UMAX SEND HALFTONE PATTERN ----------------- */


#ifndef UMAX_HIDE_UNUSED
static void umax_send_halftone_pattern(Umax_Device *dev, void *data, int size)
{
  DBG(DBG_proc,"send_halftone_pattern\n");
  umax_send_data(dev, data, size*size, S_datatype_halftone);
}
#endif


/* ------------------------------------------------------------ UMAX SEND SHADING DATA  -------------------- */


static void umax_send_shading_data(Umax_Device *dev, void *data, int size)
{
  DBG(DBG_proc,"send_shading_data\n");
  umax_send_data(dev, data, size, S_datatype_shading);
}


/* ------------------------------------------------------------ UMAX SEND GAIN DATA  ----------------------- */

#ifndef UMAX_HIDE_UNUSED
static void umax_send_gain_data(Umax_Device *dev, void *data, int size)
{
  DBG(DBG_proc,"send_gain_data\n");
  umax_send_data(dev, data, size, S_datatype_gain);
}
#endif


/* ------------------------------------------------------------ UMAX QUEUE READ IMAGE DATA REQ ------------- */

static SANE_Status umax_queue_read_image_data_req(Umax_Device *dev, unsigned int length, int bufnr)
{
 SANE_Status status;

  DBG(DBG_proc, "umax_queue_read_image_data_req for buffer[%d], length = %d\n", bufnr, length);

  set_R_xfer_length(sread.cmd, length);							       /* set length */
  set_R_datatype_code(sread.cmd, R_datatype_imagedata);					     /* set datatype */

  dev->length_queued[bufnr] = length; /* set length request */
  dev->length_read[bufnr]   = length; /* set length request, can be changed asyncronous by umax_scsi_req_enter */

  status = umax_scsi_req_enter(dev, sread.cmd, sread.size, dev->buffer[bufnr], &(dev->length_read[bufnr]), &(dev->queue_id[bufnr]));
  if (status)
  {
    DBG(DBG_error, "umax_queue_read_image_data_req: command returned status %s\n", sane_strstatus(status));
    return -1;
  }
  else
  {
    DBG(DBG_info2, "umax_queue_read_image_data_req: id for buffer[%d] is %p\n", bufnr, dev->queue_id[bufnr]);
  }

  return length;
}

/* ------------------------------------------------------------ UMAX WAIT QUEUED IMAGE DATA ---------------- */


static int umax_wait_queued_image_data(Umax_Device *dev, int bufnr)
{
 SANE_Status status;

  DBG(DBG_proc, "umax_wait_queued_image_data for buffer[%d] (id=%p)\n", bufnr, dev->queue_id[bufnr]);

  status = umax_scsi_req_wait(dev, dev->queue_id[bufnr]);
  if (status)
  {
    DBG(DBG_error, "umax_wait_queued_image_data: wait returned status %s\n", sane_strstatus(status));
    return -1;
  }

 return SANE_STATUS_GOOD;
}


/* ------------------------------------------------------------ UMAX READ DATA ----------------------------- */


static int umax_read_data(Umax_Device *dev, size_t length, int datatype)
{
 SANE_Status status;

  set_R_xfer_length(sread.cmd, length);							       /* set length */
  set_R_datatype_code(sread.cmd, datatype);						     /* set datatype */

  status = umax_scsi_cmd(dev, sread.cmd, sread.size, dev->buffer[0], &length);
  if (status)
  {
    DBG(DBG_error, "umax_read_data: command returned status %s\n", sane_strstatus(status));
    return -1;
  }  

 return length;
}


/* ------------------------------------------------------------ UMAX READ SHADING DATA  -------------------- */


static int umax_read_shading_data(Umax_Device *dev, unsigned int length)
{
  DBG(DBG_proc,"read_shading_data\n");
  return umax_read_data(dev, length, R_datatype_shading);
}


/* ------------------------------------------------------------ UMAX READ GAIN DATA  ----------------------- */


#ifndef UMAX_HIDE_UNUSED
static int umax_read_gain_data(Umax_Device *dev, unsigned int length)
{
  DBG(DBG_proc,"read_gain_data\n");
  return umax_read_data(dev, length, R_datatype_gain);
}
#endif


/* ------------------------------------------------------------ UMAX READ IMAGE DATA  ---------------------- */


#ifndef UMAX_HIDE_UNUSED
static int umax_read_image_data(Umax_Device *dev, unsigned int length)
{
  DBG(DBG_proc,"read_image_data\n");
  WAIT_SCANNER;
  return umax_read_data(dev, length, R_datatype_imagedata);
}
#endif


/* ------------------------------------------------------------ UMAX CORRECT LIGHT ------------------------- */


static int umax_correct_light(int light, int analog_gamma_byte)  /* correct highlight/shadow if analog gamma is set */
{ 
  double analog_gamma;
  analog_gamma=analog_gamma_table[analog_gamma_byte];
  return( (int) 255 * pow(  ((double) light)/255.0 , (1.0/analog_gamma) )+.5 );
}


/* ------------------------------------------------------------ UMAX SET WINDOW PARAM ---------------------- */


/* set_window_param sets all the window parameters. This means building a */
/* fairly complicated SCSI command before sending it...  */

static SANE_Status umax_set_window_param(Umax_Device *dev)
{
 SANE_Status status;
 int num_dblocks = 1;		 		       /* number of window descriptor blocks, usually 1 or 3 */
 unsigned char buffer_r[max_WDB_size], buffer_g[max_WDB_size], buffer_b[max_WDB_size];

  DBG(DBG_proc, "set_window_param\n");
  memset(buffer_r, '\0', max_WDB_size);							     /* clear buffer */
  set_WDB_length(dev->wdb_len);						   /* length of win descriptor block */
  memcpy(buffer_r, window_descriptor_block.cmd, window_descriptor_block.size);		 /* copy preset data */

  set_WD_wid(buffer_r, 0);								/* window identifier */
  set_WD_auto(buffer_r, dev->set_auto);					    /* 0 or 1: don't know what it is */

												  /* geometry */
  set_WD_Xres(buffer_r, dev->x_resolution);					      /* x resolution in dpi */
  set_WD_Yres(buffer_r, dev->y_resolution);					      /* y resolution in dpi */
  set_WD_ULX(buffer_r, dev->upper_left_x);						      /* left_edge x */
  set_WD_ULY(buffer_r, dev->upper_left_y);						     /* upper_edge y */
  set_WD_width(buffer_r, dev->scanwidth);							    /* width */
  set_WD_length(buffer_r, dev->scanlength);							   /* length */

												       /* BTC */
  set_WD_brightness(buffer_r, dev->brightness);					/* brightness, only halftone */
  set_WD_threshold(buffer_r, dev->threshold);					  /* threshold, only lineart */
  set_WD_contrast(buffer_r, dev->contrast);					  /* contrast, only halftone */
    
									       /* scanmode, preset to LINEART */
  set_WD_composition(buffer_r, WD_comp_lineart);					/* image composition */
											     /* = (scan-mode) */
  set_WD_bitsperpixel(buffer_r, WD_bits_1);				   /* bits/pixel (1,8,9,10,12,14,16) */
  set_WD_halftone(buffer_r, dev->halftone);					  /* select halftone-pattern */
  set_WD_RIF(buffer_r, dev->reverse);					  /* reverse, invert black and white */
  set_WD_speed(buffer_r, dev->WD_speed);						        /* set speed */
  set_WD_select_color(buffer_r, WD_color_gray);					   /* color for window-block */

						   /* set highlight and shadow in dependence of analog gamma */
  set_WD_highlight(buffer_r, umax_correct_light(dev->highlight_r, dev->analog_gamma_r));
  set_WD_shadow(buffer_r, umax_correct_light(dev->shadow_r, dev->analog_gamma_r));

											      /* scan options */
  set_WD_gamma(buffer_r, dev->digital_gamma_r);						/* set digital gamma */ 
  set_WD_module(buffer_r, dev->module);						  /* flatbed or transparency */ 
  set_WD_CBHS(buffer_r, dev->cbhs_range);							/* 50 or 255 */ 
  set_WD_FF(buffer_r, dev->fix_focus_position);					       /* fix focus position */
  set_WD_RMIF(buffer_r, dev->reverse_multi);					     /* reverse color-values */
  set_WD_FDC(buffer_r, dev->lens_cal_in_doc_pos);		    /* lens calibration in document position */
  set_WD_PF(buffer_r, dev->disable_pre_focus);						/* disable pre focus */
  set_WD_LCL(buffer_r, dev->holder_focus_pos_0mm);		    /* 0.6mm <-> 0.0mm holder focus position */
  set_WD_HBT(buffer_r, dev->low_byte_first);			       /* set byte order for 16 bit scanners */
  set_WD_DOR(buffer_r, dev->dor);						   /* double-resolution-mode */ 
  set_WD_scan_exposure_level(buffer_r, dev->exposure_time_scan_r);		       /* scan exposure time */
  set_WD_calibration_exposure_level(buffer_r, dev->exposure_time_calibration_r);/* calibration exposure time */

  set_WD_batch(buffer_r, dev->batch_scan);					     /* batch or normal scan */
  set_WD_MF(buffer_r, dev->manual_focus);				       /* automatic <-> manual focus */
  set_WD_line_arrangement(buffer_r, WD_line_arrengement_by_fw);		      /* line arrangement by scanner */
  set_WD_warmup(buffer_r, dev->warmup);								   /* warmup */

  set_WD_calibration(buffer_r, dev->calibration); 				        /* image calibration */

  set_WD_color_sequence(buffer_r, WD_color_sequence_RGB);				     /* sequence RGB */
  set_WD_color_ordering(buffer_r, WD_color_ordering_pixel);	      /* set to pixel for pbm, pgm, pnm-file */
  set_WD_analog_gamma(buffer_r, dev->analog_gamma_r );					     /* analog gamma */
  set_WD_lamp_c_density(buffer_r, dev->c_density);				   /* calibrat. lamp density */
  set_WD_lamp_s_density(buffer_r, dev->s_density);					/* scan lamp density */
  set_WD_next_upper_left(buffer_r, dev->batch_next_tl_y);	      /* batch scan next top left y position */
  set_WD_pixel_count(buffer_r, dev->width_in_pixels);					      /* pixel count */
  set_WD_line_count(buffer_r, dev->length_in_pixels);					       /* line count */
  set_WD_x_coordinate_base(buffer_r, dev->x_coordinate_base);				       /* dpi (1200) */
  set_WD_y_coordinate_base(buffer_r, dev->y_coordinate_base);				       /* dpi (1200) */
  set_WD_calibration_data_lines(buffer_r, dev->calib_lines);     /* required lines for calibration by driver */


  switch(dev->colormode)
  {
     case LINEART:										   /* LINEART */
      set_WD_composition(buffer_r, WD_comp_lineart);
      set_WD_bitsperpixel(buffer_r, WD_bits_1);

      set_WD_select_color(buffer_r, WD_color_gray);
     break;

     case HALFTONE:										  /* HALFTONE */
      set_WD_composition(buffer_r, WD_comp_dithered);
      set_WD_bitsperpixel(buffer_r, WD_bits_1);

      set_WD_select_color(buffer_r, WD_color_gray);
     break;

     case GRAYSCALE:										 /* GRAYSCALE */
      set_WD_composition(buffer_r, WD_comp_gray);
      set_WD_bitsperpixel(buffer_r, dev->bits_per_pixel);

      set_WD_select_color(buffer_r, WD_color_gray);
     break;

     case RGB_LINEART:								              /* COLOR MODES */
     case RGB_HALFTONE:
     case RGB:
      if (dev->colormode == RGB_LINEART )
      {
        set_WD_composition(buffer_r, WD_comp_rgb_bilevel);
        set_WD_bitsperpixel(buffer_r, WD_bits_1);
      }
      else if (dev->colormode == RGB_HALFTONE )
      {
        set_WD_composition(buffer_r, WD_comp_rgb_dithered);
        set_WD_bitsperpixel(buffer_r, WD_bits_1);
      }
      else /* RGB */
      {
        set_WD_composition(buffer_r, WD_comp_rgb_full);
        set_WD_bitsperpixel(buffer_r, dev->bits_per_pixel);
      }

      if (dev->three_pass == 0)
      {											       /* singlepass */
        num_dblocks = 3;

        if (dev->do_color_ordering != 0)
	{
          set_WD_line_arrangement(buffer_r, WD_line_arrengement_by_driver); 

	  if (dev->CCD_distance == 0)
	  {
            set_WD_color_ordering(buffer_r, WD_color_ordering_line_no_ccd);
          }
	  else
	  {
            set_WD_color_ordering(buffer_r, WD_color_ordering_line_w_ccd);
          }
        }

        memcpy(buffer_g, buffer_r, max_WDB_size);				       /* copy WDB for green */
        memcpy(buffer_b, buffer_r, max_WDB_size);					/* copy WDB for blue */

        set_WD_wid(buffer_r, WD_wid_red);					    /* window identifier red */
        set_WD_wid(buffer_g, WD_wid_green);					  /* window identifier green */
        set_WD_wid(buffer_b, WD_wid_blue);					   /* window identifier blue */

        set_WD_select_color(buffer_r, WD_color_red);			       /* select red for this window */
        set_WD_select_color(buffer_g, WD_color_green);			     /* select green for this window */
        set_WD_select_color(buffer_b, WD_color_blue);			      /* select blue for this window */

        set_WD_gamma(buffer_r, dev->digital_gamma_r);					    /* digital gamma */
        set_WD_gamma(buffer_g, dev->digital_gamma_g);
        set_WD_gamma(buffer_b, dev->digital_gamma_b);

        set_WD_analog_gamma(buffer_r, dev->analog_gamma_r);				     /* analog gamma */
        set_WD_analog_gamma(buffer_g, dev->analog_gamma_g);
        set_WD_analog_gamma(buffer_b, dev->analog_gamma_b);

							      /* set highlight in dependence of analog gamma */ 
        set_WD_highlight(buffer_r, umax_correct_light(dev->highlight_r, dev->analog_gamma_r));
        set_WD_highlight(buffer_g, umax_correct_light(dev->highlight_g, dev->analog_gamma_g));
        set_WD_highlight(buffer_b, umax_correct_light(dev->highlight_b, dev->analog_gamma_b));

								 /* set shadow in dependence of analog gamma */ 
        set_WD_shadow(buffer_r, umax_correct_light(dev->shadow_r, dev->analog_gamma_r));
        set_WD_shadow(buffer_g, umax_correct_light(dev->shadow_g, dev->analog_gamma_g));
        set_WD_shadow(buffer_b, umax_correct_light(dev->shadow_b, dev->analog_gamma_b));

        set_WD_scan_exposure_level(buffer_r, dev->exposure_time_scan_r);	  /* set scan exposure times */ 
        set_WD_scan_exposure_level(buffer_g, dev->exposure_time_scan_g);
        set_WD_scan_exposure_level(buffer_b, dev->exposure_time_scan_b);

        set_WD_calibration_exposure_level(buffer_r, dev->exposure_time_calibration_r);/* set calib exp times */
        set_WD_calibration_exposure_level(buffer_g, dev->exposure_time_calibration_g);
        set_WD_calibration_exposure_level(buffer_b, dev->exposure_time_calibration_b);
      }
      else
      {												/* threepass */
        set_WD_wid(buffer_r, 0);							/* window identifier */
        set_WD_color_ordering(buffer_r, WD_color_ordering_plane);				     /* ???? */

        if (dev->colormode == RGB_LINEART )
        {
          set_WD_composition(buffer_r, WD_comp_lineart);			       /* color-lineart-mode */
        }
        else if (dev->colormode == RGB_HALFTONE )
        {
          set_WD_composition(buffer_r, WD_comp_dithered);			      /* color-halftone-mode */
        }
        else /* RGB */
        {
          set_WD_composition(buffer_r, WD_comp_gray);					       /* color-mode */
        }

        switch (dev->three_pass_color)
        {
        case WD_wid_red:
           set_WD_select_color(buffer_r, WD_color_red);					        /* color red */
           set_WD_gamma(buffer_r, dev->digital_gamma_r);
           set_WD_analog_gamma(buffer_r, dev->analog_gamma_r);
           set_WD_highlight(buffer_r, umax_correct_light(dev->highlight_r, dev->analog_gamma_r));
           set_WD_shadow(buffer_r, umax_correct_light(dev->shadow_r, dev->analog_gamma_r)); 
           set_WD_scan_exposure_level(buffer_r, dev->exposure_time_scan_r);
           set_WD_calibration_exposure_level(buffer_r, dev->exposure_time_calibration_r);
           break;

        case WD_wid_green:
           set_WD_select_color(buffer_r, WD_color_green);				      /* color green */
           set_WD_gamma(buffer_r, dev->digital_gamma_g);
           set_WD_analog_gamma(buffer_r, dev->analog_gamma_g);
           set_WD_highlight(buffer_r, umax_correct_light(dev->highlight_g, dev->analog_gamma_g));
           set_WD_shadow(buffer_r, umax_correct_light(dev->shadow_g, dev->analog_gamma_g));
           set_WD_scan_exposure_level(buffer_r, dev->exposure_time_scan_g);
           set_WD_calibration_exposure_level(buffer_r, dev->exposure_time_calibration_g);
           break;

        case WD_wid_blue:
           set_WD_select_color(buffer_r, WD_color_blue);				       /* color blue */
           set_WD_gamma(buffer_r, dev->digital_gamma_b);
           set_WD_analog_gamma(buffer_r, dev->analog_gamma_b);
           set_WD_highlight(buffer_r, umax_correct_light(dev->highlight_b, dev->analog_gamma_b));
           set_WD_shadow(buffer_r, umax_correct_light(dev->shadow_b, dev->analog_gamma_b));
           set_WD_scan_exposure_level(buffer_r, dev->exposure_time_scan_b);
           set_WD_calibration_exposure_level(buffer_r, dev->exposure_time_calibration_b);
           break;

        } /* switch dev->three_pass_color */

      } /* if (single_pass) else (three_pass) */
     break;
  } /* switch dev->colormode, case RGB */

										       /* prepare SCSI-BUFFER */
  memcpy(dev->buffer[0], set_window.cmd, set_window.size);					   /* SET-WINDOW cmd */
  memcpy(WPDB_OFF(dev->buffer[0]), window_parameter_data_block.cmd, window_parameter_data_block.size);   /* WPDB */
  set_WPDB_wdbnum(WPDB_OFF(dev->buffer[0]), num_dblocks);					       /* set WD_len */
  memcpy(WDB_OFF(dev->buffer[0],1), buffer_r, window_descriptor_block.size);		     /* add WD_block */

  if ( num_dblocks == 3)								/* if singelpass RGB */
  {
     memcpy(WDB_OFF(dev->buffer[0],2), buffer_g, window_descriptor_block.size);			/* add green */
     memcpy(WDB_OFF(dev->buffer[0],3), buffer_b, window_descriptor_block.size);			 /* add blue */
  }


  DBG(DBG_info2, "window descriptor block created with %d bytes\n", dev->wdb_len);

  set_SW_xferlen(dev->buffer[0], (window_parameter_data_block.size + (window_descriptor_block.size * num_dblocks)));

  status = umax_scsi_cmd(dev, dev->buffer[0], set_window.size + window_parameter_data_block.size +
                                              (window_descriptor_block.size * num_dblocks), NULL, NULL);
  if (status)
  {
    DBG(DBG_error, "umax_set_window_param: command returned status %s\n", sane_strstatus(status));
  }  
  else
  {
    DBG(DBG_info, "window(s) set\n"); 
  }

 return status;
}


/* ------------------------------------------------------------ UMAX DO INQUIRY ---------------------------- */


static void umax_do_inquiry(Umax_Device *dev)
{
 size_t size;
 SANE_Status status;

  DBG(DBG_proc,"do_inquiry\n");
  memset(dev->buffer[0], '\0', 256);							     /* clear buffer */

  size = 5;

  set_inquiry_return_size(inquiry.cmd, size);  /* first get only 5 bytes to get size of inquiry_return_block */
  status = umax_scsi_cmd(dev, inquiry.cmd, inquiry.size, dev->buffer[0], &size);
  if (status)
  {
    DBG(DBG_error, "umax_do_inquiry: command returned status %s\n", sane_strstatus(status));
  }  

  size = get_inquiry_additional_length(dev->buffer[0]) + 5;

  set_inquiry_return_size(inquiry.cmd, size);			        /* then get inquiry with actual size */
  status = umax_scsi_cmd(dev, inquiry.cmd, inquiry.size, dev->buffer[0], &size);
  if (status)
  {
    DBG(DBG_error, "umax_do_inquiry: command returned status %s\n", sane_strstatus(status));
  }  
}


/* ------------------------------------------------------------ UMAX START SCAN ---------------------------- */


static SANE_Status umax_start_scan(Umax_Device *dev)
{
 int size = 1;
 SANE_Status status;

  DBG(DBG_proc,"start_scan\n");

  if (dev->adf) 							/* ADF selected: test for ADF errors */
  {
    umax_do_inquiry(dev);								      /* get inquiry */

    if (get_inquiry_ADF_paper_jam(dev->buffer[0]))					   /* test for ADF paper jam */
    {
      DBG(DBG_error,"ERROR: umax_start_scan: ADF paper jam\n");
      return SANE_STATUS_JAMMED;
    }
    else if (get_inquiry_ADF_cover_open(dev->buffer[0]))				  /* test for ADF cover open */
    {
      DBG(DBG_error,"ERROR: umax_start_scan: ADF cover open\n");
      return SANE_STATUS_COVER_OPEN;
    }
    else if (get_inquiry_ADF_no_paper(dev->buffer[0]))				    /* test for ADF no paper */
    {
      DBG(DBG_error,"ERROR: umax_start_scan: ADF no paper\n");
      return SANE_STATUS_NO_DOCS;
    }
  }

  set_SC_quality(scan.cmd, dev->quality);						  /*  1=qual, 0=fast */
  set_SC_adf(    scan.cmd, dev->adf);							/* ADF, 0=off, 1=use */
  set_SC_preview(scan.cmd, dev->preview);							/* 1=preview */
  
  set_SC_wid(scan.cmd, 1, 0);								/* Window-Identifier */

  set_SC_xfer_length(scan.cmd, size);							  /* following Bytes */

  DBG(DBG_info,"starting scan\n");

  status = umax_scsi_cmd(dev, scan.cmd, scan.size + size, NULL, NULL);
  if (status)
  {
    DBG(DBG_error, "umax_start_scan: command returned status %s\n", sane_strstatus(status));
  }  

 return status;
}


/* ------------------------------------------------------------ UMAX DO CALIBRATION ------------------------ */


static SANE_Status umax_do_calibration(Umax_Device *dev)
{
 SANE_Status status;
 unsigned int width   = 0;
 unsigned int lines   = 0;
 unsigned int bytespp = 0;

  DBG(DBG_proc,"do_calibration\n");

  status = umax_wait_scanner(dev);

  if ((status == SANE_STATUS_GOOD) && (dev->do_calibration != 0))			    /* calibration by driver */
  {
   unsigned char *shading_data = 0;
   unsigned int i, j;
   long *average;


    DBG(DBG_info,"driver is doing calibration\n");


    if (umax_execute_request_sense)
    {
      DBG(DBG_info,"request sense call is enabled\n");
      memset(dev->buffer[0], 0, rs_return_block_size);					  /* clear sense data buffer */
      umax_do_request_sense(dev);					   /* new request-sense call to get all data */
    }
    else
    {
      DBG(DBG_info,"request sense call is disabled\n");
    }

    if (get_RS_SCC_condition_code(dev->buffer[0]) != 1)
    {
      DBG(DBG_warning,"WARNING: missing information about shading-data\n");
      DBG(DBG_warning,"         driver tries to guess missing values!\n");

      if ((dev->calibration_area != UMAX_CALIBRATION_AREA_CCD) && (!dev->batch_scan))
      /* calibration is done with image geometry and depth */
      {
        DBG(DBG_warning,"         Calibration is done with selected image geometry and depth!\n");

        width = dev->scanwidth * dev->relevant_optical_res / dev->x_coordinate_base;

        if (dev->calibration_width_offset > -99999) /* driver or user (umax.conf) define an offset */
        {
          width = width + dev->calibration_width_offset; 
          DBG(DBG_warning,"         Using calibration width offset of %d\n", dev->calibration_width_offset);
        }

        if (dev->colormode == RGB)
        {
          width = width * 3;
        }

        lines   = dev->calib_lines;

        if (dev->gamma_input_bits_code <= 1)
        {
          bytespp = 1; /* 8 bit mode */
        }
        else
        {
          bytespp = 2; /* 16 bit mode */
        }
      }
      else /*  calibration is done with full scanarea and full depth */
      {
        DBG(DBG_warning,"         Calibration is done for each CCD pixel with full depth!\n");

        width = (int)(dev->inquiry_fb_width * dev->inquiry_optical_res);

        if (dev->batch_scan)
        {
          if (dev->calibration_width_offset_batch > -99999) /* driver or user (umax.conf) define an offset for batch scanning */
          {
            width = width + dev->calibration_width_offset_batch; 
            DBG(DBG_warning,"         Using calibration width offset for batch scanning of %d\n", dev->calibration_width_offset_batch);
          }
        }
        else /* normal scan */
        {
          if (dev->calibration_width_offset > -99999) /* driver or user (umax.conf) define an offset */
          {
            width = width + dev->calibration_width_offset; 
            DBG(DBG_warning,"         Using calibration width offset of %d\n", dev->calibration_width_offset);
          }
        }

        if (dev->colormode == RGB)
        {
          width = width * 3;
        }

        lines = dev->calib_lines;

        if (dev->gamma_input_bits_code <= 1)
        {
          bytespp = 1; /* 8 bit mode */
         }
        else
        {
          bytespp = 2; /* 16 bit mode */
        }
      }
    }
    else
    {
      lines   =  get_RS_SCC_calibration_lines(dev->buffer[0]);
      bytespp =  get_RS_SCC_calibration_bytespp(dev->buffer[0]);
      width   =  get_RS_SCC_calibration_bytesperline(dev->buffer[0]) / bytespp;
    }

    if (dev->calibration_bytespp > 0) /* correct bytespp if necessary and driver knows about it or user did select it */
    {
      bytespp = dev->calibration_bytespp;
    }

    DBG(DBG_info,"scanner sends %d lines with %d pixels and %d bytes/pixel\n", lines, width, bytespp);

    if (width * bytespp > dev->bufsize)
    {
      DBG(DBG_error,"ERROR: scsi buffer is to small for one shading line, calibration aborted\n");
      DBG(DBG_error,"=> change umax.conf options scsi-buffer-size-min and scsi-buffer-size-max\n");
     return SANE_STATUS_NO_MEM;
    }

    /* UMAX S12 sends a kind of uncalibrated image data, bright -> 255, dark -> 0 */
    /* (although 0 is not black) my scanner sends values around 220 */
    /* for some scanners the data is simply sent back, other scanners want 255-value as awnswer */

    average = calloc(width, sizeof(long));
    if (average == 0)
    {
      DBG(DBG_error,"ERROR: could not allocate memory for averaging shading data: calibration aborted\n");
     return SANE_STATUS_NO_MEM;
    }

    shading_data = calloc(width, bytespp);
    if (shading_data == 0)
    {
      DBG(DBG_error,"ERROR: could not allocate memory for shading data: calibration aborted\n");
     return SANE_STATUS_NO_MEM;
    }

    if (bytespp == 1)					 /* 1 byte per pixel */
    {
      DBG(DBG_info,"calculating average value for 8 bit shading data!\n");

      for (i=0; i<lines; i++)
      {
        umax_read_shading_data(dev, width * bytespp);

        for (j=0; j<width; j++)
        {
          average[j] += (long) dev->buffer[0][j];
        }

        DBG(DBG_read,"8 bit shading-line %d read\n", i+1);
      }

      for (j=0; j<width; j++)
      {
        shading_data[j] = (unsigned char) (average[j] / lines);
      }
    }
    else if (dev->low_byte_first) /* 2 bytes per pixel with low byte first */
    {
      DBG(DBG_info,"calculating average value for 16 bit shading data (low byte first)!\n");
      for (i=0; i<lines; i++)
      {
        umax_read_shading_data(dev, width * bytespp);

        for (j=0; j<width; j++)
        {
          average[j] += (long) 256 * dev->buffer[0][2*j+1] + dev->buffer[0][2*j] ;
        }

        DBG(DBG_read,"16 bit shading-line %d read\n", i+1);
      }

      for (j=0; j<width; j++)
      {
        shading_data[2*j+1] = (unsigned char) (average[j] / (256 * lines));
        shading_data[2*j]   = (unsigned char) (average[j] / lines);
      }
    }
    else					/* 2 bytes per pixel with highbyte first */
    {
      DBG(DBG_info,"calculating average value for 16 bit shading data (high byte first)!\n");
      for (i=0; i<lines; i++)
      {
        umax_read_shading_data(dev, width * bytespp);

        for (j=0; j<width; j++)
        {
          average[j] += (long) 256 * dev->buffer[0][2*j] + dev->buffer[0][2*j + 1] ;
        }

        DBG(DBG_read,"16 bit shading-line %d read\n", i+1);
      }

      for (j=0; j<width; j++)
      {
        shading_data[2*j]   = (unsigned char) (average[j] / (256 * lines));
        shading_data[2*j+1] = (unsigned char) (average[j] / lines);
      }
    }

    free(average);

    if ( (dev->invert_shading_data) ) /* invert data */
    {
      if (bytespp == 1)
      {
        DBG(DBG_info,"inverting 8 bit shading data\n");

        for (j=0; j<width; j++)
        {
          shading_data[j] = 255 - shading_data[j];
        }
      }
      else
      {
       unsigned int value;

        DBG(DBG_info,"inverting 16 bit shading data\n");

        for (j=0; j<width; j++)
        {
          value = shading_data[2*j] + shading_data[2*j+1] * 256;
          value = 65535 - value;
          shading_data[2*j]   = (unsigned char) value/256;
          shading_data[2*j+1] = (unsigned char) value & 255;
        }
      }
    }

    umax_send_shading_data(dev, shading_data, width * bytespp);
    DBG(DBG_info,"shading-data sent\n");
    free(shading_data);

    status = umax_start_scan(dev);						      /* now start real scan */

    dev->do_calibration = 0;
  }

 return status;
}


/* ------------------------------------------------------------ UMAX DO NEW INQUIRY ------------------------ */


static void umax_do_new_inquiry(Umax_Device *dev, size_t size)	       /* call inquiry again if wrong length */
{
 SANE_Status status;

  DBG(DBG_proc,"do_new_inquiry\n");
  memset(dev->buffer[0], '\0', 256);							     /* clear buffer */

  set_inquiry_return_size(inquiry.cmd, size);
  status = umax_scsi_cmd(dev, inquiry.cmd, inquiry.size, dev->buffer[0], &size);
  if (status)
  {
    DBG(DBG_error, "umax_do_new_inquiry: command returned status %s\n", sane_strstatus(status));
  }  
}


/* ------------------------------------------------------------ UMAX CORRECT INQUIRY ----------------------- */


static void umax_correct_inquiry(Umax_Device *dev, char *vendor, char *product, char *version)
{
  DBG(DBG_info, "umax_correct_inquiry(\"%s %s %s\")\n", vendor, product, version);

  if (!strncmp(vendor, "UMAX ", 5))
  {
    if (!strncmp(product, "Astra 600S ", 11))
    {
     int add_len = get_inquiry_additional_length(dev->buffer[0]);

      DBG(DBG_warning,"setting up special options for %s\n", product);

      if (add_len == 0x8f)
      {
        DBG(DBG_warning," - correcting wrong inquiry data\n");
	umax_do_new_inquiry(dev, 0x9b);		  /* get inquiry with correct length */
        set_inquiry_length(dev->buffer[0], 0x9e); /* correct inquiry len */
					      /* correct color-ordering from pixel to line_with_ccd_distance */
        set_inquiry_color_order(dev->buffer[0], IN_color_ordering_line_w_ccd);
	set_inquiry_fb_uta_line_arrangement_mode(dev->buffer[0], 32);
	set_inquiry_CCD_line_distance(dev->buffer[0], 8);
        /* we should reset ADF-bit here too */

        if (dev->invert_shading_data == -1) /* nothing defined in umax.conf */
        {
          DBG(DBG_warning," - activating inversion of shading data\n");
          dev->invert_shading_data = 1;
        }
      }
    }
    else if (!strncmp(product, "Astra 610S ", 11))
    {
     int add_len = get_inquiry_additional_length(dev->buffer[0]);

      DBG(DBG_warning,"setting up special options for %s\n", product);

      if (add_len == 0x8f)
      {
        DBG(DBG_warning," - correcting wrong inquiry data\n");
	umax_do_new_inquiry(dev, 0x9b);		  /* get inquiry with correct length */
        set_inquiry_length(dev->buffer[0], 0x9e); /* correct inquiry len */
					      /* correct color-ordering from pixel to line_with_ccd_distance */
        set_inquiry_color_order(dev->buffer[0], IN_color_ordering_line_w_ccd);
	set_inquiry_fb_uta_line_arrangement_mode(dev->buffer[0], 33);
	set_inquiry_CCD_line_distance(dev->buffer[0], 8);

        if (dev->invert_shading_data == -1) /* nothing defined in umax.conf */
        {
          DBG(DBG_warning," - activating inversion of shading data\n");
          dev->invert_shading_data = 1;
        }
      }
    }
    else if ( (!strncmp(product, "Astra 1200S ", 12)) ||
              (!strncmp(product, "Perfection600 ", 14)) )
    {
      DBG(DBG_warning,"using standard options for %s\n", product);
    }
    else if (!strncmp(product, "Astra 1220S ", 12))
    {
      DBG(DBG_warning,"setting up special options for %s\n", product);

      if (dev->gamma_lsb_padded == -1) /* nothing defined in umax.conf and not by backend */
      {
        DBG(DBG_warning," - 16 bit gamma table is created lsb padded\n");
        dev->gamma_lsb_padded = 1;
      }

      if (!strncmp(version, "V1.5 ", 4))
      {
        DBG(DBG_warning," - lamp control enabled for version %s\n", version);
        dev->lamp_control_available = 1;
      }
    }
    else if (!strncmp(product, "Astra 2100S ", 12))
    {
      DBG(DBG_warning,"setting up special options for %s\n", product);
      DBG(DBG_warning," - lamp control enabled\n");
      dev->lamp_control_available = 1;

      if (dev->calibration_bytespp == -1) /* no calibration-bytespp defined in umax.conf */
      {
        DBG(DBG_warning," - setting calibration_bytespp = 1\n");
        dev->calibration_bytespp = 1; /* scanner says 2 bytespp for calibration but 1 bytepp is correct */
      }
    }
    else if (!strncmp(product, "Astra 2200 ", 11))
    {
      DBG(DBG_warning,"setting up special options for %s\n", product);
      DBG(DBG_warning," - lamp control enabled\n");
      dev->lamp_control_available = 1;

      if (dev->calibration_area == -1) /* no calibration area defined in umax.conf */
      {
        DBG(DBG_warning," - calibration by driver is done for each CCD pixel\n");
        dev->calibration_area = UMAX_CALIBRATION_AREA_CCD;
      }

      if (dev->calibration_bytespp == -1) /* no calibration-bytespp defined in umax.conf */
      {
        DBG(DBG_warning," - setting calibration_bytespp = 2\n");
        dev->calibration_bytespp = 2;
      }

      DBG(DBG_warning," - common x and y resolution\n");
      dev->common_xy_resolutions = 1;

      if (dev->connection_type == SANE_UMAX_USB)
      {
        DBG(DBG_warning," - disabling quality calibration for USB connection\n");
	set_inquiry_fw_quality(dev->buffer[0], 0);
      }
    }
    else if (!strncmp(product, "Astra 2400S ", 12))
    {
      DBG(DBG_warning,"setting up special options for %s\n", product);
      DBG(DBG_warning," - defining pauses\n");
      dev->pause_for_color_calibration = 7000;		/* pause between start_scan and do_calibration in ms */
      dev->pause_for_gray_calibration = 4000;		/* pause between start_scan and do_calibration in ms */
      dev->pause_after_calibration = 0000;		 /* pause between do_calibration and read data in ms */
      dev->pause_after_reposition = 3000;			      /* pause after repostion scanner in ms */
      dev->pause_for_moving = 3000;			         /* pause for moving scanhead over full area */

      DBG(DBG_warning," - correcting ADF bit in inquiry\n");
      set_inquiry_sc_adf(dev->buffer[0], 1);		   /* set second bit that indicates ADF is supported */
    }
    else if (!strncmp(product, "Vista-T630 ", 11))
    {
      DBG(DBG_warning,"setting up special options for %s\n", product);

      if (dev->slow == -1) /* option is not predefined in umax.conf */
      {
        DBG(DBG_warning," - activating slow option\n");
        dev->slow = 1;
      }
    }
    else if (!strncmp(product, "UC630 ", 6))
    {
      DBG(DBG_warning,"setting up special options for %s\n", product);
      DBG(DBG_warning," - reposition_scanner waits until move of scan head has finished\n");
      dev->pause_after_reposition = 0;	    /* call wait_scanner */
    }
    else if (!strncmp(product, "UC840 ", 6))
    {
      DBG(DBG_warning,"setting up special options for %s\n", product);
      DBG(DBG_warning," - reposition_scanner waits until move of scan head has finished\n");
      dev->pause_after_reposition = 0;	    /* call wait_scanner */
    }
    else if (!strncmp(product, "UC1260 ", 7))
    {
      DBG(DBG_warning,"setting up special options for %s\n", product);
      DBG(DBG_warning," - setting gamma download curve format to type 1\n");
      dev->inquiry_gamma_DCF = 1;				       /* define gamma download curve format */
      DBG(DBG_warning," - reposition_scanner waits until move of scan head has finished\n");
      dev->pause_after_reposition = 0;     /* call wait_scanner */
    }
    else if (!strncmp(product, "UC1200S ", 8))
    {
      DBG(DBG_warning,"setting up special options for %s\n", product);
      DBG(DBG_warning," - setting gamma download curve format to type 1\n");
      dev->inquiry_gamma_DCF = 1;				       /* define gamma download curve format */
    }
    else if (!strncmp(product, "UC1200SE ", 9))
    {
      DBG(DBG_warning,"setting up special options for %s\n", product);
      DBG(DBG_warning," - setting gamma download curve format to type 0\n");
      dev->inquiry_gamma_DCF = 0;				       /* define gamma download curve format */
    }
    else if (!strncmp(product, "ARCUS PLUS ", 11))
    {
      DBG(DBG_warning,"setting up special options for %s\n", product);
      DBG(DBG_warning," - setting gamma download curve format to type 0\n");
      dev->inquiry_gamma_DCF = 0;				       /* define gamma download curve format */
    }
    else if ( (!strncmp(product, "UMAX S-12G ", 11)) ||
              (!strncmp(product, "UMAX S-12 ", 10)) ||
              (!strncmp(product, "SuperVista S-12 ", 16)) )
    {
      DBG(DBG_warning,"setting up special options for %s\n", product);

      DBG(DBG_warning," - setting maximum calibration data lines to 66\n");
      set_inquiry_max_calibration_data_lines(dev->buffer[0], 66);

      if (dev->calibration_width_offset == -99999) /* no calibration-width-offset defined in umax.conf */
      {
        dev->calibration_width_offset = -1;
        DBG(DBG_warning," - adding calibration width offset of %d pixels\n", dev->calibration_width_offset);
      }

      if (dev->calibration_area == -1) /* no calibration area defined in umax.conf */
      {
        DBG(DBG_warning," - calibration by driver is done for each CCD pixel\n");
        dev->calibration_area = UMAX_CALIBRATION_AREA_CCD;
      }
    }
    else if (!strncmp(product, "Mirage D-16L ", 13))
    {
      DBG(DBG_warning,"setting up special options for %s\n", product);
      if (dev->calibration_area == -1) /* no calibration area defined in umax.conf */
      {
        DBG(DBG_warning," - calibration by driver is done for each CCD pixel\n");
        dev->calibration_area = UMAX_CALIBRATION_AREA_CCD;
      }

      if (dev->calibration_width_offset == -99999) /* no calibration-width-offset defined in umax.conf */
      {
        dev->calibration_width_offset = 308;
        DBG(DBG_warning," - adding calibration width offset of %d pixels\n", dev->calibration_width_offset);
      }
    }
    else if (!strncmp(product, "PowerLook III ", 14))
    {
      DBG(DBG_warning,"setting up special options for %s\n", product);

      if (dev->calibration_width_offset == -99999) /* no calibration-width-offset defined in umax.conf */
      {
        dev->calibration_width_offset = 28;
        DBG(DBG_warning," - adding calibration width offset of %d pixels\n", dev->calibration_width_offset);
      }
      /* calibration_area = image */

      if (dev->calibration_width_offset_batch == -99999) /* no calibration-width-offset for batch scanning defined in umax.conf */
      {
        dev->calibration_width_offset_batch = 828;
        DBG(DBG_warning," - adding calibration width offset for batch scanning of %d pixels\n", dev->calibration_width_offset_batch);
      }
    }
    else if (!strncmp(product, "Power Look 2000", 15))
    {
      DBG(DBG_warning,"setting up special options for %s\n", product);

      if (dev->calibration_width_offset == -99999) /* no calibration-width-offset defined in umax.conf */
      {
        dev->calibration_width_offset = 22;
        DBG(DBG_warning," - adding calibration width offset of %d pixels\n", dev->calibration_width_offset);
      }
      /* calibration_area = image */

      if (dev->calibration_width_offset_batch == -99999) /* no calibration-width-offset for batch scanning defined in umax.conf */
      {
        dev->calibration_width_offset_batch = 24;
        DBG(DBG_warning," - adding calibration width offset for batch scanning of %d pixels\n", dev->calibration_width_offset_batch);
      }
    }
    else if (!strncmp(product, "PowerLook 2100XL", 16))
    {
      DBG(DBG_warning,"setting up special options for %s\n", product);

      if (dev->calibration_width_offset == -99999) /* no calibration-width-offset defined in umax.conf */
      {
        dev->calibration_width_offset = 52;
        DBG(DBG_warning," - adding calibration width offset of %d pixels\n", dev->calibration_width_offset);
      }
      /* calibration_area = image */

      if (dev->calibration_width_offset_batch == -99999) /* no calibration-width-offset for batch scanning defined in umax.conf */
      {
        dev->calibration_width_offset_batch = 1052;
        DBG(DBG_warning," - adding calibration width offset for batch scanning of %d pixels\n", dev->calibration_width_offset_batch);
      }

      dev->force_quality_calibration = 1;
      DBG(DBG_warning," - always set quality calibration\n");

      /* the scanner uses the same exposure times for red, green and blue exposure_time_rgb_bind = 1 */
    }
    else if (!strncmp(product, "PowerLook 3000 ", 15))
    {
      DBG(DBG_warning,"setting up special options for %s\n", product);

      if (dev->calibration_width_offset == -99999) /* no calibration-width-offset defined in umax.conf */
      {
        dev->calibration_width_offset = 52;
        DBG(DBG_warning," - adding calibration width offset of %d pixels\n", dev->calibration_width_offset);
      }
      /* calibration_area = image */

      if (dev->calibration_width_offset_batch == -99999) /* no calibration-width-offset for batch scanning defined in umax.conf */
      {
        /* not tested */
        dev->calibration_width_offset_batch = 1052;
        DBG(DBG_warning," - adding calibration width offset for batch scanning of %d pixels\n", dev->calibration_width_offset_batch);
      }
    }
    else
    {
      DBG(DBG_warning,"using standard options for %s\n", product);
    }
  }
  else if (!strncmp(vendor, "LinoHell ", 9))
  {
    if ( (!strncmp(product, "Office ", 7)) || (!strncmp(product, "JADE ", 5)) ) /* is a Supervista S-12 */
    {
      DBG(DBG_warning,"setting up special options for %s\n", product);

      DBG(DBG_warning," - setting maximum calibration data lines to 66\n");
      set_inquiry_max_calibration_data_lines(dev->buffer[0], 66);

      if (dev->calibration_width_offset == -99999) /* no calibration-width-offset defined in umax.conf */
      {
        dev->calibration_width_offset = -1;
        DBG(DBG_warning," - adding calibration width offset of %d pixels\n", dev->calibration_width_offset);
      }

      if (dev->calibration_area == -1) /* no calibration area defined in umax.conf */
      {
        DBG(DBG_warning," - calibration by driver is done for each CCD pixel\n");
        dev->calibration_area = UMAX_CALIBRATION_AREA_CCD;
      }
    }
    else if (!strncmp(product, "OPAL2 ", 6)) /* looks like a Mirage II */
    {
      DBG(DBG_warning,"setting up special options for %s\n", product);

      if (dev->gamma_lsb_padded == -1) /* nothing defined in umax.conf and not by backend */
      {
        DBG(DBG_warning," - 16 bit gamma table is created lsb padded\n");
        dev->gamma_lsb_padded = 1;
      }
    }
  }
  else if (!strncmp(vendor, "Linotype ", 9))
  {
    if (!strncmp(product, "SAPHIR4 ", 8)) /* is a Powerlook III */
    {
      DBG(DBG_warning,"setting up special options for %s\n", product);

      if (dev->calibration_width_offset == -99999) /* no calibration-width-offset defined in umax.conf */
      {
        dev->calibration_width_offset = 28;
        DBG(DBG_warning," - adding calibration width offset of %d pixels\n", dev->calibration_width_offset);
      }
      /* calibration_area = image */

      if (dev->calibration_width_offset_batch == -99999) /* no calibration-width-offset for batch scanning defined in umax.conf */
      {
        dev->calibration_width_offset_batch = 828;
        DBG(DBG_warning," - adding calibration width offset for batch scanning of %d pixels\n", dev->calibration_width_offset_batch);
      }
    }
  }
  else if (!strncmp(vendor, "HDM ", 4))
  {
    if (!strncmp(product, "LS4H1S ", 7)) /* is a Powerlook III */
    {
      DBG(DBG_warning,"setting up special options for %s\n", product);

      if (dev->calibration_width_offset == -99999) /* no calibration-width-offset defined in umax.conf */
      {
        dev->calibration_width_offset = 28;
        DBG(DBG_warning," - adding calibration width offset of %d pixels\n", dev->calibration_width_offset);
      }
      /* calibration_area = image */

      if (dev->calibration_width_offset_batch == -99999) /* no calibration-width-offset for batch scanning defined in umax.conf */
      {
        dev->calibration_width_offset_batch = 828;
        DBG(DBG_warning," - adding calibration width offset for batch scanning of %d pixels\n", dev->calibration_width_offset_batch);
      }
    }
  }
  else if (!strncmp(vendor, "ESCORT ", 7))
  {
    if (!strncmp(product, "Galleria 600S ", 14)) /* this is an Astra 600S */
    {
     int add_len = get_inquiry_additional_length(dev->buffer[0]);

      DBG(DBG_warning,"setting up special options for %s\n", product);

      if (add_len == 0x8f)
      {
        DBG(DBG_warning," - correcting wrong inquiry data\n");
	umax_do_new_inquiry(dev, 0x9b);		  /* get inquiry with correct length */
        set_inquiry_length(dev->buffer[0], 0x9e); /* correct inquiry len */
					      /* correct color-ordering from pixel to line_with_ccd_distance */
        set_inquiry_color_order(dev->buffer[0], IN_color_ordering_line_w_ccd);
	set_inquiry_fb_uta_line_arrangement_mode(dev->buffer[0], 32);
	set_inquiry_CCD_line_distance(dev->buffer[0], 8);
        /* we should reset ADF-bit here too */

        if (dev->invert_shading_data == -1) /* nothing defined in umax.conf */
        {
          DBG(DBG_warning," - activating inversion of shading data\n");
          dev->invert_shading_data = 1;
        }
      }
    }
  }
  else if (!strncmp(vendor, "TriGem ", 7))
  {
    if (!strncmp(product, "PowerScanII ", 12)) /* is a Supervista S-12 */
    {
      DBG(DBG_warning,"setting up special options for %s\n", product);

      DBG(DBG_warning," - setting maximum calibration data lines to 66\n");
      set_inquiry_max_calibration_data_lines(dev->buffer[0], 66);

      if (dev->calibration_width_offset == -99999) /* no calibration-width-offset defined in umax.conf */
      {
        dev->calibration_width_offset = -1;
        DBG(DBG_warning," - adding calibration width offset of %d pixels\n", dev->calibration_width_offset);
      }

      if (dev->calibration_area == -1) /* no calibration area defined in umax.conf */
      {
        DBG(DBG_warning," - calibration by driver is done for each CCD pixel\n");
        dev->calibration_area = UMAX_CALIBRATION_AREA_CCD;
      }
    }
  }
}


/* ------------------------------------------------------------ UMAX IDENTIFY SCANNER ---------------------- */


static int umax_identify_scanner(Umax_Device *dev)
{
 char vendor[10];
 char product[0x12];
 char version[6];
 char *pp;

  DBG(DBG_proc,"identify_scanner\n");
  umax_do_inquiry(dev);									      /* get inquiry */
  if (get_inquiry_periph_devtype(dev->buffer[0]) != IN_periph_devtype_scanner) { return 1; }      /* no scanner */

  get_inquiry_vendor( (char *)dev->buffer[0], vendor);  vendor[8]   = ' '; vendor[9]   = '\0';
  get_inquiry_product((char *)dev->buffer[0], product); product[16] = ' '; product[17] = '\0';
  get_inquiry_version((char *)dev->buffer[0], version); version[4]  = ' '; version[5]  = '\0';

  pp = &vendor[8];
  while (*(pp-1) == ' ')
  {
    *pp-- = '\0';
  }

  pp = &product[0x10];
  while (*(pp-1) == ' ')
  {
    *pp-- = '\0';
  }
  
  pp = &version[4];
  while (*pp == ' ')
  {
    *pp-- = '\0';
  }

  DBG(DBG_info, "Found %s scanner %sversion %s on device %s\n", vendor, product, version, dev->devicename);

					      /* look for scanners that do not give all inquiry-information */
							   /* and if possible use driver-known inquiry-data  */

  if (get_inquiry_additional_length(dev->buffer[0])>=0x8f)
  {
    int i = 0;
    while (strncmp("END_OF_LIST", scanner_str[2*i], 11) != 0)	     /* Now identify full supported scanners */
    {
      if (!strncmp(vendor, scanner_str[2*i], strlen(scanner_str[2*i])) )
      { 
        if (!strncmp(product, scanner_str[2*i+1], strlen(scanner_str[2*i+1])) )
        {
	  umax_correct_inquiry(dev, vendor, product, version);
          return 0;
        } 
      }
      i++;
    }

    if (strncmp(vendor, "UMAX ", 5)) { return 1; }				      /* not UMAX then abort */

    DBG(DBG_error0, "WARNING: %s scanner %s version %s on device %s\n"
     "is currently an unrecognized device for this backend version.\n"
     "Please make sure you use the most recent version of the umax backend.\n"
     "You can download new umax-backend versions from:\n"
     "http://www.rauch-domain.de/sane-umax\n",
     vendor, product, version, dev->devicename);

    DBG(DBG_error0,
     "Inquiry seems to be ok.\n"
     "******************************************************************\n"
     "***             !!!! CONTINUE AT YOUR OWN RISK !!!!            ***\n"
     "******************************************************************\n"
     "If you already use the most recent umax-backend version\n"
     "then please contact me: Oliver.Rauch@rauch-domain.de\n");

    return 0; 
  }
  else									        /* inquiry-data not complete */
  if (!strncmp(vendor, "UMAX ", 5)) /* test UMAX-scanners with short inquiry */
  {
   inquiry_blk inq_data;
   int  i;

    for(i=0; i < known_inquiry; i++)
    {
      inq_data = *inquiry_table[i];
      if (!strncmp(product, inq_data.scanner, strlen(inq_data.scanner))) 
      {
	DBG(DBG_warning, "inquiry-block-length: %d\n", get_inquiry_additional_length(dev->buffer[0])+5);
	DBG(DBG_warning, "using driver-internal inquiry-data for this scanner!\n");

						      /* copy driver-defined inquiry-data into inquiry-block */
        memcpy(dev->buffer[0]+0x24, inq_data.inquiry, inq_data.inquiry_len-0x24);

        /* correct variables */
        set_inquiry_sc_uta(dev->buffer[0], get_inquiry_transavail(dev->buffer[0]));	/* transparancy available ? */
        set_inquiry_sc_adf(dev->buffer[0], get_inquiry_scanmode(dev->buffer[0]));	/* automatic document feeder available ? */

        set_inquiry_length(dev->buffer[0], inq_data.inquiry_len); 
        umax_correct_inquiry(dev, vendor, product, version);

        return 0;										       /* ok */
      }
    }
    DBG(DBG_error0, "ERROR: %s scanner %s version %s on device %s\n"
         "is currently an unrecognized device, and inquiry is too short,\n"
         "so we are not able to continue!\n"
         "Please make sure you use the most recent version of the umax backend.\n"
         "You can download new umax-backend versions from:\n"
         "http://www.rauch-domain.de/sane-umax\n"
         "You already use the most recent umax-backend version:\n"
         "Please contact me: Oliver.Rauch@rauch-domain.de\n",
         vendor, product, version, dev->devicename);
  }

 return 1;				    /* NO SUPPORTED SCANNER: short inquiry-block and unknown scanner */
}


/* ------------------------------------------------------------ UMAX TRIM BUFSIZE -------------------------- */


static void umax_trim_rowbufsize(Umax_Device *dev)
{
 unsigned int lines=0;

  if (dev->row_bufsize > dev->row_len)
  {
    lines = dev->row_bufsize / dev->row_len;

    if (lines > dev->lines_max) /* reduce number of lines to scan if set up in config file */
    {
      lines = dev->lines_max;
    }

    dev->row_bufsize = lines * dev->row_len;
  }

  DBG(DBG_proc,"trim_rowbufsize: row_bufsize = %d bytes = %d lines\n", dev->row_bufsize, lines);
}


/* ------------------------------------------------------------ UMAX CALCULATE EXPOSURE TIME --------------- */

  
static void umax_calculate_exposure_time(Umax_Device *dev, int def, int *value)
{
 int level;

  DBG(DBG_proc,"calculate_exposure_time\n");
  if ( (*value))
  {
    if ( (*value) == -1 ) { (*value) = def; }
    else
    {
      level = (*value) / dev->inquiry_exposure_time_step_unit;
      (*value) = inrange(dev->use_exposure_time_min, level, dev->inquiry_exposure_time_max);
    }
  }
}

			  
/* ------------------------------------------------------------ UMAX CHECK VALUES -------------------------- */


static int umax_check_values(Umax_Device *dev)
{
 double inquiry_x_orig;
 double inquiry_y_orig;
 double inquiry_width;
 double inquiry_length;
 unsigned int maxwidth;
 unsigned int maxlength;

  DBG(DBG_proc,"check_values\n");

  /* ------------------------------- flatbed ------------------------------- */

  dev->module = WD_module_flatbed;					  /* reset scanmode to flatbed first */

  /* --------------------------------- uta --------------------------------- */

  if (dev->uta != 0) 
  {
    dev->module = WD_module_transparency;
    if ( (dev->inquiry_uta == 0) || (dev->inquiry_transavail == 0) )
    {
      DBG(DBG_error, "ERROR: transparency mode not supported by scanner\n");
     return(1);
    }
  }

  /* --------------------------------- adf --------------------------------- */

  if (dev->adf != 0) 
  {
    if (dev->inquiry_adf == 0)
    {
      DBG(DBG_error,"ERROR: adf mode not supported by scanner\n");
     return(1);
    }
  }

  /* --------------------------------- dor --------------------------------- */

  if (dev->dor != 0)
  {
    if (dev->inquiry_dor == 0)
    {
      DBG(DBG_error, "ERROR: double optical resolution not supported by scanner\n");
     return(1); 
    }
  }

  /* ------------------------------- resolution ------------------------ */

  if (dev->dor == 0) /* standard (FB) */
  {
    dev->relevant_optical_res = dev->inquiry_optical_res;
    dev->relevant_max_x_res   = dev->inquiry_x_res;
    dev->relevant_max_y_res   = dev->inquiry_y_res;
  }
  else /* DOR mode */
  {
    dev->relevant_optical_res = dev->inquiry_dor_optical_res;
    dev->relevant_max_x_res   = dev->inquiry_dor_x_res;
    dev->relevant_max_y_res   = dev->inquiry_dor_y_res;
  }

  if (dev->x_resolution <= 0)
  {
    DBG(DBG_error,"ERROR: no x-resolution given\n");
    return(1);
  }

  if (dev->x_resolution > dev->relevant_max_x_res)
  {
    dev->x_resolution = dev->relevant_max_x_res;
  }

  if (dev->x_resolution > dev->relevant_optical_res)
  {
    dev->scale_x = 2;
  }
  else
  {
    dev->scale_x = 1;
  }

  if (dev->y_resolution <= 0)
  {
    DBG(DBG_error,"ERROR: no y-resolution given\n");
    return(1);
  }

  if (dev->y_resolution > dev->relevant_max_y_res)
  {
    dev->y_resolution = dev->relevant_max_y_res;
  }

  if (dev->y_resolution > dev->relevant_optical_res)
  {
    dev->scale_y = 2;
  }
  else if (dev->y_resolution > dev->relevant_optical_res/2)
  {
    dev->scale_y = 1;
  }
  else
  {
    /* astra 600S and 610S need this in umax_forget_line */
    dev->scale_y = 0.5;
  }


  /* ------------------------------- scanarea ------------------------ */

  if (dev->module == WD_module_flatbed)							     /* flatbed mode */
  {
    inquiry_x_orig = 0;								   /* flatbed origin */
    inquiry_y_orig = 0;
    inquiry_width  = dev->inquiry_fb_width;						    /* flatbed width */
    inquiry_length = dev->inquiry_fb_length;
  }
  else										        /* transparency mode */
  {
    inquiry_x_orig = dev->inquiry_uta_x_off;						       /* uta origin */
    inquiry_y_orig = dev->inquiry_uta_y_off;
    inquiry_width  = dev->inquiry_uta_x_off + dev->inquiry_uta_width;				/* uta width */
    inquiry_length = dev->inquiry_uta_y_off + dev->inquiry_uta_length;
  }

  if (dev->dor != 0)
  {
    inquiry_x_orig = dev->inquiry_dor_x_off;						       /* dor origin */
    inquiry_y_orig = dev->inquiry_dor_y_off;
    inquiry_width  = dev->inquiry_dor_x_off + dev->inquiry_dor_width;				/* dor width */
    inquiry_length = dev->inquiry_dor_y_off + dev->inquiry_dor_length;
  }

							     /* limit the size to what the scanner can scan. */
					   /* this is particularly important because the scanners don't have */
				  /* built-in checks and will happily grind their gears if this is exceeded. */


  maxwidth = inquiry_width  * dev->x_coordinate_base - dev->upper_left_x - 1;

  if ( (dev->scanwidth <= 0) || (dev->scanwidth > maxwidth) )
  {
    dev->scanwidth = maxwidth;
  }

  if (dev->upper_left_x < inquiry_x_orig)
  {
    dev->upper_left_x = inquiry_x_orig;
  }


  maxlength = inquiry_length * dev->y_coordinate_base - dev->upper_left_y - 1;

  if ( (dev->scanlength <= 0) || (dev->scanlength > maxlength) )
  {
    dev->scanlength = maxlength;
  }

  if (dev->upper_left_y < inquiry_y_orig)
  {
    dev->upper_left_y = inquiry_y_orig;
  }


  /* Now calculate width and length in pixels */
  dev->width_in_pixels  = umax_calculate_pixels(dev->scanwidth,  dev->x_resolution,
                                                dev->relevant_optical_res * dev->scale_x, dev->x_coordinate_base);

  dev->length_in_pixels = umax_calculate_pixels(dev->scanlength, dev->y_resolution,
                                                dev->relevant_optical_res * dev->scale_y, dev->y_coordinate_base);

  if ((dev->scanwidth <= 0) || (dev->scanlength <= 0))
  {
    DBG(DBG_error,"ERROR: scanwidth or scanlength not given\n");
    return(1);
  }

  if (dev->bits_per_pixel_code == 1)
  {
    dev->bytes_per_color = 1;
  }
  else
  {
    dev->bytes_per_color = 2;
  }

  switch(dev->colormode)
  {
   case LINEART:
     dev->width_in_pixels -= dev->width_in_pixels % 8;
     dev->row_len = (dev->width_in_pixels / 8);
    break;

   case HALFTONE:
     dev->width_in_pixels -= dev->width_in_pixels % 8;
     dev->row_len = (dev->width_in_pixels / 8);
    break;

   case GRAYSCALE:
     dev->row_len = dev->width_in_pixels * dev->bytes_per_color;
    break;

   case RGB_LINEART:
   case RGB_HALFTONE:
     if (dev->three_pass)
     {
       dev->row_len = dev->width_in_pixels / 8 ;
     }
     else
     {
       dev->row_len = (dev->width_in_pixels / 8 ) * 3;
     }
    break;

   case RGB:
     if (dev->three_pass)				     /* three (24bpp) or six (30bpp) bytes per pixel */
     {
       dev->row_len = dev->width_in_pixels * dev->bytes_per_color;
     }
     else
     {
       dev->row_len = dev->width_in_pixels * 3 * dev->bytes_per_color;
     }
    break;
  }


  /* ------------------------------- wdb length ------------------------ */

  if (dev->wdb_len <= 0)
  {
    dev->wdb_len = dev->inquiry_wdb_len; 
    if (dev->wdb_len <= 0)
    {
      DBG(DBG_error,"ERROR: wdb-length not given\n");
      return(1);
    }
  }

  if (dev->wdb_len > used_WDB_size)
  {
    DBG(DBG_warning,"WARNING:window descriptor block too long, will be shortned!\n");
    dev->wdb_len = used_WDB_size;
  }

  /* ----------------------------- cbhs-range ----------------------------- */

  dev->threshold   = umax_cbhs_correct(dev->inquiry_threshold_min,  dev->threshold , dev->inquiry_threshold_max);
  dev->contrast    = umax_cbhs_correct(dev->inquiry_contrast_min,   dev->contrast  , dev->inquiry_contrast_max);
  dev->brightness  = umax_cbhs_correct(dev->inquiry_brightness_min, dev->brightness, dev->inquiry_brightness_max);

  dev->highlight_r = umax_cbhs_correct(dev->inquiry_highlight_min, dev->highlight_r, dev->inquiry_highlight_max);
  dev->highlight_g = umax_cbhs_correct(dev->inquiry_highlight_min, dev->highlight_g, dev->inquiry_highlight_max);
  dev->highlight_b = umax_cbhs_correct(dev->inquiry_highlight_min, dev->highlight_b, dev->inquiry_highlight_max);

  dev->shadow_r    = umax_cbhs_correct(dev->inquiry_shadow_min, dev->shadow_r, dev->inquiry_shadow_max-1);
  dev->shadow_g    = umax_cbhs_correct(dev->inquiry_shadow_min, dev->shadow_g, dev->inquiry_shadow_max-1);
  dev->shadow_b    = umax_cbhs_correct(dev->inquiry_shadow_min, dev->shadow_b, dev->inquiry_shadow_max-1);

  if (dev->shadow_r >= dev->highlight_r)
  {
    dev->shadow_r = dev->highlight_r-1;
  }
  if (dev->shadow_g >= dev->highlight_g)
  {
    dev->shadow_g = dev->highlight_g-1;
  }
  if (dev->shadow_b >= dev->highlight_b)
  {
    dev->shadow_b = dev->highlight_b-1;
  }

  /* ----------------------- quality calibration and preview -------------- */

  if (dev->inquiry_preview == 0)
  {
    if (dev->preview)
    {
      DBG(DBG_warning, "WARNING: fast preview function not supported by scanner\n");
      dev->preview = 0;
    }
  }

  /* always set calibration lines because we also need this value if the scanner
     requeires calibration by driver */
  dev->calib_lines = dev->inquiry_max_calib_lines;

  if (dev->force_quality_calibration)
  {
    dev->quality = 1; /* always use quality calibration */
  }
  else if (dev->inquiry_quality_ctrl == 0)
  {
    if (dev->quality)
    {
      DBG(DBG_warning, "WARNING: quality calibration not supported by scanner\n");
      dev->quality = 0;
    }
  }
  else
  {
    if (dev->preview != 0)
    {
      DBG(DBG_info, "quality calibration disabled in preview mode\n");
      dev->quality = 0; /* do not use quality calibration in preview mode */
    }
  }

  /* --------------------------- lamp intensity control ------------------- */

  if (dev->inquiry_lamp_ctrl == 0)
  {
    if (dev->c_density || dev->s_density)
    {
      DBG(DBG_warning, "WARNING: scanner doesn't support lamp intensity control\n");
    }
    dev->c_density = dev->s_density = 0;
  }


  /* --------------------------- reverse (negative) ----------------------- */

  if (dev->reverse != 0)
  {
    if ( (dev->colormode == LINEART)     || (dev->colormode == HALFTONE) ||
         (dev->colormode == RGB_LINEART) || (dev->colormode == RGB_HALFTONE) )
    {
      if (dev->inquiry_reverse == 0)
      {
         DBG(DBG_error, "ERROR: reverse for bi-level-image not supported\n");
         return(1);
      }
    }
    else
    { dev->reverse = 0; }
  }

  if (dev->reverse_multi != 0)
  {
    if ((dev->colormode == RGB) || (dev->colormode == GRAYSCALE) )
    {
      if (dev->inquiry_reverse_multi == 0)
      {
         DBG(DBG_error, "ERROR: reverse for multi-level-image not supported\n");
         return(1);
      }
    }
    else
    {
      dev->reverse_multi = 0;
    }
  }

  /* ----------------------------- analog gamma ---------------------------- */

  if (dev->inquiry_analog_gamma == 0)
  {
    if (dev->analog_gamma_r + dev->analog_gamma_g + dev->analog_gamma_b != 0)
    {
      DBG(DBG_warning,"WARNING: analog gamma correction not supported by scanner!\n");
    }
    dev->analog_gamma_r = dev->analog_gamma_g = dev->analog_gamma_b = 0;
  }

  /* ---------------------------- digital gamma ---------------------------- */

  if ( (dev->digital_gamma_r == 0) || (dev->digital_gamma_g == 0) ||
       (dev->digital_gamma_b == 0) )
  {
    if (dev->inquiry_gamma_dwload == 0)
    {
      DBG(DBG_warning, "WARNING: gamma download not available\n");
      dev->digital_gamma_r = dev->digital_gamma_g = dev->digital_gamma_b = 15;
    }
  }

  /* ---------------------------- speed and smear  ------------------------- */
  
  if (dev->slow == 1)
  {
    dev->WD_speed = WD_speed_slow;
  }
  else
  {
    dev->WD_speed = WD_speed_fast;
  }

  if (dev->smear == 1)
  {
    dev->WD_speed += WD_speed_smear;
  }

  /* ---------------------- test bits per pixel  --------------------------- */
  
  if ( ( (dev->inquiry_GIB | 1) & dev->gamma_input_bits_code) == 0 )
  {
    DBG(DBG_warning,"WARNING: selected gamma input bits not supported, gamma ignored\n");
    dev->gamma_input_bits_code = 1;
    dev->digital_gamma_r = dev->digital_gamma_g = dev->digital_gamma_b = 15;
  }

  if ( ( (dev->inquiry_GOB | 1) & dev->bits_per_pixel_code) == 0 )
  {
    DBG(DBG_error,"ERROR: selected bits per pixel not supported\n");
    return(1);
  }
  
  /* ----------------------- scan mode dependencies ------------------------ */

  switch(dev->colormode)
  {
    case LINEART:							       /* ------------ LINEART ------------- */
    case RGB_LINEART:						       /* ---------- RGB_LINEART ----------- */
      dev->use_exposure_time_min = dev->inquiry_exposure_time_l_min;

      if (dev->module == WD_module_flatbed) 
      {
        dev->use_exposure_time_def_r = dev->inquiry_exposure_time_l_fb_def;
      }
      else
      {
        dev->use_exposure_time_def_r = dev->inquiry_exposure_time_l_uta_def;
      }

      if (dev->inquiry_lineart == 0)
      {
        DBG(DBG_error,"ERROR: lineart mode not supported by scanner\n");
       return(1);
      }
     break;

    case HALFTONE:							 /* ----------- HALFTONE------------ */
    case RGB_HALFTONE:							 /* --------- RGB_HALFTONE---------- */
      dev->use_exposure_time_min = dev->inquiry_exposure_time_h_min;
      if (dev->module == WD_module_flatbed) 
      {
        dev->use_exposure_time_def_r = dev->inquiry_exposure_time_h_fb_def;
      }
      else
      {
        dev->use_exposure_time_def_r = dev->inquiry_exposure_time_h_uta_def;
      }

      if (dev->inquiry_halftone == 0)
      {
        DBG(DBG_error,"ERROR: halftone mode not supported by scanner\n");
        return(1);
      }
     break;

    case GRAYSCALE:						       /* ---------- GRAYSCALE ------------- */
      dev->use_exposure_time_min = dev->inquiry_exposure_time_g_min;

      if (dev->module == WD_module_flatbed) 
      {
        dev->use_exposure_time_def_r = dev->inquiry_exposure_time_g_fb_def;
      }
      else
      {
        dev->use_exposure_time_def_r = dev->inquiry_exposure_time_g_uta_def;
      }

      if (dev->inquiry_gray == 0)
      {
        DBG(DBG_error, "ERROR: grayscale mode not supported by scanner\n");
       return(1);
      }
     break;

    case RGB:							       /* ----------------- COLOR ---------- */
      dev->use_exposure_time_min = dev->inquiry_exposure_time_c_min;
      if (dev->module == WD_module_flatbed) 
      {
        dev->use_exposure_time_def_r = dev->inquiry_exposure_time_c_fb_def_r;
        dev->use_exposure_time_def_g = dev->inquiry_exposure_time_c_fb_def_g;
        dev->use_exposure_time_def_b = dev->inquiry_exposure_time_c_fb_def_b;
      }
      else
      {
        dev->use_exposure_time_def_r = dev->inquiry_exposure_time_c_uta_def_r;
        dev->use_exposure_time_def_g = dev->inquiry_exposure_time_c_uta_def_g;
        dev->use_exposure_time_def_b = dev->inquiry_exposure_time_c_uta_def_b;
      }

      if (dev->inquiry_color == 0)
      {
        DBG(DBG_error,"ERROR: color mode not supported by scanner\n");
       return(1);
      }

      if (dev->inquiry_one_pass_color)
      {
        DBG(DBG_info,"using one pass scanning mode\n");

        if (dev->inquiry_color_order & IN_color_ordering_pixel)
        {
          DBG(DBG_info,"scanner uses color-pixel-ordering\n");
        }
        else if (dev->inquiry_color_order & IN_color_ordering_line_no_ccd)
        {
          dev->CCD_distance = 0;
          dev->do_color_ordering = 1;
          DBG(DBG_info,"scanner uses color-line-ordering without CCD-distance\n");
        }
        else if (dev->inquiry_color_order & IN_color_ordering_line_w_ccd)
        {
          dev->CCD_distance = dev->inquiry_CCD_line_distance;
          dev->do_color_ordering = 1;
          switch (dev->inquiry_fb_uta_color_arrangement)		     /* define color order for line ordering */
          {
            case 1:
              dev->CCD_color[0] = CCD_color_green;

              dev->CCD_color[1] = CCD_color_blue;
              dev->CCD_color[2] = CCD_color_green;

              dev->CCD_color[3] = CCD_color_blue;
              dev->CCD_color[4] = CCD_color_red;
              dev->CCD_color[5] = CCD_color_green;

              dev->CCD_color[6] = CCD_color_blue;
              dev->CCD_color[7] = CCD_color_red;

              dev->CCD_color[8] = CCD_color_red;
             break;

            case 2:
              dev->CCD_color[0] = CCD_color_blue;

              dev->CCD_color[1] = CCD_color_green;
              dev->CCD_color[2] = CCD_color_blue;

              dev->CCD_color[3] = CCD_color_green;
              dev->CCD_color[4] = CCD_color_red;
              dev->CCD_color[5] = CCD_color_blue;

              dev->CCD_color[6] = CCD_color_green;
              dev->CCD_color[7] = CCD_color_red;

              dev->CCD_color[8] = CCD_color_red;
             break;

            case 3:
              dev->CCD_color[0] = CCD_color_red;

              dev->CCD_color[1] = CCD_color_blue;
              dev->CCD_color[2] = CCD_color_red;
 
              dev->CCD_color[3] = CCD_color_blue;
              dev->CCD_color[4] = CCD_color_green;
              dev->CCD_color[5] = CCD_color_red;

              dev->CCD_color[6] = CCD_color_blue;
              dev->CCD_color[7] = CCD_color_green;

              dev->CCD_color[8] = CCD_color_green;
             break;

            case 4:										 /* may be wrong !!! */
              dev->CCD_color[0] = CCD_color_red;

              dev->CCD_color[1] = CCD_color_green;
              dev->CCD_color[2] = CCD_color_red;

              dev->CCD_color[3] = CCD_color_green;
              dev->CCD_color[4] = CCD_color_red;
              dev->CCD_color[5] = CCD_color_blue;

              dev->CCD_color[6] = CCD_color_green;
              dev->CCD_color[7] = CCD_color_blue;

              dev->CCD_color[8] = CCD_color_blue;
             break;

            case 32:						    /* not defined from UMAX, for Astra 600S */
              dev->CCD_color[0] = CCD_color_green;

              dev->CCD_color[1] = CCD_color_green;
              dev->CCD_color[2] = CCD_color_blue;

              dev->CCD_color[3] = CCD_color_green;
              dev->CCD_color[4] = CCD_color_red;
              dev->CCD_color[5] = CCD_color_blue;

              dev->CCD_color[6] = CCD_color_red;
              dev->CCD_color[7] = CCD_color_blue;

              dev->CCD_color[8] = CCD_color_red;
             break;

            case 33:						    /* not defined from UMAX, for Astra 610S */
              dev->CCD_color[0] = CCD_color_red;

              dev->CCD_color[1] = CCD_color_red;
              dev->CCD_color[2] = CCD_color_blue;

              dev->CCD_color[3] = CCD_color_red;
              dev->CCD_color[4] = CCD_color_green;
              dev->CCD_color[5] = CCD_color_blue;

              dev->CCD_color[6] = CCD_color_green;
              dev->CCD_color[7] = CCD_color_blue;
 
              dev->CCD_color[8] = CCD_color_green;
             break;

            default:
              dev->CCD_color[0] = CCD_color_green;
 
              dev->CCD_color[1] = CCD_color_blue;
              dev->CCD_color[2] = CCD_color_green;

              dev->CCD_color[3] = CCD_color_blue;
              dev->CCD_color[4] = CCD_color_red;
              dev->CCD_color[5] = CCD_color_green;

              dev->CCD_color[6] = CCD_color_blue;
              dev->CCD_color[7] = CCD_color_red;

              dev->CCD_color[8] = CCD_color_red;
          }
          DBG(DBG_info,"scanner uses color-line-ordering with CCD-distance of %d lines\n", dev->CCD_distance);
        }
        else
        { 
          DBG(DBG_error,"ERROR: color-ordering-type not supported \n");
         return(1);
        }
      }
      else 
      {
        DBG(DBG_info,"using three pass scanning mode\n");
        dev->three_pass=1;
      }
     break;
  } /* switch */

  /* ----------------------------- color ordering  ------------------------ */

  if (dev->do_color_ordering != 0)
  {
    if ( (dev->colormode != RGB) || (dev->three_pass != 0) )
    {
      dev->do_color_ordering = 0; /* color ordering not necessery */
    }
  }

 return(0);
}


/* ------------------------------------------------------------ UMAX GET INQUIRY VALUES -------------------- */


static void umax_get_inquiry_values(Umax_Device *dev)
{
 unsigned char * inquiry_block;

  DBG(DBG_proc,"get_inquiry_values\n");

  inquiry_block   = dev->buffer[0];
  dev->inquiry_len = get_inquiry_additional_length(dev->buffer[0])+5;
  dev->cbhs_range  = dev->inquiry_cbhs = get_inquiry_CBHS(inquiry_block);

  if (dev->cbhs_range > IN_CBHS_255)
  {
    dev->cbhs_range = IN_CBHS_255;
  }

  if (dev->cbhs_range == IN_CBHS_50)
  {
    dev->inquiry_contrast_min   = 103;	      /* minimum value for c */
    dev->inquiry_contrast_max   = 153;	      /* maximum value for c */
    dev->inquiry_brightness_min = 78;	      /* minimum value for b */
    dev->inquiry_brightness_max = 178;	      /* maximum value for b */
    dev->inquiry_threshold_min  = 78;	      /* minimum value for t */
    dev->inquiry_threshold_max  = 178;	      /* maximum value for t */
    dev->inquiry_highlight_min  = 1;	      /* minimum value for h */
    dev->inquiry_highlight_max  = 50;	      /* maximum value for h */
    dev->inquiry_shadow_min     = 0;	      /* minimum value for s */
    dev->inquiry_shadow_max     = 49;	      /* maximum value for s */ 
  }

  get_inquiry_vendor( (char *)inquiry_block, dev->vendor);  dev->vendor[8]  ='\0';
  get_inquiry_product((char *)inquiry_block, dev->product); dev->product[16]='\0';
  get_inquiry_version((char *)inquiry_block, dev->version); dev->version[4] ='\0';

  dev->inquiry_batch_scan       = get_inquiry_fw_batch_scan(inquiry_block);
  dev->inquiry_quality_ctrl     = get_inquiry_fw_quality(inquiry_block);
  dev->inquiry_preview          = get_inquiry_fw_fast_preview(inquiry_block);
  dev->inquiry_lamp_ctrl        = get_inquiry_fw_lamp_int_cont(inquiry_block);
  dev->inquiry_calibration      = get_inquiry_fw_calibration(inquiry_block);
  dev->inquiry_transavail       = get_inquiry_transavail(inquiry_block);
  dev->inquiry_adfmode          = get_inquiry_scanmode(inquiry_block);

  if (dev->inquiry_len<=0x8f)
  {
    DBG(DBG_warning, "WARNING: inquiry return block is unexpected short.\n");
  }

  dev->inquiry_uta              = get_inquiry_sc_uta(inquiry_block);
  dev->inquiry_adf              = get_inquiry_sc_adf(inquiry_block);

  dev->inquiry_one_pass_color   = get_inquiry_sc_one_pass_color(inquiry_block);
  dev->inquiry_three_pass_color = get_inquiry_sc_three_pass_color(inquiry_block);
  dev->inquiry_color            = get_inquiry_sc_color(inquiry_block);
  dev->inquiry_gray             = get_inquiry_sc_gray(inquiry_block);
  dev->inquiry_halftone         = get_inquiry_sc_halftone(inquiry_block);
  dev->inquiry_lineart          = get_inquiry_sc_lineart(inquiry_block);

  dev->inquiry_exposure_adj              = get_inquiry_fw_adjust_exposure_tf(inquiry_block);
  dev->inquiry_exposure_time_step_unit   = get_inquiry_exposure_time_step_unit(inquiry_block);
  dev->inquiry_exposure_time_max         = get_inquiry_exposure_time_max(inquiry_block);

             /* --- lineart --- */
  dev->inquiry_exposure_time_l_min       = get_inquiry_exposure_time_lhg_min(inquiry_block);
  dev->inquiry_exposure_time_l_fb_def    = get_inquiry_exposure_time_lh_def_fb(inquiry_block);
  dev->inquiry_exposure_time_l_uta_def   = get_inquiry_exposure_time_lh_def_uta(inquiry_block);

             /* --- halftone --- */
  dev->inquiry_exposure_time_h_min       = get_inquiry_exposure_time_lhg_min(inquiry_block);
  dev->inquiry_exposure_time_h_fb_def    = get_inquiry_exposure_time_lh_def_fb(inquiry_block);
  dev->inquiry_exposure_time_h_uta_def   = get_inquiry_exposure_time_lh_def_uta(inquiry_block);

             /* --- grayscale --- */
  dev->inquiry_exposure_time_g_min       = get_inquiry_exposure_time_lhg_min(inquiry_block);
  dev->inquiry_exposure_time_g_fb_def    = get_inquiry_exposure_time_gray_def_fb(inquiry_block);
  dev->inquiry_exposure_time_g_uta_def   = get_inquiry_exposure_time_gray_def_uta(inquiry_block);

             /* --- color --- */
  dev->inquiry_exposure_time_c_min       = get_inquiry_exposure_time_color_min(inquiry_block);
  dev->inquiry_exposure_time_c_fb_def_r  = get_inquiry_exposure_time_def_r_fb(inquiry_block);
  dev->inquiry_exposure_time_c_fb_def_g  = get_inquiry_exposure_time_def_g_fb(inquiry_block);
  dev->inquiry_exposure_time_c_fb_def_b  = get_inquiry_exposure_time_def_g_fb(inquiry_block);
  dev->inquiry_exposure_time_c_uta_def_r = get_inquiry_exposure_time_def_r_uta(inquiry_block);
  dev->inquiry_exposure_time_c_uta_def_g = get_inquiry_exposure_time_def_g_uta(inquiry_block);
  dev->inquiry_exposure_time_c_uta_def_b = get_inquiry_exposure_time_def_b_uta(inquiry_block);


  dev->inquiry_dor           = get_inquiry_sc_double_res(inquiry_block);
  dev->inquiry_reverse       = get_inquiry_sc_bi_image_reverse(inquiry_block);
  dev->inquiry_reverse_multi = get_inquiry_sc_multi_image_reverse(inquiry_block);
  dev->inquiry_shadow        = 1 - get_inquiry_sc_no_shadow(inquiry_block);
  dev->inquiry_highlight     = 1 - get_inquiry_sc_no_highlight(inquiry_block);
  dev->inquiry_analog_gamma  = get_inquiry_analog_gamma(inquiry_block);
  dev->inquiry_lineart_order = get_inquiry_lineart_order(inquiry_block);

  dev->inquiry_lens_cal_in_doc_pos  = get_inquiry_manual_focus(inquiry_block);
  dev->inquiry_manual_focus         = get_inquiry_manual_focus(inquiry_block);
  dev->inquiry_sel_uta_lens_cal_pos = get_inquiry_manual_focus(inquiry_block);

  dev->inquiry_gamma_dwload  = get_inquiry_gamma_download_available(inquiry_block);

  if (get_inquiry_gamma_type_2(inquiry_block) != 0)
  {
    dev->inquiry_gamma_DCF = 2;
  }

  dev->inquiry_GIB           = get_inquiry_gib(inquiry_block);
  dev->inquiry_GOB           = get_inquiry_gob(inquiry_block);
  dev->inquiry_color_order   = get_inquiry_color_order(inquiry_block);
  dev->inquiry_vidmem        = get_inquiry_max_vidmem(inquiry_block);

  /* optical resolution = [0x73] * 100 + [0x94] , 0x94 is not always defined */
  dev->inquiry_optical_res = 100 * get_inquiry_max_opt_res(inquiry_block);
  if (dev->inquiry_len > 0x94)
  {
    dev->inquiry_optical_res += get_inquiry_optical_resolution_residue(inquiry_block);
  }

  /* x resolution = [0x74] * 100 + [0x95] , 0x95 is not always defined */
  dev->inquiry_x_res = 100 * get_inquiry_max_x_res(inquiry_block);
  if (dev->inquiry_len > 0x95)
  {
    dev->inquiry_x_res+= get_inquiry_x_resolution_residue(inquiry_block);
  };

  /* y resolution = [0x75] * 100 + [0x96] , 0x96 is not always defined */
  dev->inquiry_y_res = 100 * get_inquiry_max_y_res(inquiry_block);
  if (dev->inquiry_len > 0x96)
  {
    dev->inquiry_y_res+= get_inquiry_y_resolution_residue(inquiry_block);
  }


  /* optical resolution = [0x83] * 100 + [0xa0] , 0xa0 is not always defined */
  dev->inquiry_dor_optical_res = 100 * get_inquiry_dor_max_opt_res(inquiry_block);
  if (dev->inquiry_len > 0xa0)
  {
    dev->inquiry_dor_optical_res += get_inquiry_dor_optical_resolution_residue(inquiry_block);
  }

  /* x resolution = [0x84] * 100 + [0xa1] , 0xa1 is not always defined */
  dev->inquiry_dor_x_res = 100 * get_inquiry_dor_max_x_res(inquiry_block);
  if (dev->inquiry_len > 0xa1)
  {
    dev->inquiry_dor_x_res+= get_inquiry_dor_x_resolution_residue(inquiry_block);
  }

  /* y resolution = [0x85] * 100 + [0xa2] , 0xa2 is not always defined */
  dev->inquiry_dor_y_res = 100 * get_inquiry_dor_max_y_res(inquiry_block);
  if (dev->inquiry_len > 0xa2)
  {
    dev->inquiry_dor_y_res+= get_inquiry_dor_y_resolution_residue(inquiry_block);
  }

  if (dev->inquiry_dor) /* DOR mode available ? */
  {
    /* if DOR resolutions are not defined, use double of standard resolution */

    if (dev->inquiry_dor_optical_res == 0)
    {
      dev->inquiry_dor_optical_res = dev->inquiry_optical_res * 2;
    }

    if (dev->inquiry_dor_x_res == 0)
    {
      dev->inquiry_dor_x_res = dev->inquiry_x_res * 2;
    }

    if (dev->inquiry_dor_y_res == 0)
    {
      dev->inquiry_dor_y_res = dev->inquiry_y_res * 2;
    }
  }

  dev->inquiry_fb_width   = (double)get_inquiry_fb_max_scan_width(inquiry_block)  * 0.01;
  dev->inquiry_fb_length  = (double)get_inquiry_fb_max_scan_length(inquiry_block) * 0.01;

  dev->inquiry_uta_width  = (double)get_inquiry_uta_max_scan_width(inquiry_block)  * 0.01;
  dev->inquiry_uta_length = (double)get_inquiry_uta_max_scan_length(inquiry_block) * 0.01;
  dev->inquiry_uta_x_off  = (double)get_inquiry_uta_x_original_point(inquiry_block) * 0.01;
  dev->inquiry_uta_y_off  = (double)get_inquiry_uta_y_original_point(inquiry_block) * 0.01;

  dev->inquiry_dor_width  = (double)get_inquiry_dor_max_scan_width(inquiry_block)  * 0.01;
  dev->inquiry_dor_length = (double)get_inquiry_dor_max_scan_length(inquiry_block) * 0.01;
  dev->inquiry_dor_x_off  = (double)get_inquiry_dor_x_original_point(inquiry_block) * 0.01;
  dev->inquiry_dor_y_off  = (double)get_inquiry_dor_y_original_point(inquiry_block) * 0.01;

  dev->inquiry_max_warmup_time          = get_inquiry_lamp_warmup_maximum_time(inquiry_block) * 2;

  dev->inquiry_wdb_len                  = get_inquiry_wdb_length(inquiry_block);

  /* it is not guaranteed that the following values are in the inquiry return block */

  /* 0x9a */
  if (dev->inquiry_len<=0x9a)
  {
    return;
  }
  dev->inquiry_max_calib_lines          = get_inquiry_max_calibration_data_lines(inquiry_block);

  /* 0x9b */
  if (dev->inquiry_len<=0x9b)
  {
    return;
  }
  dev->inquiry_fb_uta_color_arrangement = get_inquiry_fb_uta_line_arrangement_mode(inquiry_block);

  /* 0x9c */
  if (dev->inquiry_len<=0x9c)
  {
    return;
  }
  dev->inquiry_adf_color_arrangement    = get_inquiry_adf_line_arrangement_mode(inquiry_block);

  /* 0x9d */
  if (dev->inquiry_len<=0x9d)
  {
    return;
  } 
  dev->inquiry_CCD_line_distance        = get_inquiry_CCD_line_distance(inquiry_block);

  return;
}


/* ------------------------------------------------------------ UMAX CALCULATE ANALOG GAMMA ---------------- */


static int umax_calculate_analog_gamma(double value)
{
 int gamma;

  if (value < 1.0)
   { value=1.0; }

  if (value > 2.0)
   { value=2.0; }

  gamma=0;						       /* select gamma_value from analog_gamma_table */
  while (value>analog_gamma_table[gamma])
  {
    gamma++;
  } 

  if (gamma)
  {
    if ((analog_gamma_table[gamma-1] + analog_gamma_table[gamma]) /2 > value)
    {
      gamma--;
    }
  }
  
 return(gamma);
}

/* ------------------------------------------------------------ UMAX OUTPUT IMAGE DATA  -------------------- */

static void umax_output_image_data(Umax_Device *dev, FILE *fp, unsigned int data_to_read, int bufnr)
{
    if (dev->do_color_ordering == 0)							   /* pixel ordering */
    {
      if ((dev->inquiry_lineart_order) && (dev->colormode == LINEART)) /* lineart with LSB first */
      {
       unsigned int i, j;
       int new, old;

        for (i=0; i<data_to_read; i++)
        {
          old = dev->buffer[bufnr][i];    
          new = 0;
          for (j=0; j<8; j++)  /* reverse bit order of 1 byte */
          {
            new = (new << 1) + (old & 1);
            old = old >> 1;
          }
          dev->buffer[bufnr][i]=new;
        }
      }
      fwrite(dev->buffer[bufnr], 1, data_to_read, fp);
    }
    else										    /* line ordering */
    {
     unsigned char *linesource = dev->buffer[bufnr];
     unsigned char *pixelsource;
     int bytes = 1;
     int lines;
     int i;

      if (dev->bits_per_pixel_code != 1)							  /* >24 bpp */
      {
        bytes = 2;
      }

      lines = data_to_read / (dev->width_in_pixels * bytes);

      for(i=0; i<lines; i++)
      {
        umax_order_line(dev, linesource);
        linesource += dev->width_in_pixels * bytes;

        pixelsource = umax_get_pixel_line(dev);
        if (pixelsource != NULL)
        {
          fwrite(pixelsource, bytes, dev->width_in_pixels * 3, fp);
        }
      }
    }
}

/* ------------------------------------------------------------ UMAX READER PROCESS ------------------------ */


static int umax_reader_process(Umax_Device *dev, FILE *fp, unsigned int image_size)
{
 int status;
 int bytes        = 1;
 int queue_filled = 0;
 unsigned int bufnr_queue  = 0;
 unsigned int bufnr_read   = 0;
 unsigned int data_left_to_read  = image_size;
 unsigned int data_left_to_queue = image_size;
 unsigned int data_to_read;
 unsigned int data_to_queue;

  dev->row_bufsize = dev->bufsize;
  umax_trim_rowbufsize(dev);								     /* trim bufsize */

  if (dev->bits_per_pixel_code != 1) /* >24 bpp */
  {
    bytes = 2;
  }

  DBG(DBG_read,"reading %u bytes in blocks of %u bytes\n", image_size, dev->row_bufsize);

  if (dev->pixelbuffer != NULL)								   /* buffer exists? */
  {
    free(dev->pixelbuffer);
    dev->pixelbuffer = NULL;
  }

  if (dev->do_color_ordering != 0)
  {
    DBG(DBG_info,"ordering from line-order to pixel-order\n");

    dev->pixelline_max = 3 *  dev->CCD_distance * dev->scale_y + 2;

    dev->pixelbuffer = malloc(dev->width_in_pixels * dev->pixelline_max * bytes * 3);

    if (dev->pixelbuffer == NULL) /* NO MEMORY */
    {
      return -1;
    }
  }

  WAIT_SCANNER;

  do
  {
    if (data_left_to_queue)
    {
      data_to_queue = (data_left_to_queue < dev->row_bufsize) ? data_left_to_queue : dev->row_bufsize;

      /* umax_get_data_buffer_status(dev); */

      status = umax_queue_read_image_data_req(dev, data_to_queue, bufnr_queue);

      if (status == 0) /* no error but nothing queued */
      {
        continue;
      }

      if (status == -1) /* error */
      {
        DBG(DBG_error,"ERROR: umax_reader_process: unable to queue read image data request!\n");
        free(dev->pixelbuffer);
        dev->pixelbuffer = NULL;
        return(-1);
      }

      data_left_to_queue -= data_to_queue;
      DBG(DBG_read, "umax_reader_process: read image data queued for buffer[%d] \n", bufnr_queue);

      bufnr_queue++;
      if (bufnr_queue >= dev->scsi_maxqueue)
      {
        bufnr_queue = 0;
        queue_filled = 1; /* ok, we can start to read the queued buffers - if not already started */
      }

      if (!data_left_to_queue)
      {
        queue_filled = 1; /* ok, we can start to read the queued buffer(s) - all read requests are send */
      }
    }

    if (queue_filled) /* queue filled, ok we can read data */
    {
      status = umax_wait_queued_image_data(dev, bufnr_read);

      if (status == -1)
      {
        DBG(DBG_error,"ERROR: umax_reader_process: unable to get image data from scanner!\n");
        free(dev->pixelbuffer);
        dev->pixelbuffer = NULL;
        return(-1);
      }

      data_to_read = dev->length_read[bufnr_read]; /* number of bytes in buffer */
      umax_output_image_data(dev, fp, data_to_read, bufnr_read);

      data_left_to_read -= data_to_read;
      DBG(DBG_read, "umax_reader_process: buffer of %d bytes read; %d bytes to go\n", data_to_read, data_left_to_read);

      /* if we did not get all requested data increase data_left_to_queue so that we get all needed data */
      if (dev->length_read[bufnr_read] != dev->length_queued[bufnr_read])
      {
        data_left_to_queue += dev->length_queued[bufnr_read] - dev->length_read[bufnr_read];
      }

      bufnr_read++;
      if (bufnr_read >= dev->scsi_maxqueue)
      {
        bufnr_read = 0;
      }
    }
  } while (data_left_to_read);

  free(dev->pixelbuffer);
  dev->pixelbuffer = NULL;

 return 0;
}


/* ------------------------------------------------------------ UMAX INITIALIZE VALUES --------------------- */


static void umax_initialize_values(Umax_Device *dev)	      /* called each time before setting scan-values */
{										 /* Initialize dev structure */
  DBG(DBG_proc,"initialize_values\n");

  dev->three_pass            = 0;						  /* 1 if threepas_mode only */
  dev->row_len               = -1;
  dev->max_value             = 255;							    /* maximum value */

  dev->wdb_len               = 0;
  dev->width_in_pixels       = 0;						     /* scan width in pixels */
  dev->length_in_pixels      = 0;						    /* scan length in pixels */
  dev->scanwidth             = 0;			           /* width in inch at x_coordinate_base dpi */
  dev->scanlength            = 0;				  /* length in inch at y_coordinate_base dpi */
  dev->x_resolution          = 0;
  dev->y_resolution          = 0;
  dev->upper_left_x          = 0;							   /* at 1200pt/inch */
  dev->upper_left_y          = 0;							   /* at 1200pt/inch */
  dev->bytes_per_color       = 0;						     /* bytes for each color */

  dev->bits_per_pixel        = 8;						 /* number of bits per pixel */
  dev->bits_per_pixel_code   = 1;			    /* 1 =  8/24 bpp,  2 =  9/27 bpp,  4 = 10/30 bpp */
  dev->gamma_input_bits_code = 1;			    /* 8 = 12/36 bpp, 16 = 14/42 bpp, 32 = 16/48 bpp */
  dev->set_auto              = 0;								   /* 0 or 1 */
  dev->preview               = 0;							    /* 1 for preview */
  dev->quality               = 0;						      /* quality calibration */
  dev->warmup                = 0;							       /* warmup-bit */
  dev->fix_focus_position    = 0;						       /* fix focus position */
  dev->lens_cal_in_doc_pos   = 0;				    /* lens calibration in document position */
  dev->disable_pre_focus     = 0;							/* disable pre focus */
  dev->holder_focus_pos_0mm  = 0;				    /* 0.6mm <-> 0.0mm holder focus position */
  dev->manual_focus          = 0;					       /* automatic <-> manual focus */
  dev->colormode             = 0;				      /* LINEART, HALFTONE, GRAYSCALE or RGB */
  dev->adf                   = 0;						   /* 1 if adf shall be used */
  dev->uta                   = 0;						   /* 1 if uta shall be used */
  dev->module                = WD_module_flatbed;
  dev->cbhs_range            = WD_CBHS_255;
  dev->dor                   = 0;
  dev->halftone              = WD_halftone_8x8_1;
  dev->reverse               = 0;
  dev->reverse_multi         = 0;
  dev->calibration           = 0;

  dev->exposure_time_calibration_r = 0;						 /* use this for calibration */
  dev->exposure_time_calibration_g = 0;						 /* use this for calibration */
  dev->exposure_time_calibration_b = 0;						 /* use this for calibration */
  dev->exposure_time_scan_r        = 0;							/* use this for scan */
  dev->exposure_time_scan_g        = 0;							/* use this for scan */
  dev->exposure_time_scan_b        = 0;							/* use this for scan */

  dev->c_density           = WD_lamp_c_density_auto;				 /* calibration lamp density */
  dev->s_density           = WD_lamp_s_density_auto;				   /* next scan lamp density */

  dev->threshold           = 128;					       /* threshold for lineart mode */
  dev->brightness          = 128;					     /* brightness for halftone mode */
  dev->contrast            = 128;					       /* contrast for halftone mode */
  dev->highlight_r         = 255;						       /* highlight gray/red */
  dev->highlight_g         = 255;							  /* highlight green */
  dev->highlight_b         = 255;						  	   /* highlight blue */
  dev->shadow_r            = 0;							     	  /* shadow gray/red */
  dev->shadow_g            = 0;								     /* shadow green */
  dev->shadow_b            = 0;								      /* shadow blue */

  dev->digital_gamma_r     = WD_gamma_normal;
  dev->digital_gamma_g     = WD_gamma_normal;
  dev->digital_gamma_b     = WD_gamma_normal;

  dev->analog_gamma_r      = 0;					     /* analog gamma for red and gray to 1.0 */
  dev->analog_gamma_g      = 0;						    /* analog gamma for green to 1.0 */
  dev->analog_gamma_b      = 0;						     /* analog gamma for blue to 1.0 */


  dev->pixelline_ready[0]  = 0;					      /* reset all values for color ordering */
  dev->pixelline_ready[1]  = 0;
  dev->pixelline_ready[2]  = 0;
  dev->pixelline_next[0]   = 0;
  dev->pixelline_next[1]   = 0;
  dev->pixelline_next[2]   = 0;
  dev->pixelline_del[0]    = 1;
  dev->pixelline_del[1]    = 1;
  dev->pixelline_del[2]    = 1;
  dev->pixelline_optic[0]  = 1;
  dev->pixelline_optic[1]  = 1;
  dev->pixelline_optic[2]  = 1;
  dev->pixelline_max       = 0;        
  dev->pixelline_opt_res   = 0;        
  dev->pixelline_read      = 0;        
  dev->pixelline_written   = 0;        
  dev->CCD_distance        = 0;        

  dev->calib_lines         = 0;							/* request calibration lines */
  dev->do_calibration      = 0;							 /* no calibration by driver */
  dev->do_color_ordering   = 0;						  /* no line- to pixel-mode ordering */

  dev->button0_pressed     = 0;						      /* reset button 0 pressed flag */
  dev->button1_pressed     = 0;						      /* reset button 1 pressed flag */
  dev->button2_pressed     = 0;						      /* reset button 2 pressed flag */
}


/* ------------------------------------------------------------ UMAX INIT ---------------------------------- */


static void umax_init(Umax_Device *dev)		     /* umax_init is called once while driver-initialization */
{
  DBG(DBG_proc,"init\n");

  dev->devicename  = NULL;
  dev->pixelbuffer = NULL;

  /* config file or predefined settings */
  if (dev->connection_type == SANE_UMAX_SCSI)
  {
    dev->request_scsi_maxqueue  = umax_scsi_maxqueue;
  }
  else /* SANE_UMAX_USB, USB does not support command queueing */
  {
    DBG(DBG_info2, "setting request_scsi_maxqueue = 1 for USB connection\n");
    dev->request_scsi_maxqueue  = 1;	  
  }

  dev->request_preview_lines          = umax_preview_lines;
  dev->request_scan_lines             = umax_scan_lines;
  dev->handle_bad_sense_error         = umax_handle_bad_sense_error;
  dev->execute_request_sense          = umax_execute_request_sense;
  dev->scsi_buffer_size_min           = umax_scsi_buffer_size_min;
  dev->scsi_buffer_size_max           = umax_scsi_buffer_size_max;
  dev->force_preview_bit_rgb          = umax_force_preview_bit_rgb;
  dev->slow                           = umax_slow;
  dev->smear                          = umax_smear;
  dev->calibration_area               = umax_calibration_area;
  dev->calibration_width_offset       = umax_calibration_width_offset;
  dev->calibration_width_offset_batch = umax_calibration_width_offset_batch;
  dev->calibration_bytespp            = umax_calibration_bytespp;
  dev->exposure_time_rgb_bind         = umax_exposure_time_rgb_bind;
  dev->invert_shading_data            = umax_invert_shading_data;
  dev->lamp_control_available         = umax_lamp_control_available;
  dev->gamma_lsb_padded               = umax_gamma_lsb_padded;

  DBG(DBG_info, "request_scsi_maxqueue          = %d\n", dev->request_scsi_maxqueue);
  DBG(DBG_info, "request_preview_lines          = %d\n", dev->request_preview_lines);
  DBG(DBG_info, "request_scan_lines             = %d\n", dev->request_scan_lines);
  DBG(DBG_info, "handle_bad_sense_error         = %d\n", dev->handle_bad_sense_error);
  DBG(DBG_info, "execute_request_sense          = %d\n", dev->execute_request_sense);
  DBG(DBG_info, "scsi_buffer_size_min           = %d\n", dev->scsi_buffer_size_min);
  DBG(DBG_info, "scsi_buffer_size_max           = %d\n", dev->scsi_buffer_size_max);
  DBG(DBG_info, "force_preview_bit_rgb          = %d\n", dev->force_preview_bit_rgb);
  DBG(DBG_info, "slow                           = %d\n", dev->slow);
  DBG(DBG_info, "smear                          = %d\n", dev->smear);
  DBG(DBG_info, "calibration_area               = %d\n", dev->calibration_area);
  DBG(DBG_info, "calibration_width_offset       = %d\n", dev->calibration_width_offset);
  DBG(DBG_info, "calibration_width_offset_batch = %d\n", dev->calibration_width_offset_batch);
  DBG(DBG_info, "calibration_bytespp            = %d\n", dev->calibration_bytespp);
  DBG(DBG_info, "exposure_time_rgb_bind         = %d\n", dev->exposure_time_rgb_bind);
  DBG(DBG_info, "invert_shading_data            = %d\n", dev->invert_shading_data);
  DBG(DBG_info, "lamp_control_available         = %d\n", dev->lamp_control_available);


  dev->inquiry_len                       = 0;
  dev->inquiry_wdb_len                   = -1;
  dev->inquiry_optical_res               = -1;
  dev->inquiry_x_res                     = -1;
  dev->inquiry_y_res                     = -1;
  dev->inquiry_fb_width                  = -1;
  dev->inquiry_fb_length                 = -1;
  dev->inquiry_uta_width                 = -1;
  dev->inquiry_uta_length                = -1;
  dev->inquiry_dor_width                 = -1;
  dev->inquiry_dor_length                = -1;
  dev->inquiry_exposure_adj              = 0;
  dev->inquiry_exposure_time_step_unit   = -1;				  /* exposure time unit in micro sec */
  dev->inquiry_exposure_time_max         = -1;					    /* exposure time maximum */
  dev->inquiry_exposure_time_l_min       = -1;			       /*  exposure time minimum for lineart */
  dev->inquiry_exposure_time_l_fb_def    = -1;			/* exposure time default for lineart flatbed */
  dev->inquiry_exposure_time_l_uta_def   = -1;			    /* exposure time default for lineart uta */
  dev->inquiry_exposure_time_h_min       = -1;			      /*  exposure time minimum for halftone */
  dev->inquiry_exposure_time_h_fb_def    = -1;		       /* exposure time default for halftone flatbed */
  dev->inquiry_exposure_time_h_uta_def   = -1;			   /* exposure time default for halftone uta */
  dev->inquiry_exposure_time_g_min       = -1;			     /*  exposure time minimum for grayscale */
  dev->inquiry_exposure_time_g_fb_def    = -1;		      /* exposure time default for grayscale flatbed */
  dev->inquiry_exposure_time_g_uta_def   = -1;			  /* exposure time default for grayscale uta */
  dev->inquiry_exposure_time_c_min       = -1;				  /* exposure time minimum for color */
  dev->inquiry_exposure_time_c_fb_def_r  = -1;		      /* exposure time default for color flatbed red */
  dev->inquiry_exposure_time_c_fb_def_g  = -1;		    /* exposure time default for color flatbed green */
  dev->inquiry_exposure_time_c_fb_def_b  = -1;		     /* exposure time default for color flatbed blue */
  dev->inquiry_exposure_time_c_uta_def_r = -1;			  /* exposure time default for color uta red */
  dev->inquiry_exposure_time_c_uta_def_g = -1;			/* exposure time default for color uta green */
  dev->inquiry_exposure_time_c_uta_def_b = -1;			 /* exposure time default for color uta blue */
  dev->inquiry_max_warmup_time           = 0;					      /* maximum warmup time */
  dev->inquiry_cbhs                      = WD_CBHS_255;
  dev->inquiry_contrast_min              = 1;					      /* minimum value for c */
  dev->inquiry_contrast_max              = 255;					      /* maximum value for c */
  dev->inquiry_brightness_min            = 1;					      /* minimum value for b */
  dev->inquiry_brightness_max            = 255;					      /* maximum value for b */
  dev->inquiry_threshold_min             = 1;					      /* minimum value for t */
  dev->inquiry_threshold_max             = 255;					      /* maximum value for t */
  dev->inquiry_highlight_min             = 1;					      /* minimum value for h */
  dev->inquiry_highlight_max             = 255;					      /* maximum value for h */
  dev->inquiry_shadow_min                = 0;					      /* minimum value for s */
  dev->inquiry_shadow_max                = 254;					      /* maximum value for s */ 
  dev->inquiry_quality_ctrl              = 0;
  dev->inquiry_preview                   = 0;
  dev->inquiry_lamp_ctrl                 = 0;
  dev->inquiry_transavail                = 0;
  dev->inquiry_uta                       = 0;
  dev->inquiry_adfmode                   = 0;
  dev->inquiry_adf                       = 0;
  dev->inquiry_dor                       = 0;
  dev->inquiry_reverse                   = 0;
  dev->inquiry_reverse_multi             = 0;
  dev->inquiry_analog_gamma              = 0;
  dev->inquiry_gamma_dwload              = 0;
  dev->inquiry_one_pass_color            = 0;
  dev->inquiry_three_pass_color          = 0;
  dev->inquiry_color                     = 0;
  dev->inquiry_gray                      = 0;
  dev->inquiry_halftone                  = 0;
  dev->inquiry_lineart                   = 0;
  dev->inquiry_calibration               = 1;
  dev->inquiry_shadow                    = 0;
  dev->inquiry_highlight                 = 0;
  dev->inquiry_gamma_DCF                 = -1;
  dev->inquiry_max_calib_lines           = 66;	 /* most scanners use 66 lines, so lets define it as default */

  dev->common_xy_resolutions             = 0;

  dev->x_coordinate_base = 1200;						/* these are the 1200pt/inch */
  dev->y_coordinate_base = 1200;						/* these are the 1200pt/inch */

  dev->button0_pressed   = 0;						      /* reset button 0 pressed flag */
  dev->button1_pressed   = 0;						      /* reset button 1 pressed flag */
  dev->button2_pressed   = 0;						      /* reset button 2 pressed flag */

  dev->pause_for_color_calibration = 0;			/* pause between start_scan and do_calibration in ms */
  dev->pause_for_gray_calibration  = 0;			/* pause between start_scan and do_calibration in ms */
  dev->pause_after_calibration     = 0;			 /* pause between do_calibration and read data in ms */
  dev->pause_after_reposition      = -1;	    /* pause after repostion scanner in ms, -1 = do not wait */
  dev->pause_for_moving            = 0;			         /* pause for moving scanhead over full area */

  if (umax_test_little_endian() == SANE_TRUE)
  {
    dev->low_byte_first = 1;					        /* in 2 byte mode send lowbyte first */
    DBG(DBG_info, "backend runs on little endian machine\n");
  }
  else
  {
    dev->low_byte_first = 0;					       /* in 2 byte mode send highbyte first */
    DBG(DBG_info, "backend runs on big endian machine\n");
  }

#ifdef HAVE_SANEI_SCSI_OPEN_EXTENDED
  DBG(DBG_info,"variable scsi buffer size (usage of sanei_scsi_open_extended)\n");
#else
  DBG(DBG_info,"fixed scsi buffer size = %d bytes\n", sanei_scsi_max_request_size);
#endif
}


/* ------------------------------------------------------------ MAX STRING SIZE ---------------------------- */


static size_t max_string_size(SANE_String_Const strings[])
{
 size_t size, max_size = 0;
 int i;

  for (i = 0; strings[i]; ++i)
  {
    size = strlen (strings[i]) + 1;
    if (size > max_size)
    {
      max_size = size;
    }
  }

 return max_size;
}


/* ------------------------------------------------------------ DO CANCEL ---------------------------------- */


static SANE_Status do_cancel(Umax_Scanner *scanner)
{
  SANE_Pid pid;
  int status;

  DBG(DBG_sane_proc,"do_cancel\n");

  scanner->scanning = SANE_FALSE;

  if (scanner->reader_pid != -1)
  {
    DBG(DBG_sane_info,"killing reader_process\n");

    sanei_thread_kill(scanner->reader_pid);
    pid = sanei_thread_waitpid(scanner->reader_pid, &status);

    if (pid == -1)
    {
      DBG(DBG_sane_info, "do_cancel: sanei_thread_waitpid failed, already terminated ? (%s)\n", strerror(errno));
    }
    else
    {
      DBG(DBG_sane_info, "do_cancel: reader_process terminated with status: %s\n", sane_strstatus(status));
    }

    scanner->reader_pid = -1;

    if (scanner->device->pixelbuffer != NULL)					      /* pixelbuffer exists? */
    {
      free(scanner->device->pixelbuffer);						 /* free pixelbuffer */
      scanner->device->pixelbuffer = NULL;
    }
  }

  sanei_scsi_req_flush_all(); /* flush SCSI queue, when we do not do this then sanei_scsi crashes next time */

  if (scanner->device->sfd != -1) /* make sure we have a working filedescriptor */
  {
    umax_give_scanner(scanner->device); /* reposition and release scanner */
    DBG(DBG_sane_info,"closing scannerdevice filedescriptor\n");
    umax_scsi_close(scanner->device);
  }

  scanner->device->three_pass_color = 1; /* reset color in color scanning */

 return SANE_STATUS_CANCELLED;
}


/* ------------------------------------------------------------ ATTACH SCANNER ----------------------------- */


static SANE_Status attach_scanner(const char *devicename, Umax_Device **devp, int connection_type)
{
 Umax_Device *dev;
 int i;

  DBG(DBG_sane_proc,"attach_scanner: %s, connection_type %d\n", devicename, connection_type);

  for (dev = first_dev; dev; dev = dev->next) /* search is scanner already is listed in devicelist */
  {
    if (strcmp(dev->sane.name, devicename) == 0) /* scanner is already listed */
    {
      if (devp)
      {
        *devp = dev; /* return pointer to device */
      }
     return SANE_STATUS_GOOD;
    }
  }

  /* scanner has not been attached yet */

  dev = malloc( sizeof(*dev) );
  if (!dev)
  {
     return SANE_STATUS_NO_MEM;
  }
  memset(dev, '\0', sizeof(Umax_Device)); /* clear structure */

  /* If connection type is not known (==0) then try to open the device as an USB device. */
  /* If it fails, try the SCSI method. */

#ifdef UMAX_ENABLE_USB
  dev->connection_type = connection_type; /* 0 = unknown, 1=scsi, 2=usb */

  if (dev->connection_type != SANE_UMAX_SCSI)
  {
    dev->bufsize = 16384; /* 16KB */
    DBG(DBG_info, "attach_scanner: opening usb device %s\n", devicename);

    if (sanei_umaxusb_open(devicename, &dev->sfd, sense_handler, dev) == SANE_STATUS_GOOD)
    {
      dev->connection_type = SANE_UMAX_USB;
    }
    else /* opening usb device failed */
    {
      if (dev->connection_type == SANE_UMAX_USB) /* we know it is not a scsi device: error */
      {
        DBG(DBG_error, "ERROR: attach_scanner: opening usb device %s failed\n", devicename);
        free(dev);
       return SANE_STATUS_INVAL;
      }

      DBG(DBG_info, "attach_scanner: failed to open %s as usb device\n", devicename);
    }
  }
#else
  dev->connection_type = SANE_UMAX_SCSI;
#endif

  if (dev->connection_type != SANE_UMAX_USB) /* not an USB device, then try as SCSI */
  {
#ifdef HAVE_SANEI_SCSI_OPEN_EXTENDED
    dev->bufsize = 16384; /* 16KB */
    DBG(DBG_info, "attach_scanner: opening scsi device %s\n", devicename);

    if (sanei_scsi_open_extended(devicename, &dev->sfd, sense_handler, dev, (int *) &dev->bufsize) != 0)
    {
      DBG(DBG_error, "ERROR: attach_scanner: opening scsi device %s failed\n", devicename);
      free(dev);
     return SANE_STATUS_INVAL;
    }

    if (dev->bufsize < 4096) /* < 4KB */
    {
      DBG(DBG_error, "ERROR: attach_scanner: sanei_scsi_open_extended returned too small scsi buffer\n");
      umax_scsi_close(dev);
      free(dev);
     return SANE_STATUS_NO_MEM;
    }

    DBG(DBG_info, "attach_scanner: sanei_scsi_open_extended returned scsi buffer size = %d\n", dev->bufsize);
#else
    dev->bufsize = sanei_scsi_max_request_size;

    if (sanei_scsi_open(devicename, dev, sense_handler, dev) != 0)
    {
      DBG(DBG_error, "ERROR: attach_scanner: opening scsi device %s failed\n", devicename);
      free(dev);
     return SANE_STATUS_INVAL;
    }
#endif
    dev->connection_type = SANE_UMAX_SCSI; /* set connection type (may have been unknown == 0) */
  }

  DBG(DBG_info, "attach_scanner: allocating SCSI buffer[0]\n");
  dev->buffer[0] = malloc(dev->bufsize);								/* allocate buffer */

  for (i=1; i<SANE_UMAX_SCSI_MAXQUEUE; i++)
  {
    dev->buffer[i] = NULL;
  }

  if (!dev->buffer[0]) /* malloc failed */
  {
    DBG(DBG_error, "ERROR: attach scanner: could not allocate buffer[0]\n");
    umax_scsi_close(dev);
    free(dev);
    return SANE_STATUS_NO_MEM;
  }

  dev->scsi_maxqueue = 1; /* only one buffer outside the reader process */

  umax_init(dev);									 /* preset values in structure dev */
  umax_initialize_values(dev);										   /* reset values */

  dev->devicename = strdup(devicename);

  if (umax_identify_scanner(dev) != 0)
  {
    DBG(DBG_error, "ERROR: attach_scanner: scanner-identification failed\n");
    umax_scsi_close(dev);
    free(dev->buffer[0]);
    free(dev);
    return SANE_STATUS_INVAL;
  }

  if (dev->slow == -1) /* option is not predefined in umax.conf and not by backend */
  {
    dev->slow = 0;
  }

  if (dev->smear == -1) /* option is not predefined in umax.conf and not by backend */
  {
    dev->smear = 0;
  }

  if (dev->invert_shading_data == -1) /* nothing defined in umax.conf and not by backend */
  {
    dev->invert_shading_data = 0;
  }

  if (dev->gamma_lsb_padded == -1) /* nothing defined in umax.conf and not by backend */
  {
    dev->gamma_lsb_padded = 0;
  }

  umax_get_inquiry_values(dev);
  umax_print_inquiry(dev);
  DBG(DBG_inquiry,"\n");
  DBG(DBG_inquiry,"==================== end of inquiry ====================\n");
  DBG(DBG_inquiry,"\n");

  umax_scsi_close(dev);

  dev->sane.name   = dev->devicename;
  dev->sane.vendor = dev->vendor;
  dev->sane.model  = dev->product;
  dev->sane.type   = "flatbed scanner"; 

  if (strcmp(dev->sane.model,"PSD ") == 0)
  {
    dev->sane.type = "page scanner";
  }

  dev->x_range.min               = SANE_FIX(0);
  dev->x_range.quant             = SANE_FIX(0);
  dev->x_range.max               = SANE_FIX(dev->inquiry_fb_width  * MM_PER_INCH);

  dev->y_range.min               = SANE_FIX(0);
  dev->y_range.quant             = SANE_FIX(0);
  dev->y_range.max               = SANE_FIX(dev->inquiry_fb_length * MM_PER_INCH);

#if UMAX_RESOLUTION_PERCENT_STEP
  dev->x_dpi_range.min           = SANE_FIX(dev->inquiry_optical_res/100);
  dev->x_dpi_range.quant         = SANE_FIX(dev->inquiry_optical_res/100);
#else
  dev->x_dpi_range.min           = SANE_FIX(5);
  dev->x_dpi_range.quant         = SANE_FIX(5);
#endif
  dev->x_dpi_range.max           = SANE_FIX(dev->inquiry_x_res);

#if UMAX_RESOLUTION_PERCENT_STEP
  dev->y_dpi_range.min           = SANE_FIX(dev->inquiry_optical_res/100);
  dev->y_dpi_range.quant         = SANE_FIX(dev->inquiry_optical_res/100);
#else
  dev->y_dpi_range.min           = SANE_FIX(5);
  dev->y_dpi_range.quant         = SANE_FIX(5);
#endif
  dev->y_dpi_range.max           = SANE_FIX(dev->inquiry_y_res);

  dev->analog_gamma_range.min    = SANE_FIX(1.0);
  dev->analog_gamma_range.quant  = SANE_FIX(0.01);
  dev->analog_gamma_range.max    = SANE_FIX(2.0);

  DBG(DBG_info,"x_range.max     = %f\n", SANE_UNFIX(dev->x_range.max));
  DBG(DBG_info,"y_range.max     = %f\n", SANE_UNFIX(dev->y_range.max));
  DBG(DBG_info,"x_dpi_range.max = %f\n", SANE_UNFIX(dev->x_dpi_range.max));
  DBG(DBG_info,"y_dpi_range.max = %f\n", SANE_UNFIX(dev->y_dpi_range.max));

  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
  {
    *devp = dev;
  }

 return SANE_STATUS_GOOD;
}


/* ------------------------------------------------------------ READER PROCESS SIGTERM HANDLER  ------------ */


static RETSIGTYPE reader_process_sigterm_handler(int signal)
{
  DBG(DBG_sane_info,"reader_process: terminated by signal %d\n", signal);

  sanei_scsi_req_flush_all(); /* flush SCSI queue */

  _exit (SANE_STATUS_GOOD);
}


/* ------------------------------------------------------------ READER PROCESS ----------------------------- */


static int reader_process(void *data) /* executed as a child process or as thread */
{
 Umax_Scanner *scanner = (Umax_Scanner *)data;
 FILE *fp;
 int status;
 unsigned int data_length;
 struct SIGACTION act;
 unsigned int i;

  if (sanei_thread_is_forked())
  {
    DBG(DBG_sane_proc,"reader_process started (forked)\n");
    close(scanner->pipe_read_fd);
    scanner->pipe_read_fd = -1;

    /* sanei_scsi crashes when the scsi commands are not flushed, done in reader_process_sigterm_handler */
    memset(&act, 0, sizeof (act));						   /* define SIGTERM-handler */
    act.sa_handler = reader_process_sigterm_handler;
    sigaction(SIGTERM, &act, 0);
  }
  else
  {
    DBG(DBG_sane_proc,"reader_process started (as thread)\n");
  }


  scanner->device->scsi_maxqueue = scanner->device->request_scsi_maxqueue;

  if (scanner->device->request_scsi_maxqueue > 1)
  {
    for (i = 1; i<SANE_UMAX_SCSI_MAXQUEUE; i++)
    {
      if (scanner->device->buffer[i])
      {
        DBG(DBG_info, "reader_process: freeing SCSI buffer[%d]\n", i);
        free(scanner->device->buffer[i]);									     /* free buffer */
        scanner->device->buffer[i] = NULL;
      }
    }

    for (i = 1; i<scanner->device->request_scsi_maxqueue; i++)
    {
      DBG(DBG_info, "reader_process: allocating SCSI buffer[%d]\n", i);
      scanner->device->buffer[i]  = malloc(scanner->device->bufsize);			  /* allocate buffer */

      if (!scanner->device->buffer[i]) /* malloc failed */
      {
        DBG(DBG_warning, "WARNING: reader_process: only allocated %d/%d scsi buffers\n", i, scanner->device->request_scsi_maxqueue);
        scanner->device->scsi_maxqueue = i;
        break; /* leave for loop */
      }
    }
  }

  data_length = scanner->params.lines * scanner->params.bytes_per_line;

  fp = fdopen(scanner->pipe_write_fd, "w");
  if (!fp)
  {
    return SANE_STATUS_IO_ERROR;
  } 

  DBG(DBG_sane_info,"reader_process: starting to READ data\n");

  status = umax_reader_process(scanner->device, fp, data_length);
  fclose(fp); /* close write end of pipe */

  for (i = 1; i<scanner->device->request_scsi_maxqueue; i++)
  {
    if (scanner->device->buffer[i])
    {
      DBG(DBG_info, "reader_process: freeing SCSI buffer[%d]\n", i);
      free(scanner->device->buffer[i]);									     /* free buffer */
      scanner->device->buffer[i] = NULL;
    }
  }
  DBG(DBG_sane_info,"reader_process: finished reading data\n");

 return status;
}


/* ------------------------------------------------------------ INIT OPTIONS ------------------------------- */


static SANE_Status init_options(Umax_Scanner *scanner)
{
 int i;
 int scan_modes;
 int bit_depths;

  DBG(DBG_sane_proc,"init_options\n");

  memset(scanner->opt, 0, sizeof (scanner->opt));
  memset(scanner->val, 0, sizeof (scanner->val));

  for (i = 0; i < NUM_OPTIONS; ++i)
  {
    scanner->opt[i].size = sizeof (SANE_Word);
    scanner->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }

  scanner->opt[OPT_NUM_OPTS].name  = SANE_NAME_NUM_OPTIONS; /* empty string */
  scanner->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  scanner->opt[OPT_NUM_OPTS].desc  = SANE_DESC_NUM_OPTIONS;
  scanner->opt[OPT_NUM_OPTS].type  = SANE_TYPE_INT;
  scanner->opt[OPT_NUM_OPTS].cap   = SANE_CAP_SOFT_DETECT;
  scanner->val[OPT_NUM_OPTS].w     = NUM_OPTIONS;

  /* "Mode" group: */
  scanner->opt[OPT_MODE_GROUP].title = SANE_I18N("Scan Mode");
  scanner->opt[OPT_MODE_GROUP].desc  = "";
  scanner->opt[OPT_MODE_GROUP].type  = SANE_TYPE_GROUP;
  scanner->opt[OPT_MODE_GROUP].cap   = 0;
  scanner->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  scan_modes = -1;

  if (scanner->device->inquiry_lineart)
  {
    scan_mode_list[++scan_modes] = LINEART_STR;
  }

  if (scanner->device->inquiry_halftone)
  {
    scan_mode_list[++scan_modes]= HALFTONE_STR;
  }

  if (scanner->device->inquiry_gray)
  {
    scan_mode_list[++scan_modes]= GRAY_STR;
  }

  if (scanner->device->inquiry_color)
  {
/*  
    if (scanner->device->inquiry_lineart)
    { scan_mode_list[++scan_modes]= COLOR_LINEART_STR; }

    if (scanner->device->inquiry_halftone) 
    { scan_mode_list[++scan_modes]= COLOR_HALFTONE_STR; }
*/
    scan_mode_list[++scan_modes]= COLOR_STR; 
  }

  scan_mode_list[scan_modes + 1] = 0; 

  {
   int i=0;
    source_list[i++]= FLB_STR;

    if (scanner->device->inquiry_adfmode)
    {
      source_list[i++] = ADF_STR;
    }

    if (scanner->device->inquiry_transavail)
    {
      source_list[i++] = UTA_STR;
    }

    source_list[i] = 0;
  }
  
  /* scan mode */
  scanner->opt[OPT_MODE].name  = SANE_NAME_SCAN_MODE;
  scanner->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  scanner->opt[OPT_MODE].desc  = SANE_DESC_SCAN_MODE;
  scanner->opt[OPT_MODE].type  = SANE_TYPE_STRING;
  scanner->opt[OPT_MODE].size  = max_string_size((SANE_String_Const *) scan_mode_list);
  scanner->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_MODE].constraint.string_list = (SANE_String_Const *) scan_mode_list;
  scanner->val[OPT_MODE].s     = (SANE_Char*)strdup(scan_mode_list[0]);

  /* source */
  scanner->opt[OPT_SOURCE].name  = SANE_NAME_SCAN_SOURCE;
  scanner->opt[OPT_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
  scanner->opt[OPT_SOURCE].desc  = SANE_DESC_SCAN_SOURCE;
  scanner->opt[OPT_SOURCE].type  = SANE_TYPE_STRING;
  scanner->opt[OPT_SOURCE].size  = max_string_size(source_list);
  scanner->opt[OPT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_SOURCE].constraint.string_list = source_list;
  scanner->val[OPT_SOURCE].s     = (SANE_Char*)strdup(source_list[0]);
  
  /* x-resolution */
  scanner->opt[OPT_X_RESOLUTION].name  = SANE_NAME_SCAN_RESOLUTION;
  scanner->opt[OPT_X_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  scanner->opt[OPT_X_RESOLUTION].desc  = SANE_DESC_SCAN_RESOLUTION;
  scanner->opt[OPT_X_RESOLUTION].type  = SANE_TYPE_FIXED;
  scanner->opt[OPT_X_RESOLUTION].unit  = SANE_UNIT_DPI;
  scanner->opt[OPT_X_RESOLUTION].constraint_type  = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_X_RESOLUTION].constraint.range = &scanner->device->x_dpi_range;
  scanner->val[OPT_X_RESOLUTION].w     = 100 << SANE_FIXED_SCALE_SHIFT;

  /* y-resolution */
  scanner->opt[OPT_Y_RESOLUTION].name  = SANE_NAME_SCAN_Y_RESOLUTION;
  scanner->opt[OPT_Y_RESOLUTION].title = SANE_TITLE_SCAN_Y_RESOLUTION;
  scanner->opt[OPT_Y_RESOLUTION].desc  = SANE_DESC_SCAN_Y_RESOLUTION;
  scanner->opt[OPT_Y_RESOLUTION].type  = SANE_TYPE_FIXED;
  scanner->opt[OPT_Y_RESOLUTION].unit  = SANE_UNIT_DPI;
  scanner->opt[OPT_Y_RESOLUTION].constraint_type  = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_Y_RESOLUTION].constraint.range = &scanner->device->y_dpi_range;
  scanner->val[OPT_Y_RESOLUTION].w     = 100 << SANE_FIXED_SCALE_SHIFT;
  scanner->opt[OPT_Y_RESOLUTION].cap  |= SANE_CAP_INACTIVE;

  /* bind resolution */
  scanner->opt[OPT_RESOLUTION_BIND].name  = SANE_NAME_RESOLUTION_BIND;
  scanner->opt[OPT_RESOLUTION_BIND].title = SANE_TITLE_RESOLUTION_BIND;
  scanner->opt[OPT_RESOLUTION_BIND].desc  = SANE_DESC_RESOLUTION_BIND;
  scanner->opt[OPT_RESOLUTION_BIND].type  = SANE_TYPE_BOOL;
  scanner->val[OPT_RESOLUTION_BIND].w     = SANE_TRUE;
  if (scanner->device->common_xy_resolutions)	/* disable bind if x and y res have to be the same */
  {
    scanner->opt[OPT_RESOLUTION_BIND].cap  |= SANE_CAP_INACTIVE;
  }


  /* negative */
  scanner->opt[OPT_NEGATIVE].name  = SANE_NAME_NEGATIVE;
  scanner->opt[OPT_NEGATIVE].title = SANE_TITLE_NEGATIVE;
  scanner->opt[OPT_NEGATIVE].desc  = SANE_DESC_NEGATIVE;
  scanner->opt[OPT_NEGATIVE].type  = SANE_TYPE_BOOL;
  scanner->val[OPT_NEGATIVE].w     = SANE_FALSE;

  if (scanner->device->inquiry_reverse_multi == 0)
  {
    scanner->opt[OPT_NEGATIVE].cap  |= SANE_CAP_INACTIVE;
  }

  /* ------------------------------ */

  /* "Geometry" group: */
  scanner->opt[OPT_GEOMETRY_GROUP].title = SANE_I18N("Geometry");
  scanner->opt[OPT_GEOMETRY_GROUP].desc  = "";
  scanner->opt[OPT_GEOMETRY_GROUP].type  = SANE_TYPE_GROUP;
  scanner->opt[OPT_GEOMETRY_GROUP].cap   = SANE_CAP_ADVANCED;
  scanner->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* top-left x */
  scanner->opt[OPT_TL_X].name  = SANE_NAME_SCAN_TL_X;
  scanner->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  scanner->opt[OPT_TL_X].desc  = SANE_DESC_SCAN_TL_X;
  scanner->opt[OPT_TL_X].type  = SANE_TYPE_FIXED;
  scanner->opt[OPT_TL_X].unit  = SANE_UNIT_MM;
  scanner->opt[OPT_TL_X].constraint_type  = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_TL_X].constraint.range = &(scanner->device->x_range);
  scanner->val[OPT_TL_X].w     = 0;

  /* top-left y */
  scanner->opt[OPT_TL_Y].name  = SANE_NAME_SCAN_TL_Y;
  scanner->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  scanner->opt[OPT_TL_Y].desc  = SANE_DESC_SCAN_TL_Y;
  scanner->opt[OPT_TL_Y].type  = SANE_TYPE_FIXED;
  scanner->opt[OPT_TL_Y].unit  = SANE_UNIT_MM;
  scanner->opt[OPT_TL_Y].constraint_type  = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_TL_Y].constraint.range = &(scanner->device->y_range);
  scanner->val[OPT_TL_Y].w     = 0;

  /* bottom-right x */
  scanner->opt[OPT_BR_X].name  = SANE_NAME_SCAN_BR_X;
  scanner->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  scanner->opt[OPT_BR_X].desc  = SANE_DESC_SCAN_BR_X;
  scanner->opt[OPT_BR_X].type  = SANE_TYPE_FIXED;
  scanner->opt[OPT_BR_X].unit  = SANE_UNIT_MM;
  scanner->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_BR_X].constraint.range = &(scanner->device->x_range);
  scanner->val[OPT_BR_X].w     = scanner->device->x_range.max;

  /* bottom-right y */
  scanner->opt[OPT_BR_Y].name  = SANE_NAME_SCAN_BR_Y;
  scanner->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  scanner->opt[OPT_BR_Y].desc  = SANE_DESC_SCAN_BR_Y;
  scanner->opt[OPT_BR_Y].type  = SANE_TYPE_FIXED;
  scanner->opt[OPT_BR_Y].unit  = SANE_UNIT_MM;
  scanner->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_BR_Y].constraint.range = &(scanner->device->y_range);
  scanner->val[OPT_BR_Y].w     = scanner->device->y_range.max;

  /* ------------------------------ */


  /* "Enhancement" group: */
  scanner->opt[OPT_ENHANCEMENT_GROUP].title = SANE_I18N("Enhancement");
  scanner->opt[OPT_ENHANCEMENT_GROUP].desc  = "";
  scanner->opt[OPT_ENHANCEMENT_GROUP].type  = SANE_TYPE_GROUP;
  scanner->opt[OPT_ENHANCEMENT_GROUP].cap   = 0;
  scanner->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;


  /* bit depth */
  bit_depths = 0;

  if (scanner->device->inquiry_GOB & 1)
  {
    bit_depth_list[++bit_depths] = 8;
  }

  if (scanner->device->inquiry_GOB & 2)
  {
    bit_depth_list[++bit_depths] = 9;
  }

  if (scanner->device->inquiry_GOB & 4)
  {
    bit_depth_list[++bit_depths] = 10;
  }

  if (scanner->device->inquiry_GOB & 8)
  {
    bit_depth_list[++bit_depths] = 12;
  }

  if (scanner->device->inquiry_GOB & 16)
  {
    bit_depth_list[++bit_depths] = 14;
  }

  if (scanner->device->inquiry_GOB & 32)
  {
    bit_depth_list[++bit_depths] = 16;
  }

  bit_depth_list[0] = bit_depths;

  scanner->opt[OPT_BIT_DEPTH].name  = SANE_NAME_BIT_DEPTH;
  scanner->opt[OPT_BIT_DEPTH].title = SANE_TITLE_BIT_DEPTH;
  scanner->opt[OPT_BIT_DEPTH].desc  = SANE_DESC_BIT_DEPTH;
  scanner->opt[OPT_BIT_DEPTH].type  = SANE_TYPE_INT;
  scanner->opt[OPT_BIT_DEPTH].unit  = SANE_UNIT_BIT;
  scanner->opt[OPT_BIT_DEPTH].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  scanner->opt[OPT_BIT_DEPTH].constraint.word_list = bit_depth_list;
  scanner->val[OPT_BIT_DEPTH].w     = bit_depth_list[1];


  /* quality-calibration */
  scanner->opt[OPT_QUALITY].name  = SANE_NAME_QUALITY_CAL;
  scanner->opt[OPT_QUALITY].title = SANE_TITLE_QUALITY_CAL;
  scanner->opt[OPT_QUALITY].desc  = SANE_DESC_QUALITY_CAL;
  scanner->opt[OPT_QUALITY].type  = SANE_TYPE_BOOL;
  scanner->val[OPT_QUALITY].w     = SANE_FALSE;

  if ((scanner->device->inquiry_quality_ctrl == 0) || (scanner->device->force_quality_calibration) )
  {
    scanner->opt[OPT_QUALITY].cap  |= SANE_CAP_INACTIVE;
  }
  else /* enable quality calibration when available */
  {
    scanner->val[OPT_QUALITY].w = SANE_TRUE;
  }


  /* double optical resolution */
  scanner->opt[OPT_DOR].name  = SANE_NAME_DOR;
  scanner->opt[OPT_DOR].title = SANE_TITLE_DOR;
  scanner->opt[OPT_DOR].desc  = SANE_DESC_DOR;
  scanner->opt[OPT_DOR].type  = SANE_TYPE_BOOL;
  scanner->val[OPT_DOR].w     = SANE_FALSE;

  if (scanner->device->inquiry_dor == 0)
  {
    scanner->opt[OPT_DOR].cap  |= SANE_CAP_INACTIVE;
  }


  /* warmup */
  scanner->opt[OPT_WARMUP].name  = SANE_NAME_WARMUP;
  scanner->opt[OPT_WARMUP].title = SANE_TITLE_WARMUP;
  scanner->opt[OPT_WARMUP].desc  = SANE_DESC_WARMUP;
  scanner->opt[OPT_WARMUP].type  = SANE_TYPE_BOOL;
  scanner->val[OPT_WARMUP].w     = SANE_FALSE;

  if (scanner->device->inquiry_max_warmup_time == 0)
  {
    scanner->opt[OPT_WARMUP].cap  |= SANE_CAP_INACTIVE;
  }

  scanner->opt[OPT_RGB_BIND].name  = SANE_NAME_RGB_BIND;
  scanner->opt[OPT_RGB_BIND].title = SANE_TITLE_RGB_BIND;
  scanner->opt[OPT_RGB_BIND].desc  = SANE_DESC_RGB_BIND;
  scanner->opt[OPT_RGB_BIND].type  = SANE_TYPE_BOOL;
  scanner->val[OPT_RGB_BIND].w     = SANE_FALSE;

  /* brightness */
  scanner->opt[OPT_BRIGHTNESS].name  = SANE_NAME_BRIGHTNESS;
  scanner->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  scanner->opt[OPT_BRIGHTNESS].desc  = SANE_DESC_BRIGHTNESS;
  scanner->opt[OPT_BRIGHTNESS].type  = SANE_TYPE_FIXED;
  scanner->opt[OPT_BRIGHTNESS].unit  = SANE_UNIT_PERCENT;
  scanner->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_BRIGHTNESS].constraint.range = &percentage_range;
  scanner->val[OPT_BRIGHTNESS].w     = 0;

  /* contrast */
  scanner->opt[OPT_CONTRAST].name  = SANE_NAME_CONTRAST;
  scanner->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  scanner->opt[OPT_CONTRAST].desc  = SANE_DESC_CONTRAST;
  scanner->opt[OPT_CONTRAST].type  = SANE_TYPE_FIXED;
  scanner->opt[OPT_CONTRAST].unit  = SANE_UNIT_PERCENT;
  scanner->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_CONTRAST].constraint.range = &percentage_range;
  scanner->val[OPT_CONTRAST].w     = 0;


  /* threshold */
  scanner->opt[OPT_THRESHOLD].name  = SANE_NAME_THRESHOLD;
  scanner->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
  scanner->opt[OPT_THRESHOLD].desc  = SANE_DESC_THRESHOLD;
  scanner->opt[OPT_THRESHOLD].type  = SANE_TYPE_FIXED;
  scanner->opt[OPT_THRESHOLD].unit  = SANE_UNIT_PERCENT;
  scanner->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_THRESHOLD].constraint.range = &percentage_range_100;
  scanner->val[OPT_THRESHOLD].w     = SANE_FIX(50);


  /* ------------------------------ */


  /* highlight, white level */
  scanner->opt[OPT_HIGHLIGHT].name  = SANE_NAME_HIGHLIGHT;
  scanner->opt[OPT_HIGHLIGHT].title = SANE_TITLE_HIGHLIGHT;
  scanner->opt[OPT_HIGHLIGHT].desc  = SANE_DESC_HIGHLIGHT;
  scanner->opt[OPT_HIGHLIGHT].type  = SANE_TYPE_FIXED;
  scanner->opt[OPT_HIGHLIGHT].unit  = SANE_UNIT_PERCENT;
  scanner->opt[OPT_HIGHLIGHT].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_HIGHLIGHT].constraint.range = &percentage_range_100;
  scanner->val[OPT_HIGHLIGHT].w     = SANE_FIX(100);

  scanner->opt[OPT_HIGHLIGHT_R].name  = SANE_NAME_HIGHLIGHT_R;
  scanner->opt[OPT_HIGHLIGHT_R].title = SANE_TITLE_HIGHLIGHT_R;
  scanner->opt[OPT_HIGHLIGHT_R].desc  = SANE_DESC_HIGHLIGHT_R;
  scanner->opt[OPT_HIGHLIGHT_R].type  = SANE_TYPE_FIXED;
  scanner->opt[OPT_HIGHLIGHT_R].unit  = SANE_UNIT_PERCENT;
  scanner->opt[OPT_HIGHLIGHT_R].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_HIGHLIGHT_R].constraint.range = &percentage_range_100;
  scanner->val[OPT_HIGHLIGHT_R].w     = SANE_FIX(100);

  scanner->opt[OPT_HIGHLIGHT_G].name  = SANE_NAME_HIGHLIGHT_G;
  scanner->opt[OPT_HIGHLIGHT_G].title = SANE_TITLE_HIGHLIGHT_G;
  scanner->opt[OPT_HIGHLIGHT_G].desc  = SANE_DESC_HIGHLIGHT_G;
  scanner->opt[OPT_HIGHLIGHT_G].type  = SANE_TYPE_FIXED;
  scanner->opt[OPT_HIGHLIGHT_G].unit  = SANE_UNIT_PERCENT;
  scanner->opt[OPT_HIGHLIGHT_G].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_HIGHLIGHT_G].constraint.range = &percentage_range_100;
  scanner->val[OPT_HIGHLIGHT_G].w     = SANE_FIX(100);

  scanner->opt[OPT_HIGHLIGHT_B].name  = SANE_NAME_HIGHLIGHT_B;
  scanner->opt[OPT_HIGHLIGHT_B].title = SANE_TITLE_HIGHLIGHT_B;
  scanner->opt[OPT_HIGHLIGHT_B].desc  = SANE_DESC_HIGHLIGHT_B;
  scanner->opt[OPT_HIGHLIGHT_B].type  = SANE_TYPE_FIXED;
  scanner->opt[OPT_HIGHLIGHT_B].unit  = SANE_UNIT_PERCENT;
  scanner->opt[OPT_HIGHLIGHT_B].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_HIGHLIGHT_B].constraint.range = &percentage_range_100;
  scanner->val[OPT_HIGHLIGHT_B].w     = SANE_FIX(100);


  /* shadow, black level */
  scanner->opt[OPT_SHADOW].name  = SANE_NAME_SHADOW;
  scanner->opt[OPT_SHADOW].title = SANE_TITLE_SHADOW;
  scanner->opt[OPT_SHADOW].desc  = SANE_DESC_SHADOW;
  scanner->opt[OPT_SHADOW].type  = SANE_TYPE_FIXED;
  scanner->opt[OPT_SHADOW].unit  = SANE_UNIT_PERCENT;
  scanner->opt[OPT_SHADOW].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_SHADOW].constraint.range = &percentage_range_100;
  scanner->val[OPT_SHADOW].w     = 0;

  scanner->opt[OPT_SHADOW_R].name  = SANE_NAME_SHADOW_R;
  scanner->opt[OPT_SHADOW_R].title = SANE_TITLE_SHADOW_R;
  scanner->opt[OPT_SHADOW_R].desc  = SANE_DESC_SHADOW_R;
  scanner->opt[OPT_SHADOW_R].type  = SANE_TYPE_FIXED;
  scanner->opt[OPT_SHADOW_R].unit  = SANE_UNIT_PERCENT;
  scanner->opt[OPT_SHADOW_R].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_SHADOW_R].constraint.range = &percentage_range_100;
  scanner->val[OPT_SHADOW_R].w     = 0;

  scanner->opt[OPT_SHADOW_G].name  = SANE_NAME_SHADOW_G;
  scanner->opt[OPT_SHADOW_G].title = SANE_TITLE_SHADOW_G;
  scanner->opt[OPT_SHADOW_G].desc  = SANE_DESC_SHADOW_G;
  scanner->opt[OPT_SHADOW_G].type  = SANE_TYPE_FIXED;
  scanner->opt[OPT_SHADOW_G].unit  = SANE_UNIT_PERCENT;
  scanner->opt[OPT_SHADOW_G].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_SHADOW_G].constraint.range = &percentage_range_100;
  scanner->val[OPT_SHADOW_G].w     = 0;

  scanner->opt[OPT_SHADOW_B].name  = SANE_NAME_SHADOW_B;
  scanner->opt[OPT_SHADOW_B].title = SANE_TITLE_SHADOW_B;
  scanner->opt[OPT_SHADOW_B].desc  = SANE_DESC_SHADOW_B;
  scanner->opt[OPT_SHADOW_B].type  = SANE_TYPE_FIXED;
  scanner->opt[OPT_SHADOW_B].unit  = SANE_UNIT_PERCENT;
  scanner->opt[OPT_SHADOW_B].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_SHADOW_B].constraint.range = &percentage_range_100;
  scanner->val[OPT_SHADOW_B].w     = 0;



  /* analog-gamma */
  scanner->opt[OPT_ANALOG_GAMMA].name  = SANE_NAME_ANALOG_GAMMA;
  scanner->opt[OPT_ANALOG_GAMMA].title = SANE_TITLE_ANALOG_GAMMA;
  scanner->opt[OPT_ANALOG_GAMMA].desc  = SANE_DESC_ANALOG_GAMMA;
  scanner->opt[OPT_ANALOG_GAMMA].type  = SANE_TYPE_FIXED;
  scanner->opt[OPT_ANALOG_GAMMA].unit  = SANE_UNIT_NONE;
  scanner->opt[OPT_ANALOG_GAMMA].constraint_type  = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_ANALOG_GAMMA].constraint.range = &(scanner->device->analog_gamma_range);
  scanner->val[OPT_ANALOG_GAMMA].w     = 1 << SANE_FIXED_SCALE_SHIFT;

  scanner->opt[OPT_ANALOG_GAMMA_R].name  = SANE_NAME_ANALOG_GAMMA_R;
  scanner->opt[OPT_ANALOG_GAMMA_R].title = SANE_TITLE_ANALOG_GAMMA_R;
  scanner->opt[OPT_ANALOG_GAMMA_R].desc  = SANE_DESC_ANALOG_GAMMA_R;
  scanner->opt[OPT_ANALOG_GAMMA_R].type  = SANE_TYPE_FIXED;
  scanner->opt[OPT_ANALOG_GAMMA_R].unit  = SANE_UNIT_NONE;
  scanner->opt[OPT_ANALOG_GAMMA_R].constraint_type  = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_ANALOG_GAMMA_R].constraint.range = &(scanner->device->analog_gamma_range);
  scanner->val[OPT_ANALOG_GAMMA_R].w     = 1 << SANE_FIXED_SCALE_SHIFT;

  scanner->opt[OPT_ANALOG_GAMMA_G].name  = SANE_NAME_ANALOG_GAMMA_G;
  scanner->opt[OPT_ANALOG_GAMMA_G].title = SANE_TITLE_ANALOG_GAMMA_G;
  scanner->opt[OPT_ANALOG_GAMMA_G].desc  = SANE_DESC_ANALOG_GAMMA_G;
  scanner->opt[OPT_ANALOG_GAMMA_G].type  = SANE_TYPE_FIXED;
  scanner->opt[OPT_ANALOG_GAMMA_G].unit  = SANE_UNIT_NONE;
  scanner->opt[OPT_ANALOG_GAMMA_G].constraint_type  = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_ANALOG_GAMMA_G].constraint.range = &(scanner->device->analog_gamma_range);
  scanner->val[OPT_ANALOG_GAMMA_G].w     = 1 << SANE_FIXED_SCALE_SHIFT;

  scanner->opt[OPT_ANALOG_GAMMA_B].name  = SANE_NAME_ANALOG_GAMMA_B;
  scanner->opt[OPT_ANALOG_GAMMA_B].title = SANE_TITLE_ANALOG_GAMMA_B;
  scanner->opt[OPT_ANALOG_GAMMA_B].desc  = SANE_DESC_ANALOG_GAMMA_B;
  scanner->opt[OPT_ANALOG_GAMMA_B].type  = SANE_TYPE_FIXED;
  scanner->opt[OPT_ANALOG_GAMMA_B].unit  = SANE_UNIT_NONE;
  scanner->opt[OPT_ANALOG_GAMMA_B].constraint_type  = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_ANALOG_GAMMA_B].constraint.range = &(scanner->device->analog_gamma_range);
  scanner->val[OPT_ANALOG_GAMMA_B].w     = 1 << SANE_FIXED_SCALE_SHIFT;


  /* custom-gamma table */
  scanner->opt[OPT_CUSTOM_GAMMA].name  = SANE_NAME_CUSTOM_GAMMA;
  scanner->opt[OPT_CUSTOM_GAMMA].title = SANE_TITLE_CUSTOM_GAMMA;
  scanner->opt[OPT_CUSTOM_GAMMA].desc  = SANE_DESC_CUSTOM_GAMMA;
  scanner->opt[OPT_CUSTOM_GAMMA].type  = SANE_TYPE_BOOL;
  scanner->val[OPT_CUSTOM_GAMMA].w     = SANE_TRUE;

  /* grayscale gamma vector */
  scanner->opt[OPT_GAMMA_VECTOR].name  = SANE_NAME_GAMMA_VECTOR;
  scanner->opt[OPT_GAMMA_VECTOR].title = SANE_TITLE_GAMMA_VECTOR;
  scanner->opt[OPT_GAMMA_VECTOR].desc  = SANE_DESC_GAMMA_VECTOR;
  scanner->opt[OPT_GAMMA_VECTOR].type  = SANE_TYPE_INT;
  scanner->opt[OPT_GAMMA_VECTOR].unit  = SANE_UNIT_NONE;
  scanner->opt[OPT_GAMMA_VECTOR].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->val[OPT_GAMMA_VECTOR].wa    = scanner->gamma_table[0];
  scanner->opt[OPT_GAMMA_VECTOR].constraint.range = &scanner->output_range;
  scanner->opt[OPT_GAMMA_VECTOR].size  = scanner->gamma_length * sizeof(SANE_Word);

  scanner->output_range.min   = 0;

  if (bit_depth_list[1] == 8)
  {
    scanner->output_range.max = 255;    /* 8 bits/pixel */
  }
  else
  {
    scanner->output_range.max = 65535;  /* 9-16 bits/pixel */
  }

  scanner->output_range.quant = 0;

  /* red gamma vector */
  scanner->opt[OPT_GAMMA_VECTOR_R].name  = SANE_NAME_GAMMA_VECTOR_R;
  scanner->opt[OPT_GAMMA_VECTOR_R].title = SANE_TITLE_GAMMA_VECTOR_R;
  scanner->opt[OPT_GAMMA_VECTOR_R].desc  = SANE_DESC_GAMMA_VECTOR_R;
  scanner->opt[OPT_GAMMA_VECTOR_R].type  = SANE_TYPE_INT;
  scanner->opt[OPT_GAMMA_VECTOR_R].unit  = SANE_UNIT_NONE;
  scanner->opt[OPT_GAMMA_VECTOR_R].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->val[OPT_GAMMA_VECTOR_R].wa    = scanner->gamma_table[1];
  scanner->opt[OPT_GAMMA_VECTOR_R].constraint.range = &(scanner->gamma_range);
  scanner->opt[OPT_GAMMA_VECTOR_R].size  = scanner->gamma_length * sizeof(SANE_Word);

  /* green gamma vector */
  scanner->opt[OPT_GAMMA_VECTOR_G].name  = SANE_NAME_GAMMA_VECTOR_G;
  scanner->opt[OPT_GAMMA_VECTOR_G].title = SANE_TITLE_GAMMA_VECTOR_G;
  scanner->opt[OPT_GAMMA_VECTOR_G].desc  = SANE_DESC_GAMMA_VECTOR_G;
  scanner->opt[OPT_GAMMA_VECTOR_G].type  = SANE_TYPE_INT;
  scanner->opt[OPT_GAMMA_VECTOR_G].unit  = SANE_UNIT_NONE;
  scanner->opt[OPT_GAMMA_VECTOR_G].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->val[OPT_GAMMA_VECTOR_G].wa    = scanner->gamma_table[2];
  scanner->opt[OPT_GAMMA_VECTOR_G].constraint.range = &(scanner->gamma_range);
  scanner->opt[OPT_GAMMA_VECTOR_G].size  = scanner->gamma_length * sizeof(SANE_Word);

  /* blue gamma vector */
  scanner->opt[OPT_GAMMA_VECTOR_B].name  = SANE_NAME_GAMMA_VECTOR_B;
  scanner->opt[OPT_GAMMA_VECTOR_B].title = SANE_TITLE_GAMMA_VECTOR_B;
  scanner->opt[OPT_GAMMA_VECTOR_B].desc  = SANE_DESC_GAMMA_VECTOR_B;
  scanner->opt[OPT_GAMMA_VECTOR_B].type  = SANE_TYPE_INT;
  scanner->opt[OPT_GAMMA_VECTOR_B].unit  = SANE_UNIT_NONE;
  scanner->opt[OPT_GAMMA_VECTOR_B].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->val[OPT_GAMMA_VECTOR_B].wa    = scanner->gamma_table[3];
  scanner->opt[OPT_GAMMA_VECTOR_B].constraint.range = &(scanner->gamma_range);
  scanner->opt[OPT_GAMMA_VECTOR_B].size  = scanner->gamma_length * sizeof(SANE_Word);

  /* halftone dimension */
  scanner->opt[OPT_HALFTONE_DIMENSION].name  = SANE_NAME_HALFTONE_DIMENSION;
  scanner->opt[OPT_HALFTONE_DIMENSION].title = SANE_TITLE_HALFTONE_DIMENSION;
  scanner->opt[OPT_HALFTONE_DIMENSION].desc  = SANE_DESC_HALFTONE_DIMENSION;
  scanner->opt[OPT_HALFTONE_DIMENSION].type  = SANE_TYPE_INT;
  scanner->opt[OPT_HALFTONE_DIMENSION].unit  = SANE_UNIT_PIXEL;
  scanner->opt[OPT_HALFTONE_DIMENSION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  scanner->opt[OPT_HALFTONE_DIMENSION].constraint.word_list = pattern_dim_list;
  scanner->val[OPT_HALFTONE_DIMENSION].w     = pattern_dim_list[1];

  /* halftone pattern */
  scanner->opt[OPT_HALFTONE_PATTERN].name  = SANE_NAME_HALFTONE_PATTERN;
  scanner->opt[OPT_HALFTONE_PATTERN].title = SANE_TITLE_HALFTONE_PATTERN;
  scanner->opt[OPT_HALFTONE_PATTERN].desc  = SANE_DESC_HALFTONE_PATTERN;
  scanner->opt[OPT_HALFTONE_PATTERN].type  = SANE_TYPE_INT;
  scanner->opt[OPT_HALFTONE_PATTERN].size  = 0;
  scanner->opt[OPT_HALFTONE_PATTERN].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_HALFTONE_PATTERN].constraint.range = &u8_range;
  scanner->val[OPT_HALFTONE_PATTERN].wa    = scanner->halftone_pattern;


  /* ------------------------------ */


  /* "Advanced" group: */
  scanner->opt[OPT_ADVANCED_GROUP].title = SANE_I18N("Advanced");
  scanner->opt[OPT_ADVANCED_GROUP].desc  = "";
  scanner->opt[OPT_ADVANCED_GROUP].type  = SANE_TYPE_GROUP;
  scanner->opt[OPT_ADVANCED_GROUP].cap   = SANE_CAP_ADVANCED;
  scanner->opt[OPT_ADVANCED_GROUP].constraint_type = SANE_CONSTRAINT_NONE;


  /* ------------------------------ */


  /* select exposure time */
  scanner->opt[OPT_SELECT_EXPOSURE_TIME].name  = SANE_NAME_SELECT_EXPOSURE_TIME;
  scanner->opt[OPT_SELECT_EXPOSURE_TIME].title = SANE_TITLE_SELECT_EXPOSURE_TIME;
  scanner->opt[OPT_SELECT_EXPOSURE_TIME].desc  = SANE_DESC_SELECT_EXPOSURE_TIME;
  scanner->opt[OPT_SELECT_EXPOSURE_TIME].type  = SANE_TYPE_BOOL;
  scanner->val[OPT_SELECT_EXPOSURE_TIME].w     = SANE_FALSE;

  /* select calibration exposure time */
  scanner->opt[OPT_SELECT_CAL_EXPOSURE_TIME].name  = SANE_UMAX_NAME_SELECT_CALIBRATON_EXPOSURE_TIME;
  scanner->opt[OPT_SELECT_CAL_EXPOSURE_TIME].title = SANE_UMAX_TITLE_SELECT_CALIBRATION_EXPOSURE_TIME;
  scanner->opt[OPT_SELECT_CAL_EXPOSURE_TIME].desc  = SANE_UMAX_DESC_SELECT_CALIBRATION_EXPOSURE_TIME;
  scanner->opt[OPT_SELECT_CAL_EXPOSURE_TIME].type  = SANE_TYPE_BOOL;
  scanner->val[OPT_SELECT_CAL_EXPOSURE_TIME].w     = SANE_FALSE;
  scanner->opt[OPT_SELECT_CAL_EXPOSURE_TIME].cap  |= SANE_CAP_INACTIVE;

  /* calibration exposure time */
  scanner->opt[OPT_CAL_EXPOS_TIME].name  = SANE_NAME_CAL_EXPOS_TIME;
  scanner->opt[OPT_CAL_EXPOS_TIME].title = SANE_TITLE_CAL_EXPOS_TIME;
  scanner->opt[OPT_CAL_EXPOS_TIME].desc  = SANE_DESC_CAL_EXPOS_TIME;
  scanner->opt[OPT_CAL_EXPOS_TIME].type  = SANE_TYPE_INT;
  scanner->opt[OPT_CAL_EXPOS_TIME].unit  = SANE_UNIT_MICROSECOND;
  scanner->opt[OPT_CAL_EXPOS_TIME].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_CAL_EXPOS_TIME].constraint.range = &(scanner->exposure_time_range);
  scanner->val[OPT_CAL_EXPOS_TIME].w     = scanner->device->inquiry_exposure_time_g_fb_def *
                                           scanner->device->inquiry_exposure_time_step_unit;
  scanner->opt[OPT_CAL_EXPOS_TIME].cap  |= SANE_CAP_INACTIVE;

  /* calibration exposure time red */
  scanner->opt[OPT_CAL_EXPOS_TIME_R].name  = SANE_NAME_CAL_EXPOS_TIME_R;
  scanner->opt[OPT_CAL_EXPOS_TIME_R].title = SANE_TITLE_CAL_EXPOS_TIME_R;
  scanner->opt[OPT_CAL_EXPOS_TIME_R].desc  = SANE_DESC_CAL_EXPOS_TIME_R;
  scanner->opt[OPT_CAL_EXPOS_TIME_R].type  = SANE_TYPE_INT;
  scanner->opt[OPT_CAL_EXPOS_TIME_R].unit  = SANE_UNIT_MICROSECOND;
  scanner->opt[OPT_CAL_EXPOS_TIME_R].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_CAL_EXPOS_TIME_R].constraint.range = &(scanner->exposure_time_range);
  scanner->val[OPT_CAL_EXPOS_TIME_R].w     = scanner->device->inquiry_exposure_time_c_fb_def_r *
                                             scanner->device->inquiry_exposure_time_step_unit;
  scanner->opt[OPT_CAL_EXPOS_TIME_R].cap  |= SANE_CAP_INACTIVE;

  /* calibration exposure time green */
  scanner->opt[OPT_CAL_EXPOS_TIME_G].name  = SANE_NAME_CAL_EXPOS_TIME_G;
  scanner->opt[OPT_CAL_EXPOS_TIME_G].title = SANE_TITLE_CAL_EXPOS_TIME_G;
  scanner->opt[OPT_CAL_EXPOS_TIME_G].desc  = SANE_DESC_CAL_EXPOS_TIME_G;
  scanner->opt[OPT_CAL_EXPOS_TIME_G].type  = SANE_TYPE_INT;
  scanner->opt[OPT_CAL_EXPOS_TIME_G].unit  = SANE_UNIT_MICROSECOND;
  scanner->opt[OPT_CAL_EXPOS_TIME_G].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_CAL_EXPOS_TIME_G].constraint.range = &(scanner->exposure_time_range);
  scanner->val[OPT_CAL_EXPOS_TIME_G].w     = scanner->device->inquiry_exposure_time_c_fb_def_g *
                                             scanner->device->inquiry_exposure_time_step_unit;
  scanner->opt[OPT_CAL_EXPOS_TIME_G].cap  |= SANE_CAP_INACTIVE;

  /* calibration exposure time blue */
  scanner->opt[OPT_CAL_EXPOS_TIME_B].name  = SANE_NAME_CAL_EXPOS_TIME_B;
  scanner->opt[OPT_CAL_EXPOS_TIME_B].title = SANE_TITLE_CAL_EXPOS_TIME_B;
  scanner->opt[OPT_CAL_EXPOS_TIME_B].desc  = SANE_DESC_CAL_EXPOS_TIME_B;
  scanner->opt[OPT_CAL_EXPOS_TIME_B].type  = SANE_TYPE_INT;
  scanner->opt[OPT_CAL_EXPOS_TIME_B].unit  = SANE_UNIT_MICROSECOND;
  scanner->opt[OPT_CAL_EXPOS_TIME_B].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_CAL_EXPOS_TIME_B].constraint.range = &(scanner->exposure_time_range);
  scanner->val[OPT_CAL_EXPOS_TIME_B].w     = scanner->device->inquiry_exposure_time_c_fb_def_b *
                                             scanner->device->inquiry_exposure_time_step_unit;
  scanner->opt[OPT_CAL_EXPOS_TIME_B].cap  |= SANE_CAP_INACTIVE;

  /* scan exposure time */
  scanner->opt[OPT_SCAN_EXPOS_TIME].name  = SANE_NAME_SCAN_EXPOS_TIME;
  scanner->opt[OPT_SCAN_EXPOS_TIME].title = SANE_TITLE_SCAN_EXPOS_TIME;
  scanner->opt[OPT_SCAN_EXPOS_TIME].desc  = SANE_DESC_SCAN_EXPOS_TIME;
  scanner->opt[OPT_SCAN_EXPOS_TIME].type  = SANE_TYPE_INT;
  scanner->opt[OPT_SCAN_EXPOS_TIME].unit  = SANE_UNIT_MICROSECOND;
  scanner->opt[OPT_SCAN_EXPOS_TIME].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_SCAN_EXPOS_TIME].constraint.range = &(scanner->exposure_time_range);
  scanner->val[OPT_SCAN_EXPOS_TIME].w     = scanner->device->inquiry_exposure_time_g_fb_def *
                                            scanner->device->inquiry_exposure_time_step_unit;
  scanner->opt[OPT_SCAN_EXPOS_TIME].cap  |= SANE_CAP_INACTIVE;

  /* scan exposure time red */
  scanner->opt[OPT_SCAN_EXPOS_TIME_R].name  = SANE_NAME_SCAN_EXPOS_TIME_R;
  scanner->opt[OPT_SCAN_EXPOS_TIME_R].title = SANE_TITLE_SCAN_EXPOS_TIME_R;
  scanner->opt[OPT_SCAN_EXPOS_TIME_R].desc  = SANE_DESC_SCAN_EXPOS_TIME_R;
  scanner->opt[OPT_SCAN_EXPOS_TIME_R].type  = SANE_TYPE_INT;
  scanner->opt[OPT_SCAN_EXPOS_TIME_R].unit  = SANE_UNIT_MICROSECOND;
  scanner->opt[OPT_SCAN_EXPOS_TIME_R].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_SCAN_EXPOS_TIME_R].constraint.range = &(scanner->exposure_time_range);
  scanner->val[OPT_SCAN_EXPOS_TIME_R].w     = scanner->device->inquiry_exposure_time_c_fb_def_r *
                                              scanner->device->inquiry_exposure_time_step_unit;
  scanner->opt[OPT_SCAN_EXPOS_TIME_R].cap  |= SANE_CAP_INACTIVE;

  /* scan exposure time green */
  scanner->opt[OPT_SCAN_EXPOS_TIME_G].name  = SANE_NAME_SCAN_EXPOS_TIME_G;
  scanner->opt[OPT_SCAN_EXPOS_TIME_G].title = SANE_TITLE_SCAN_EXPOS_TIME_G;
  scanner->opt[OPT_SCAN_EXPOS_TIME_G].desc  = SANE_DESC_SCAN_EXPOS_TIME_G;
  scanner->opt[OPT_SCAN_EXPOS_TIME_G].type  = SANE_TYPE_INT;
  scanner->opt[OPT_SCAN_EXPOS_TIME_G].unit  = SANE_UNIT_MICROSECOND;
  scanner->opt[OPT_SCAN_EXPOS_TIME_G].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_SCAN_EXPOS_TIME_G].constraint.range = &(scanner->exposure_time_range);
  scanner->val[OPT_SCAN_EXPOS_TIME_G].w     = scanner->device->inquiry_exposure_time_c_fb_def_g *
                                              scanner->device->inquiry_exposure_time_step_unit;
  scanner->opt[OPT_SCAN_EXPOS_TIME_G].cap  |= SANE_CAP_INACTIVE;

  /* scan exposure time blue */
  scanner->opt[OPT_SCAN_EXPOS_TIME_B].name  = SANE_NAME_SCAN_EXPOS_TIME_B;
  scanner->opt[OPT_SCAN_EXPOS_TIME_B].title = SANE_TITLE_SCAN_EXPOS_TIME_B;
  scanner->opt[OPT_SCAN_EXPOS_TIME_B].desc  = SANE_DESC_SCAN_EXPOS_TIME_B;
  scanner->opt[OPT_SCAN_EXPOS_TIME_B].type  = SANE_TYPE_INT;
  scanner->opt[OPT_SCAN_EXPOS_TIME_B].unit  = SANE_UNIT_MICROSECOND;
  scanner->opt[OPT_SCAN_EXPOS_TIME_B].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_SCAN_EXPOS_TIME_B].constraint.range = &(scanner->exposure_time_range);
  scanner->val[OPT_SCAN_EXPOS_TIME_B].w     = scanner->device->inquiry_exposure_time_c_fb_def_b *
                                              scanner->device->inquiry_exposure_time_step_unit;
  scanner->opt[OPT_SCAN_EXPOS_TIME_B].cap  |= SANE_CAP_INACTIVE;

  if (scanner->device->inquiry_exposure_adj == 0)
  {
    scanner->opt[OPT_SELECT_EXPOSURE_TIME].cap   |= SANE_CAP_INACTIVE;
  }


  /* ------------------------------ */


  /* select calibration lamp density */
  scanner->opt[OPT_SELECT_LAMP_DENSITY].name  = SANE_NAME_SELECT_LAMP_DENSITY;
  scanner->opt[OPT_SELECT_LAMP_DENSITY].title = SANE_TITLE_SELECT_LAMP_DENSITY;
  scanner->opt[OPT_SELECT_LAMP_DENSITY].desc  = SANE_DESC_SELECT_LAMP_DENSITY;
  scanner->opt[OPT_SELECT_LAMP_DENSITY].type  = SANE_TYPE_BOOL;
  scanner->val[OPT_SELECT_LAMP_DENSITY].w     = SANE_FALSE;

  /* calibration lamp density */
  scanner->opt[OPT_CAL_LAMP_DEN].name  = SANE_NAME_CAL_LAMP_DEN;
  scanner->opt[OPT_CAL_LAMP_DEN].title = SANE_TITLE_CAL_LAMP_DEN;
  scanner->opt[OPT_CAL_LAMP_DEN].desc  = SANE_DESC_CAL_LAMP_DEN;
  scanner->opt[OPT_CAL_LAMP_DEN].type  = SANE_TYPE_FIXED;
  scanner->opt[OPT_CAL_LAMP_DEN].unit  = SANE_UNIT_PERCENT;
  scanner->opt[OPT_CAL_LAMP_DEN].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_CAL_LAMP_DEN].constraint.range = &percentage_range_100;
  scanner->val[OPT_CAL_LAMP_DEN].w     = SANE_FIX(50);
  scanner->opt[OPT_CAL_LAMP_DEN].cap  |= SANE_CAP_INACTIVE;

  /* scan lamp density */
  scanner->opt[OPT_SCAN_LAMP_DEN].name  = SANE_NAME_SCAN_LAMP_DEN;
  scanner->opt[OPT_SCAN_LAMP_DEN].title = SANE_TITLE_SCAN_LAMP_DEN;
  scanner->opt[OPT_SCAN_LAMP_DEN].desc  = SANE_DESC_SCAN_LAMP_DEN;
  scanner->opt[OPT_SCAN_LAMP_DEN].type  = SANE_TYPE_FIXED;
  scanner->opt[OPT_SCAN_LAMP_DEN].unit  = SANE_UNIT_PERCENT;
  scanner->opt[OPT_SCAN_LAMP_DEN].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_SCAN_LAMP_DEN].constraint.range = &percentage_range_100;
  scanner->val[OPT_SCAN_LAMP_DEN].w     = SANE_FIX(50);
  scanner->opt[OPT_SCAN_LAMP_DEN].cap  |= SANE_CAP_INACTIVE;

  if (scanner->device->inquiry_lamp_ctrl == 0)
  {
    scanner->opt[OPT_SELECT_LAMP_DENSITY].cap  |= SANE_CAP_INACTIVE;
  }

  /* ------------------------------ */

  /* disable pre focus */
  scanner->opt[OPT_DISABLE_PRE_FOCUS].name  = "disable-pre-focus";
  scanner->opt[OPT_DISABLE_PRE_FOCUS].title = SANE_I18N("Disable pre focus");
  scanner->opt[OPT_DISABLE_PRE_FOCUS].desc  = SANE_I18N("Do not calibrate focus");
  scanner->opt[OPT_DISABLE_PRE_FOCUS].type  = SANE_TYPE_BOOL;
  scanner->val[OPT_DISABLE_PRE_FOCUS].w     = SANE_FALSE;

  if (scanner->device->inquiry_manual_focus == 0)
  {
    scanner->opt[OPT_DISABLE_PRE_FOCUS].cap  |= SANE_CAP_INACTIVE;
  }

  /* manual pre focus */
  scanner->opt[OPT_MANUAL_PRE_FOCUS].name  = "manual-pre-focus";
  scanner->opt[OPT_MANUAL_PRE_FOCUS].title = SANE_I18N("Manual pre focus");
  scanner->opt[OPT_MANUAL_PRE_FOCUS].desc  = "";
  scanner->opt[OPT_MANUAL_PRE_FOCUS].type  = SANE_TYPE_BOOL;
  scanner->val[OPT_MANUAL_PRE_FOCUS].w     = SANE_FALSE;

  if (scanner->device->inquiry_manual_focus == 0)
  {
    scanner->opt[OPT_MANUAL_PRE_FOCUS].cap  |= SANE_CAP_INACTIVE;
  }

  /* fix focus position */
  scanner->opt[OPT_FIX_FOCUS_POSITION].name  = "fix-focus-position";
  scanner->opt[OPT_FIX_FOCUS_POSITION].title = SANE_I18N("Fix focus position");
  scanner->opt[OPT_FIX_FOCUS_POSITION].desc  = "";
  scanner->opt[OPT_FIX_FOCUS_POSITION].type  = SANE_TYPE_BOOL;
  scanner->val[OPT_FIX_FOCUS_POSITION].w     = SANE_FALSE;

  if (scanner->device->inquiry_manual_focus == 0)
  {
    scanner->opt[OPT_FIX_FOCUS_POSITION].cap  |= SANE_CAP_INACTIVE;
  }

  /* lens calibration in doc position */
  scanner->opt[OPT_LENS_CALIBRATION_DOC_POS].name  = "lens-calibration-in-doc-position";
  scanner->opt[OPT_LENS_CALIBRATION_DOC_POS].title = SANE_I18N("Lens calibration in doc position");
  scanner->opt[OPT_LENS_CALIBRATION_DOC_POS].desc  = SANE_I18N("Calibrate lens focus in document position");
  scanner->opt[OPT_LENS_CALIBRATION_DOC_POS].type  = SANE_TYPE_BOOL;
  scanner->val[OPT_LENS_CALIBRATION_DOC_POS].w     = SANE_FALSE;

  if (scanner->device->inquiry_lens_cal_in_doc_pos == 0)
  {
    scanner->opt[OPT_LENS_CALIBRATION_DOC_POS].cap  |= SANE_CAP_INACTIVE;
  }

  /* 0mm holder focus position */
  scanner->opt[OPT_HOLDER_FOCUS_POS_0MM].name  = "holder-focus-position-0mm";
  scanner->opt[OPT_HOLDER_FOCUS_POS_0MM].title = SANE_I18N("Holder focus position 0mm");
  scanner->opt[OPT_HOLDER_FOCUS_POS_0MM].desc  = SANE_I18N("Use 0mm holder focus position instead of 0.6mm");
  scanner->opt[OPT_HOLDER_FOCUS_POS_0MM].type  = SANE_TYPE_BOOL;
  scanner->val[OPT_HOLDER_FOCUS_POS_0MM].w     = SANE_FALSE;

  if (scanner->device->inquiry_sel_uta_lens_cal_pos == 0)
  {
    scanner->opt[OPT_HOLDER_FOCUS_POS_0MM].cap  |= SANE_CAP_INACTIVE;
  }

  /* ------------------------------ */

  /* lamp on */
  scanner->opt[OPT_LAMP_ON].name  = "lamp-on";
  scanner->opt[OPT_LAMP_ON].title = SANE_I18N("Lamp on");
  scanner->opt[OPT_LAMP_ON].desc  = SANE_I18N("Turn on scanner lamp");
  scanner->opt[OPT_LAMP_ON].type  = SANE_TYPE_BUTTON;
  scanner->opt[OPT_LAMP_ON].unit  = SANE_UNIT_NONE;
  scanner->opt[OPT_LAMP_ON].size  = 0;
  scanner->opt[OPT_LAMP_ON].constraint_type  = SANE_CONSTRAINT_NONE;
  scanner->val[OPT_LAMP_ON].w     = 0;

  if (!scanner->device->lamp_control_available)		/* lamp state can not be controlled by driver */
  {
    scanner->opt[OPT_LAMP_ON].cap  |= SANE_CAP_INACTIVE;
  }

  /* ------------------------------ */

  /* lamp off */
  scanner->opt[OPT_LAMP_OFF].name  = "lamp-off";
  scanner->opt[OPT_LAMP_OFF].title = SANE_I18N("Lamp off");
  scanner->opt[OPT_LAMP_OFF].desc  = SANE_I18N("Turn off scanner lamp");
  scanner->opt[OPT_LAMP_OFF].type  = SANE_TYPE_BUTTON;
  scanner->opt[OPT_LAMP_OFF].unit  = SANE_UNIT_NONE;
  scanner->opt[OPT_LAMP_OFF].size  = 0;
  scanner->opt[OPT_LAMP_OFF].constraint_type  = SANE_CONSTRAINT_NONE;
  scanner->val[OPT_LAMP_OFF].w     = 0;

  if (!scanner->device->lamp_control_available)		/* lamp state can not be controlled by driver */
  {
    scanner->opt[OPT_LAMP_OFF].cap  |= SANE_CAP_INACTIVE;
  }

  /* ------------------------------ */

  /* lamp off at exit */
  scanner->opt[OPT_LAMP_OFF_AT_EXIT].name  = "lamp-off-at-exit";
  scanner->opt[OPT_LAMP_OFF_AT_EXIT].title = SANE_I18N("Lamp off at exit");
  scanner->opt[OPT_LAMP_OFF_AT_EXIT].desc  = SANE_I18N("Turn off lamp when program exits");
  scanner->opt[OPT_LAMP_OFF_AT_EXIT].type  = SANE_TYPE_BOOL;
  scanner->val[OPT_LAMP_OFF_AT_EXIT].w     = SANE_FALSE;

  if (!scanner->device->lamp_control_available)		/* lamp state can not be controlled by driver */
  {
    scanner->opt[OPT_LAMP_OFF_AT_EXIT].cap  |= SANE_CAP_INACTIVE;
  }

  /* ------------------------------ */

  /* batch-scan-start */
  scanner->opt[OPT_BATCH_SCAN_START].name  = SANE_NAME_BATCH_SCAN_START;
  scanner->opt[OPT_BATCH_SCAN_START].title = SANE_TITLE_BATCH_SCAN_START;
  scanner->opt[OPT_BATCH_SCAN_START].desc  = SANE_DESC_BATCH_SCAN_START;
  scanner->opt[OPT_BATCH_SCAN_START].type  = SANE_TYPE_BOOL;
  scanner->val[OPT_BATCH_SCAN_START].w     = SANE_FALSE;

  /* batch-scan-loop */
  scanner->opt[OPT_BATCH_SCAN_LOOP].name  = SANE_NAME_BATCH_SCAN_LOOP;
  scanner->opt[OPT_BATCH_SCAN_LOOP].title = SANE_TITLE_BATCH_SCAN_LOOP;
  scanner->opt[OPT_BATCH_SCAN_LOOP].desc  = SANE_DESC_BATCH_SCAN_LOOP;
  scanner->opt[OPT_BATCH_SCAN_LOOP].type  = SANE_TYPE_BOOL;
  scanner->val[OPT_BATCH_SCAN_LOOP].w     = SANE_FALSE;

  /* batch-scan-end */
  scanner->opt[OPT_BATCH_SCAN_END].name  = SANE_NAME_BATCH_SCAN_END;
  scanner->opt[OPT_BATCH_SCAN_END].title = SANE_TITLE_BATCH_SCAN_END;
  scanner->opt[OPT_BATCH_SCAN_END].desc  = SANE_DESC_BATCH_SCAN_END;
  scanner->opt[OPT_BATCH_SCAN_END].type  = SANE_TYPE_BOOL;
  scanner->val[OPT_BATCH_SCAN_END].w     = SANE_FALSE;

  /* batch-scan-next-y */
  scanner->opt[OPT_BATCH_NEXT_TL_Y].name  = SANE_NAME_BATCH_NEXT_TL_Y;
  scanner->opt[OPT_BATCH_NEXT_TL_Y].title = SANE_TITLE_BATCH_NEXT_TL_Y;
  scanner->opt[OPT_BATCH_NEXT_TL_Y].desc  = SANE_DESC_BATCH_NEXT_TL_Y;
  scanner->opt[OPT_BATCH_NEXT_TL_Y].type  = SANE_TYPE_FIXED;
  scanner->opt[OPT_BATCH_NEXT_TL_Y].unit  = SANE_UNIT_MM;
  scanner->opt[OPT_BATCH_NEXT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_BATCH_NEXT_TL_Y].constraint.range = &(scanner->device->y_range);
  scanner->val[OPT_BATCH_NEXT_TL_Y].w     = 0xFFFF; /* mark value as not touched */

  if (scanner->device->inquiry_batch_scan == 0)
  {
    scanner->opt[OPT_BATCH_SCAN_START].cap  |= SANE_CAP_INACTIVE;
    scanner->opt[OPT_BATCH_SCAN_LOOP].cap   |= SANE_CAP_INACTIVE;
    scanner->opt[OPT_BATCH_SCAN_END].cap    |= SANE_CAP_INACTIVE;
    scanner->opt[OPT_BATCH_NEXT_TL_Y].cap      |= SANE_CAP_INACTIVE;
  }

  /* ------------------------------ */

#ifdef UMAX_CALIBRATION_MODE_SELECTABLE
  /* calibration mode */
  scanner->opt[OPT_CALIB_MODE].name  = "calibrationmode";
  scanner->opt[OPT_CALIB_MODE].title = SANE_I18N("Calibration mode");
  scanner->opt[OPT_CALIB_MODE].desc  = SANE_I18N("Define calibration mode");
  scanner->opt[OPT_CALIB_MODE].type  = SANE_TYPE_STRING;
  scanner->opt[OPT_CALIB_MODE].size  = max_string_size(calibration_list);
  scanner->opt[OPT_CALIB_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_CALIB_MODE].constraint.string_list = calibration_list;
  scanner->val[OPT_CALIB_MODE].s     = (SANE_Char*)strdup(calibration_list[0]);

  if (scanner->device->inquiry_calibration == 0)
  {
    scanner->opt[OPT_CALIB_MODE].cap  |= SANE_CAP_INACTIVE;
  }
#endif

  /* preview */
  scanner->opt[OPT_PREVIEW].name  = SANE_NAME_PREVIEW;
  scanner->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  scanner->opt[OPT_PREVIEW].desc  = SANE_DESC_PREVIEW;
  scanner->opt[OPT_PREVIEW].type  = SANE_TYPE_BOOL;
  scanner->opt[OPT_PREVIEW].cap   = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  scanner->val[OPT_PREVIEW].w     = SANE_FALSE;

  sane_control_option(scanner, OPT_MODE, SANE_ACTION_SET_VALUE,
                      (SANE_String *) scan_mode_list[scan_modes], NULL );
  
 return SANE_STATUS_GOOD;
}


/* ------------------------------------------------------------ ATTACH ONE SCSI ----------------------------- */

/* callback function for sanei_config_attach_matching_devices(dev_name, attach_one_scsi) */
static SANE_Status attach_one_scsi(const char *name)
{
  attach_scanner(name, 0, SANE_UMAX_SCSI);
 return SANE_STATUS_GOOD;
}

/* ------------------------------------------------------------ ATTACH ONE USB ------------------------------ */

/* callback function for sanei_usb_attach_matching_devices(dev_name, attach_one_usb) */
static SANE_Status attach_one_usb(const char *name)
{
  attach_scanner(name, 0, SANE_UMAX_USB);
 return SANE_STATUS_GOOD;
}

/* ------------------------------------------------------------ UMAX TEST CONFIGURE OPTION ------------------ */

static SANE_Status umax_test_configure_option(const char *option_str, char *test_name, int *test_value, int test_min, int test_max)
/* returns with 1 if option was found, 0 if option was not found */
{
 const char *value_str;
 char *end_ptr;
 int value;

  if (strncmp(option_str, test_name, strlen(test_name)) == 0)
  {
    value_str = sanei_config_skip_whitespace(option_str+strlen(test_name));

    errno = 0;
    value = strtol(value_str, &end_ptr, 10);
    if (end_ptr == value_str || errno) 
    {
      DBG(DBG_error, "ERROR: invalid value \"%s\" for option %s in %s\n", value_str, test_name, UMAX_CONFIG_FILE);
    }
    else
    {
      if (value < test_min)
      {
        DBG(DBG_error, "ERROR: value \"%d\" is too small for option %s in %s\n", value, test_name, UMAX_CONFIG_FILE);
        value = test_min;
      }
      else if (value > test_max)
      {
        DBG(DBG_error, "ERROR: value \"%d\" is too large for option %s in %s\n", value, test_name, UMAX_CONFIG_FILE);
        value = test_max;
      }

      *test_value = value;

      DBG(DBG_info, "option %s = %d\n", test_name, *test_value);
    }
    return 1;
  }
 return 0;
}

/* ------------------------------------------------------------ SANE INIT ---------------------------------- */


SANE_Status sane_init(SANE_Int *version_code, SANE_Auth_Callback authorize)
{
 char config_line[PATH_MAX];
 const char *option_str;
 size_t len;
 FILE *fp;

  /* we have to initialize these global variables here because sane_init can be called several times */
  num_devices  = 0;
  devlist      = NULL;
  first_dev    = NULL;
  first_handle = NULL;

  DBG_INIT();

  DBG(DBG_sane_init,"sane_init\n");
  DBG(DBG_error,"This is sane-umax version %d.%d build %d\n", SANE_CURRENT_MAJOR, V_MINOR, BUILD);
#ifdef UMAX_ENABLE_USB
  DBG(DBG_error,"compiled with USB support for Astra 2200\n");
#else
  DBG(DBG_error,"no USB support for Astra 2200\n");
#endif
  DBG(DBG_error,"(C) 1997-2002 by Oliver Rauch\n");
  DBG(DBG_error,"EMAIL: Oliver.Rauch@rauch-domain.de\n");

  if (version_code)
  {
    *version_code = SANE_VERSION_CODE(SANE_CURRENT_MAJOR, V_MINOR, BUILD);
  }

  frontend_authorize_callback = authorize; /* store frontend authorize callback */

  sanei_thread_init(); /* must be called before any other sanei_thread call */

#ifdef UMAX_ENABLE_USB
  sanei_usb_init();
  sanei_pv8630_init();
#endif

  fp = sanei_config_open(UMAX_CONFIG_FILE);
  if (!fp) 
  {
    /* no config-file: try /dev/scanner and /dev/usbscanner. */
    attach_scanner("/dev/scanner",    0, SANE_UMAX_SCSI);
#ifdef UMAX_ENABLE_USB
    attach_scanner("/dev/usbscanner", 0, SANE_UMAX_USB);
#endif
   return SANE_STATUS_GOOD;
  }

  DBG(DBG_info, "reading configure file %s\n", UMAX_CONFIG_FILE);

  while(sanei_config_read(config_line, sizeof(config_line), fp))
  {
    if (config_line[0] == '#')
    {
      continue; /* ignore line comments */
    }

    if (strncmp(config_line, "option", 6) == 0)
    {
      option_str = sanei_config_skip_whitespace(config_line+6);

      if      (umax_test_configure_option(option_str, "scsi-maxqueue",                  &umax_scsi_maxqueue, 1, SANE_UMAX_SCSI_MAXQUEUE));
      else if (umax_test_configure_option(option_str, "scsi-buffer-size-min",           &umax_scsi_buffer_size_min,   4096, 1048576));
      else if (umax_test_configure_option(option_str, "scsi-buffer-size-max",           &umax_scsi_buffer_size_max,   4096, 1048576));
      else if (umax_test_configure_option(option_str, "preview-lines",                  &umax_preview_lines,             1,   65535));
      else if (umax_test_configure_option(option_str, "scan-lines",                     &umax_scan_lines,                1,   65535));
      else if (umax_test_configure_option(option_str, "handle-bad-sense-error",         &umax_handle_bad_sense_error,    0,   3));
      else if (umax_test_configure_option(option_str, "execute-request-sense",          &umax_execute_request_sense,     0,   1));
      else if (umax_test_configure_option(option_str, "force-preview-bit-rgb",          &umax_force_preview_bit_rgb,     0,   1));
      else if (umax_test_configure_option(option_str, "slow-speed",                     &umax_slow,                     -1,   1));
      else if (umax_test_configure_option(option_str, "care-about-smearing",            &umax_smear,                    -1,   1));
      else if (umax_test_configure_option(option_str, "calibration-full-ccd",           &umax_calibration_area,         -1,   1));
      else if (umax_test_configure_option(option_str, "calibration-width-offset-batch", &umax_calibration_width_offset_batch, -99999, 65535));
      else if (umax_test_configure_option(option_str, "calibration-width-offset",       &umax_calibration_width_offset, -99999, 65535));
      else if (umax_test_configure_option(option_str, "calibration-bytes-pixel",        &umax_calibration_bytespp,      -1,   2));
      else if (umax_test_configure_option(option_str, "exposure-time-rgb-bind",         &umax_exposure_time_rgb_bind,   -1,   1));
      else if (umax_test_configure_option(option_str, "invert-shading-data",            &umax_invert_shading_data,      -1,   1));
      else if (umax_test_configure_option(option_str, "lamp-control-available",         &umax_lamp_control_available,    0,   1));
      else if (umax_test_configure_option(option_str, "gamma-lsb-padded",               &umax_gamma_lsb_padded,         -1,   1));
      else if (umax_test_configure_option(option_str, "connection-type",                &umax_connection_type,           1,   2));
      else
      {
        DBG(DBG_error,"ERROR: unknown option \"%s\" in %s\n", option_str, UMAX_CONFIG_FILE);
      }
      continue;
    }
    else if (strncmp(config_line, "scsi", 4) == 0)
    {
      DBG(DBG_info,"sanei_config_attach_matching_devices(%s)\n", config_line);
      sanei_config_attach_matching_devices(config_line, attach_one_scsi);
      continue;
    }
    else if (strncmp(config_line, "usb", 3) == 0)
    {
#ifdef UMAX_ENABLE_USB
      DBG(DBG_info,"sanei_usb_attach_matching_devices(%s)\n", config_line);
      sanei_usb_attach_matching_devices(config_line, attach_one_usb);
#else
      DBG(DBG_info,"USB not supported, ignoring config line: %s\n", config_line);
#endif
      continue;
    }

    len = strlen(config_line);

    if (!len) /* ignore empty lines */
    {
      continue;
    }

    /* umax_connection_type is set by umax.conf: 1=scsi, 2=usb */
    attach_scanner(config_line, 0, umax_connection_type); /* try to open as devicename */
  }

  DBG(DBG_info, "finished reading configure file\n");

  fclose(fp);

 return SANE_STATUS_GOOD;
}


/* ------------------------------------------------------------ SANE EXIT ---------------------------------- */


void sane_exit(void)
{
 Umax_Device *dev, *next;

  DBG(DBG_sane_init,"sane_exit\n");

  for (dev = first_dev; dev; dev = next)
  {
    next = dev->next;
    free(dev->devicename);
    free(dev);
  }

  if (devlist)
  {
    free(devlist);
  }
}


/* ------------------------------------------------------------ SANE GET DEVICES --------------------------- */


SANE_Status sane_get_devices(const SANE_Device ***device_list, SANE_Bool local_only)
{
 Umax_Device *dev;
 int i;

  DBG(DBG_sane_init,"sane_get_devices(local_only = %d)\n", local_only);

  if (devlist)
  {
    free(devlist);
  }

  devlist = malloc((num_devices + 1) * sizeof (devlist[0]));
  if (!devlist)
  {
    return SANE_STATUS_NO_MEM;
  }

  i = 0;

  for (dev = first_dev; i < num_devices; dev = dev->next)
  {
    devlist[i++] = &dev->sane;
  }

  devlist[i++] = 0;

  *device_list = devlist;

 return SANE_STATUS_GOOD;
}


/* ------------------------------------------------------------ SANE OPEN ---------------------------------- */


SANE_Status sane_open(SANE_String_Const devicename, SANE_Handle *handle)
{
 Umax_Device *dev;
 SANE_Status status;
 Umax_Scanner *scanner;
 unsigned int i, j;

  DBG(DBG_sane_init,"sane_open\n");

  if (devicename[0])   /* search for devicename */
  {
    DBG(DBG_sane_info,"sane_open: devicename=%s\n", devicename);

    for (dev = first_dev; dev; dev = dev->next)
    {
      if (strcmp(dev->sane.name, devicename) == 0)
      {
        break; /* device found, exit for loop */
      }
    }

    if (!dev) /* no device found */
    {
      status = attach_scanner(devicename, &dev, 0 /* connection-type not known */); /* try to open devicename and set dev */
      if (status != SANE_STATUS_GOOD)
      {
        return status;
      }
    }
  }
  else
  {
    DBG(DBG_sane_info,"sane_open: no devicename, opening first device\n");
    dev = first_dev; 							/* empty devicename -> use first device */
  }

  if (!dev) /* no device found */
  {
    return SANE_STATUS_INVAL;
  }

  scanner = malloc(sizeof (*scanner));
  if (!scanner)
  {
    return SANE_STATUS_NO_MEM;
  }

  memset(scanner, 0, sizeof (*scanner));

  scanner->device = dev;

  if (scanner->device->inquiry_GIB & 32)
  {
    scanner->gamma_length = 65536;							    /* 16 bits input */
    DBG(DBG_sane_info, "Using 16 bits for gamma input\n");
  }
  else if (scanner->device->inquiry_GIB & 16)
  {
    scanner->gamma_length = 16384;							    /* 14 bits input */
    DBG(DBG_sane_info, "Using 14 bits for gamma input\n");
  }
  else if (scanner->device->inquiry_GIB & 8)
  {
    scanner->gamma_length = 4096;							    /* 12 bits input */
    DBG(DBG_sane_info, "Using 12 bits for gamma input\n");
  }
  else if (scanner->device->inquiry_GIB & 4)
  {
    scanner->gamma_length = 1024;							    /* 10 bits input */
    DBG(DBG_sane_info, "Using 10 bits for gamma input\n");
  }
  else if (scanner->device->inquiry_GIB & 2)
  {
    scanner->gamma_length = 512;							     /* 9 bits input */
    DBG(DBG_sane_info, "Using 9 bits for gamma input\n");
  }
  else
  {
    scanner->gamma_length = 256;							     /* 8 bits input */
    DBG(DBG_sane_info, "Using 8 bits for gamma input\n");
  }

  scanner->output_bytes = 1;								    /* 8 bits output */

  scanner->gamma_range.min   = 0;
  scanner->gamma_range.max   = scanner->gamma_length-1;
  scanner->gamma_range.quant = 0;

  scanner->gamma_table[0] = (SANE_Int *) malloc(scanner->gamma_length * sizeof(SANE_Int));
  scanner->gamma_table[1] = (SANE_Int *) malloc(scanner->gamma_length * sizeof(SANE_Int));
  scanner->gamma_table[2] = (SANE_Int *) malloc(scanner->gamma_length * sizeof(SANE_Int));
  scanner->gamma_table[3] = (SANE_Int *) malloc(scanner->gamma_length * sizeof(SANE_Int));

  for (j = 0; j < scanner->gamma_length; ++j)			     /* gamma_table[0] : converts GIB to GOB */
  {
    scanner->gamma_table[0][j] = j * scanner->device->max_value / scanner->gamma_length;
  }

  for (i = 1; i < 4; ++i)			 /* gamma_table[1,2,3] : doesn't convert anything (GIB->GIB) */
  {
    for (j = 0; j < scanner->gamma_length; ++j)
    {
      scanner->gamma_table[i][j] = j;
    }
  }

  scanner->exposure_time_range.min   = scanner->device->inquiry_exposure_time_c_min *
                                       scanner->device->inquiry_exposure_time_step_unit;
  scanner->exposure_time_range.quant = scanner->device->inquiry_exposure_time_step_unit;
  scanner->exposure_time_range.max   = scanner->device->inquiry_exposure_time_max *
                                       scanner->device->inquiry_exposure_time_step_unit;

  init_options(scanner);

  scanner->next = first_handle;			    /* insert newly opened handle into list of open handles: */
  first_handle = scanner;

  *handle = scanner;
 return SANE_STATUS_GOOD;
}


/* ------------------------------------------------------------ SANE CLOSE --------------------------------- */


void sane_close(SANE_Handle handle)
{
 Umax_Scanner *prev, *scanner;

  DBG(DBG_sane_init,"sane_close\n");

  if (!first_handle)
  {
    DBG(DBG_error, "ERROR: sane_close: no handles opened\n");
    return;
  }

								 /* remove handle from list of open handles: */
  prev = 0;

  for (scanner = first_handle; scanner; scanner = scanner->next)
  {
    if (scanner == handle)
    {
      break;
    }

    prev = scanner;
  }
  
  if (!scanner)
  {
    DBG(DBG_error, "ERROR: sane_close: invalid handle %p\n", handle);
    return;								 /* oops, not a handle we know about */
  }

  if (scanner->scanning)						      /* stop scan if still scanning */
  {
    do_cancel(handle);
  } 

  if (scanner->device->lamp_control_available)			   /* lamp state can be controlled by driver */
  {
    if (scanner->val[OPT_LAMP_OFF_AT_EXIT].w)			      /* turn off scanner lamp on sane_close */
    {
      umax_set_lamp_status(handle, 0 /* lamp off */);
    }
  }

  if (prev)
  {
    prev->next = scanner->next;
  }
  else
  {
    first_handle = scanner->next;
  }

  free(scanner->gamma_table[0]);						 /* free custom gamma tables */
  free(scanner->gamma_table[1]);
  free(scanner->gamma_table[2]);
  free(scanner->gamma_table[3]);

  free(scanner->device->buffer[0]);			  /* free buffer allocated by umax_initialize_values */
  scanner->device->buffer[0]  = NULL;
  scanner->device->bufsize = 0;

  free(scanner);									     /* free scanner */
}


/* ------------------------------------------------------------ SANE GET OPTION DESCRIPTOR ----------------- */


const SANE_Option_Descriptor *sane_get_option_descriptor(SANE_Handle handle, SANE_Int option)
{
 Umax_Scanner *scanner = handle;

  DBG(DBG_sane_option,"sane_get_option_descriptor %d\n", option);

  if ((unsigned) option >= NUM_OPTIONS)
  {
    return 0;
  }

 return scanner->opt + option;
}

/* ------------------------------------------------------------ UMAX SET MAX GEOMETRY ---------------------- */

static void umax_set_max_geometry(Umax_Scanner *scanner)
{

  if (scanner->val[OPT_DOR].w)
  {
    scanner->device->x_range.min = SANE_FIX(scanner->device->inquiry_dor_x_off * MM_PER_INCH);
    scanner->device->x_range.max = SANE_FIX( (scanner->device->inquiry_dor_x_off + scanner->device->inquiry_dor_width)  * MM_PER_INCH);
    scanner->device->y_range.min = SANE_FIX(scanner->device->inquiry_dor_y_off * MM_PER_INCH);
    scanner->device->y_range.max = SANE_FIX( (scanner->device->inquiry_dor_y_off + scanner->device->inquiry_dor_length) * MM_PER_INCH);

    scanner->device->x_dpi_range.max = SANE_FIX(scanner->device->inquiry_dor_x_res);
    scanner->device->y_dpi_range.max = SANE_FIX(scanner->device->inquiry_dor_y_res);      
  }
  else if ( (strcmp(scanner->val[OPT_SOURCE].s, FLB_STR) == 0) || (strcmp(scanner->val[OPT_SOURCE].s, ADF_STR) == 0) )
  {
    scanner->device->x_range.min = 0;
    scanner->device->x_range.max = SANE_FIX(scanner->device->inquiry_fb_width  * MM_PER_INCH);
    scanner->device->y_range.min = 0;
    scanner->device->y_range.max = SANE_FIX(scanner->device->inquiry_fb_length * MM_PER_INCH);

    scanner->device->x_dpi_range.max = SANE_FIX(scanner->device->inquiry_x_res);
    scanner->device->y_dpi_range.max = SANE_FIX(scanner->device->inquiry_y_res);      
  }
  else if (strcmp(scanner->val[OPT_SOURCE].s, UTA_STR) == 0)
  {
    scanner->device->x_range.min = SANE_FIX(scanner->device->inquiry_uta_x_off  * MM_PER_INCH);
    scanner->device->x_range.max = SANE_FIX( (scanner->device->inquiry_uta_x_off + scanner->device->inquiry_uta_width) * MM_PER_INCH);
    scanner->device->y_range.min = SANE_FIX(scanner->device->inquiry_uta_y_off  * MM_PER_INCH);
    scanner->device->y_range.max = SANE_FIX( ( scanner->device->inquiry_uta_y_off + scanner->device->inquiry_uta_length) * MM_PER_INCH);

    scanner->device->x_dpi_range.max = SANE_FIX(scanner->device->inquiry_x_res);
    scanner->device->y_dpi_range.max = SANE_FIX(scanner->device->inquiry_y_res);      
  }

  DBG(DBG_info,"x_range     = [%f .. %f]\n", SANE_UNFIX(scanner->device->x_range.min), SANE_UNFIX(scanner->device->x_range.max));
  DBG(DBG_info,"y_range     = [%f .. %f]\n", SANE_UNFIX(scanner->device->y_range.min), SANE_UNFIX(scanner->device->y_range.max));
  DBG(DBG_info,"x_dpi_range = [1 .. %f]\n", SANE_UNFIX(scanner->device->x_dpi_range.max));
  DBG(DBG_info,"y_dpi_range = [1 .. %f]\n", SANE_UNFIX(scanner->device->y_dpi_range.max));

  /* make sure geometry selection is in bounds */

  if ( scanner->val[OPT_TL_X].w < scanner->device->x_range.min)
  {
    scanner->val[OPT_TL_X].w = scanner->device->x_range.min;
  }
 
  if (scanner->val[OPT_TL_Y].w < scanner->device->y_range.min)
  {
    scanner->val[OPT_TL_Y].w = scanner->device->y_range.min;
  }

  if ( scanner->val[OPT_BR_X].w > scanner->device->x_range.max)
  {
    scanner->val[OPT_BR_X].w = scanner->device->x_range.max;
  }
 
  if (scanner->val[OPT_BR_Y].w > scanner->device->y_range.max)
  {
    scanner->val[OPT_BR_Y].w = scanner->device->y_range.max;
  }
}

/* ------------------------------------------------------------ SANE CONTROL OPTION ------------------------ */


SANE_Status sane_control_option(SANE_Handle handle, SANE_Int option, SANE_Action action,
                                void *val, SANE_Int *info)
{
 Umax_Scanner *scanner = handle;
 SANE_Status status;
 SANE_Word w, cap;
 SANE_String_Const name;

  if (info)
  {
    *info = 0;
  }

  if (scanner->scanning)
  {
    return SANE_STATUS_DEVICE_BUSY;
  }

  if ((unsigned) option >= NUM_OPTIONS)
  {
    return SANE_STATUS_INVAL;
  }

  cap = scanner->opt[option].cap;
  if (!SANE_OPTION_IS_ACTIVE (cap))
  {
    return SANE_STATUS_INVAL;
  }

  name = scanner->opt[option].name;
  if (!name)
  {
    name = "(no name)";
  }

  if (action == SANE_ACTION_GET_VALUE)
  {
    DBG(DBG_sane_option,"get %s [#%d]\n", name, option);

    switch (option)
    {
       /* word options: */
      case OPT_NUM_OPTS:
      case OPT_RESOLUTION_BIND:
      case OPT_X_RESOLUTION:
      case OPT_Y_RESOLUTION:
      case OPT_TL_X:
      case OPT_TL_Y:
      case OPT_BR_X:
      case OPT_BR_Y:
      case OPT_PREVIEW:
      case OPT_BIT_DEPTH:
      case OPT_NEGATIVE:
      case OPT_BATCH_SCAN_START:
      case OPT_BATCH_SCAN_LOOP:
      case OPT_BATCH_SCAN_END:
      case OPT_BATCH_NEXT_TL_Y:
      case OPT_QUALITY:
      case OPT_DOR:
      case OPT_WARMUP:
      case OPT_RGB_BIND:
      case OPT_ANALOG_GAMMA:
      case OPT_ANALOG_GAMMA_R:
      case OPT_ANALOG_GAMMA_G:
      case OPT_ANALOG_GAMMA_B:
      case OPT_BRIGHTNESS:
      case OPT_CONTRAST:
      case OPT_THRESHOLD:
      case OPT_HIGHLIGHT:
      case OPT_HIGHLIGHT_R:
      case OPT_HIGHLIGHT_G:
      case OPT_HIGHLIGHT_B:
      case OPT_SHADOW:
      case OPT_SHADOW_R:
      case OPT_SHADOW_G:
      case OPT_SHADOW_B:
      case OPT_CUSTOM_GAMMA:
      case OPT_HALFTONE_DIMENSION:
      case OPT_SELECT_EXPOSURE_TIME:
      case OPT_SELECT_CAL_EXPOSURE_TIME:
      case OPT_CAL_EXPOS_TIME:
      case OPT_CAL_EXPOS_TIME_R:
      case OPT_CAL_EXPOS_TIME_G:
      case OPT_CAL_EXPOS_TIME_B:
      case OPT_SCAN_EXPOS_TIME:
      case OPT_SCAN_EXPOS_TIME_R:
      case OPT_SCAN_EXPOS_TIME_G:
      case OPT_SCAN_EXPOS_TIME_B:
      case OPT_CAL_LAMP_DEN:
      case OPT_SCAN_LAMP_DEN:
      case OPT_DISABLE_PRE_FOCUS:
      case OPT_MANUAL_PRE_FOCUS:
      case OPT_FIX_FOCUS_POSITION:
      case OPT_LENS_CALIBRATION_DOC_POS:
      case OPT_HOLDER_FOCUS_POS_0MM: 
      case OPT_LAMP_OFF_AT_EXIT:
      case OPT_SELECT_LAMP_DENSITY:
        *(SANE_Word *) val = scanner->val[option].w;
       return SANE_STATUS_GOOD;

      /* word-array options: */
      case OPT_GAMMA_VECTOR:
      case OPT_GAMMA_VECTOR_R:
      case OPT_GAMMA_VECTOR_G:
      case OPT_GAMMA_VECTOR_B:
      case OPT_HALFTONE_PATTERN:
        memcpy (val, scanner->val[option].wa, scanner->opt[option].size);
       return SANE_STATUS_GOOD;

      /* string options: */
      case OPT_SOURCE:
      /* fall through */
      case OPT_MODE:
      /* fall through */
#ifdef UMAX_CALIBRATION_MODE_SELECTABLE
      case OPT_CALIB_MODE:
      /* fall through */
#endif
        strcpy (val, scanner->val[option].s);
       return SANE_STATUS_GOOD;
    }
  }
  else if (action == SANE_ACTION_SET_VALUE)
  {
    switch (scanner->opt[option].type)
    {
      case SANE_TYPE_INT:
        DBG(DBG_sane_option, "set %s [#%d] to %d\n", name, option, *(SANE_Word *) val);
       break;

      case SANE_TYPE_FIXED:
        DBG(DBG_sane_option, "set %s [#%d] to %f\n", name, option, SANE_UNFIX(*(SANE_Word *) val));
       break;

      case SANE_TYPE_STRING:
        DBG(DBG_sane_option, "set %s [#%d] to %s\n", name, option, (char *) val);
       break;

      case SANE_TYPE_BOOL:
        DBG(DBG_sane_option, "set %s [#%d] to %d\n", name, option, *(SANE_Word *) val);
       break;

      default:
        DBG(DBG_sane_option, "set %s [#%d]\n", name, option);
    }

    if (!SANE_OPTION_IS_SETTABLE(cap))
    {
      DBG(DBG_error, "could not set option, not settable\n");
     return SANE_STATUS_INVAL;
    }

    status = sanei_constrain_value(scanner->opt+option, val, info);
    if (status != SANE_STATUS_GOOD)
    {
      DBG(DBG_error, "could not set option, invalid value\n");
     return status;
    }

    switch (option)
    {
      /* (mostly) side-effect-free word options: */
      case OPT_X_RESOLUTION:
      case OPT_Y_RESOLUTION:
      case OPT_TL_X:
      case OPT_TL_Y:
      case OPT_BR_X:
      case OPT_BR_Y:
        if (info)
        {
          *info |= SANE_INFO_RELOAD_PARAMS;
        }
        /* fall through */
      case OPT_NUM_OPTS:
      case OPT_NEGATIVE:
      case OPT_BATCH_SCAN_START:
      case OPT_BATCH_SCAN_LOOP:
      case OPT_BATCH_SCAN_END:
      case OPT_BATCH_NEXT_TL_Y:
      case OPT_QUALITY:
      case OPT_WARMUP:
      case OPT_PREVIEW:
      case OPT_ANALOG_GAMMA:
      case OPT_ANALOG_GAMMA_R:
      case OPT_ANALOG_GAMMA_G:
      case OPT_ANALOG_GAMMA_B:
      case OPT_BRIGHTNESS:
      case OPT_CONTRAST:
      case OPT_THRESHOLD:
      case OPT_HIGHLIGHT:
      case OPT_HIGHLIGHT_R:
      case OPT_HIGHLIGHT_G:
      case OPT_HIGHLIGHT_B:
      case OPT_SHADOW:
      case OPT_SHADOW_R:
      case OPT_SHADOW_G:
      case OPT_SHADOW_B:
      case OPT_CAL_EXPOS_TIME:
      case OPT_CAL_EXPOS_TIME_R:
      case OPT_CAL_EXPOS_TIME_G:
      case OPT_CAL_EXPOS_TIME_B:
      case OPT_SCAN_EXPOS_TIME:
      case OPT_SCAN_EXPOS_TIME_R:
      case OPT_SCAN_EXPOS_TIME_G:
      case OPT_SCAN_EXPOS_TIME_B:
      case OPT_CAL_LAMP_DEN:
      case OPT_SCAN_LAMP_DEN:
      case OPT_DISABLE_PRE_FOCUS:
      case OPT_MANUAL_PRE_FOCUS:
      case OPT_FIX_FOCUS_POSITION:
      case OPT_LENS_CALIBRATION_DOC_POS:
      case OPT_HOLDER_FOCUS_POS_0MM: 
      case OPT_LAMP_OFF_AT_EXIT:
        scanner->val[option].w = *(SANE_Word *) val;
       return SANE_STATUS_GOOD;

      case OPT_DOR:
      {
        if (scanner->val[option].w != *(SANE_Word *) val)
        {
          scanner->val[option].w = *(SANE_Word *) val; /* update valure for umax_set_max_geometry */

          if (info)
          {
            *info |= SANE_INFO_RELOAD_PARAMS;
            *info |= SANE_INFO_RELOAD_OPTIONS;
          }

          DBG(DBG_info,"sane_control_option: set DOR = %d\n", scanner->val[option].w);
          umax_set_max_geometry(scanner); 
        }
       return SANE_STATUS_GOOD;
      }

      case OPT_BIT_DEPTH:
      {
        if (scanner->val[option].w != *(SANE_Word *) val)
        {
          scanner->val[option].w = *(SANE_Word *) val;

          if (info)
          {
            *info |= SANE_INFO_RELOAD_OPTIONS;
          }

          scanner->output_range.min   = 0;
          scanner->output_range.quant = 0;

          if (scanner->val[option].w == 8)						       /* 8 bit mode */
          {
            scanner->output_bytes  = 1;							    /* 1 bytes output */
            scanner->output_range.max = 255;
          }
          else										      /* > 8 bit mode */
          {
            scanner->output_bytes  = 2;							    /* 2 bytes output */

            if (scanner->device->gamma_lsb_padded) /* e.g. astra 1220s need lsb padded data */
            {
              scanner->output_range.max   = (int) pow(2, scanner->val[option].w) - 1;
            }
            else
            {
              scanner->output_range.max = 65535; /* define maxval for msb padded data */
            }
          }


          if (info)
          {
            *info |= SANE_INFO_RELOAD_PARAMS;
          }
        }
       return SANE_STATUS_GOOD;
      }

      case OPT_RGB_BIND:
      {
        if (scanner->val[option].w != *(SANE_Word *) val)
        {
          scanner->val[option].w = *(SANE_Word *) val;
          if (info)
          {
            *info |= SANE_INFO_RELOAD_OPTIONS;
          }

          umax_set_rgb_bind(scanner);
        }
       return SANE_STATUS_GOOD;
      }

      case OPT_RESOLUTION_BIND:
      {
        if (scanner->val[option].w != *(SANE_Word *) val)
	{
          scanner->val[option].w = *(SANE_Word *) val;

          if (info)
          {
            *info |= SANE_INFO_RELOAD_OPTIONS;
          }
          if (scanner->val[option].w == SANE_FALSE)
          { /* don't bind */
            scanner->opt[OPT_Y_RESOLUTION].cap &= ~SANE_CAP_INACTIVE;
            scanner->opt[OPT_X_RESOLUTION].title = SANE_TITLE_SCAN_X_RESOLUTION;
            scanner->opt[OPT_X_RESOLUTION].name  = SANE_NAME_SCAN_RESOLUTION;
            scanner->opt[OPT_X_RESOLUTION].desc  = SANE_DESC_SCAN_X_RESOLUTION;
          }
          else
          { /* bind */
            scanner->opt[OPT_Y_RESOLUTION].cap |= SANE_CAP_INACTIVE;
            scanner->opt[OPT_X_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
            scanner->opt[OPT_X_RESOLUTION].name  = SANE_NAME_SCAN_RESOLUTION;
            scanner->opt[OPT_X_RESOLUTION].desc  = SANE_DESC_SCAN_RESOLUTION;
          }
	}
       return SANE_STATUS_GOOD;
      }

      case OPT_SELECT_EXPOSURE_TIME:
      case OPT_SELECT_CAL_EXPOSURE_TIME:
      {
        if (scanner->val[option].w != *(SANE_Word *) val)
	{
          scanner->val[option].w = *(SANE_Word *) val;

          if (info)
          {
            *info |= SANE_INFO_RELOAD_OPTIONS;
          }

          if (scanner->val[OPT_SELECT_EXPOSURE_TIME].w == SANE_FALSE)
	  {
            scanner->opt[OPT_SELECT_CAL_EXPOSURE_TIME].cap |= SANE_CAP_INACTIVE;

            scanner->opt[OPT_CAL_EXPOS_TIME].cap    |= SANE_CAP_INACTIVE;
            scanner->opt[OPT_CAL_EXPOS_TIME_R].cap  |= SANE_CAP_INACTIVE;
            scanner->opt[OPT_CAL_EXPOS_TIME_G].cap  |= SANE_CAP_INACTIVE;
            scanner->opt[OPT_CAL_EXPOS_TIME_B].cap  |= SANE_CAP_INACTIVE;

            scanner->opt[OPT_SCAN_EXPOS_TIME].cap   |= SANE_CAP_INACTIVE;
            scanner->opt[OPT_SCAN_EXPOS_TIME_R].cap |= SANE_CAP_INACTIVE;
            scanner->opt[OPT_SCAN_EXPOS_TIME_G].cap |= SANE_CAP_INACTIVE;
            scanner->opt[OPT_SCAN_EXPOS_TIME_B].cap |= SANE_CAP_INACTIVE;
	  }
	  else /* exposure time selection active */
	  {
            scanner->opt[OPT_SELECT_CAL_EXPOSURE_TIME].cap &= ~SANE_CAP_INACTIVE;

            if ( (strcmp(scanner->val[OPT_MODE].s, COLOR_STR) != 0) ||
	         (scanner->val[OPT_RGB_BIND].w == SANE_TRUE) ||
                 (scanner->device->exposure_time_rgb_bind) ) /* RGB bind */
            {
              if (scanner->val[OPT_SELECT_CAL_EXPOSURE_TIME].w)
              {
                scanner->opt[OPT_CAL_EXPOS_TIME].cap &= ~SANE_CAP_INACTIVE;
              }
              else
              {
                scanner->opt[OPT_CAL_EXPOS_TIME].cap |= SANE_CAP_INACTIVE;
              }

              scanner->opt[OPT_SCAN_EXPOS_TIME].cap &= ~SANE_CAP_INACTIVE;
            }
            else /* no RGB bind */
            {
              if (scanner->val[OPT_SELECT_CAL_EXPOSURE_TIME].w)
              {
                scanner->opt[OPT_CAL_EXPOS_TIME_R].cap &= ~SANE_CAP_INACTIVE;
                scanner->opt[OPT_CAL_EXPOS_TIME_G].cap &= ~SANE_CAP_INACTIVE;
                scanner->opt[OPT_CAL_EXPOS_TIME_B].cap &= ~SANE_CAP_INACTIVE;
              }
              else
              {
                scanner->opt[OPT_CAL_EXPOS_TIME_R].cap |= SANE_CAP_INACTIVE;
                scanner->opt[OPT_CAL_EXPOS_TIME_G].cap |= SANE_CAP_INACTIVE;
                scanner->opt[OPT_CAL_EXPOS_TIME_B].cap |= SANE_CAP_INACTIVE;
              }

              scanner->opt[OPT_SCAN_EXPOS_TIME_R].cap &= ~SANE_CAP_INACTIVE;
              scanner->opt[OPT_SCAN_EXPOS_TIME_G].cap &= ~SANE_CAP_INACTIVE;
              scanner->opt[OPT_SCAN_EXPOS_TIME_B].cap &= ~SANE_CAP_INACTIVE;
            }
	  }
	}
       return SANE_STATUS_GOOD;
      }

      case OPT_SELECT_LAMP_DENSITY:
      {
        if (scanner->val[option].w != *(SANE_Word *) val)
	{
          scanner->val[option].w = *(SANE_Word *) val;

          if (info)
          {
            *info |= SANE_INFO_RELOAD_OPTIONS;
          }
          if (scanner->val[option].w == SANE_FALSE)
	  {
            scanner->opt[OPT_CAL_LAMP_DEN].cap  |= SANE_CAP_INACTIVE;
            scanner->opt[OPT_SCAN_LAMP_DEN].cap |= SANE_CAP_INACTIVE;
	  }
	  else
	  {
            scanner->opt[OPT_CAL_LAMP_DEN].cap  &= ~SANE_CAP_INACTIVE;
            scanner->opt[OPT_SCAN_LAMP_DEN].cap &= ~SANE_CAP_INACTIVE;
	  }
	}
       return SANE_STATUS_GOOD;
      }

      /* side-effect-free word-array options: */
      case OPT_HALFTONE_PATTERN:
      case OPT_GAMMA_VECTOR:
      case OPT_GAMMA_VECTOR_R:
      case OPT_GAMMA_VECTOR_G:
      case OPT_GAMMA_VECTOR_B:
        memcpy (scanner->val[option].wa, val, scanner->opt[option].size);
       return SANE_STATUS_GOOD;

      /* single string-option with side-effect: */
      case OPT_SOURCE:
      {
        if (scanner->val[option].s)
        {
          free(scanner->val[option].s);
        }
        scanner->val[option].s = (SANE_Char *) strdup(val); /* update string for umax_set_max_geometry */

        DBG(DBG_info,"sane_control_option: set SOURCE = %s\n", (SANE_Char *) val);
        umax_set_max_geometry(scanner);

        if (info)
        {
          *info |= SANE_INFO_RELOAD_PARAMS;
          *info |= SANE_INFO_RELOAD_OPTIONS;
        }
       return SANE_STATUS_GOOD;
      }
      break;

      /* side-effect-free single-string options: */
#ifdef UMAX_CALIBRATION_MODE_SELECTABLE
      case OPT_CALIB_MODE:
      /* fall through */
#endif
      {
        if (scanner->val[option].s)
        {
          free (scanner->val[option].s);
        }
        scanner->val[option].s = (SANE_Char*)strdup(val);
       return SANE_STATUS_GOOD;
      }

      /* options with side-effects: */

      case OPT_CUSTOM_GAMMA:
      {
        w = *(SANE_Word *) val;
        if (w == scanner->val[OPT_CUSTOM_GAMMA].w) { return SANE_STATUS_GOOD; } 

        scanner->val[OPT_CUSTOM_GAMMA].w = w;
        if (w)									   /* use custom_gamma_table */
        {
           const char *mode = scanner->val[OPT_MODE].s;

           if ( (strcmp(mode, LINEART_STR) == 0) ||
                (strcmp(mode, HALFTONE_STR) == 0) ||
                (strcmp(mode, GRAY_STR) == 0) )
           { scanner->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE; }
           else if (strcmp(mode, COLOR_STR) == 0)
           {
             scanner->opt[OPT_GAMMA_VECTOR].cap   &= ~SANE_CAP_INACTIVE;
             scanner->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
             scanner->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
             scanner->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
           }
        }
        else								     /* don't use custom_gamma_table */
        {
          scanner->opt[OPT_GAMMA_VECTOR].cap   |= SANE_CAP_INACTIVE;
          scanner->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
          scanner->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
          scanner->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
        }
        if (info)
        {
          *info |= SANE_INFO_RELOAD_OPTIONS;
        }
       return SANE_STATUS_GOOD;
      }

      case OPT_MODE:
      {
       int halftoning;

        if (scanner->val[option].s)
        {
          free (scanner->val[option].s);
        }

        scanner->val[option].s = (SANE_Char*)strdup(val);

        if (info)
        {
          *info |=SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
        }

        scanner->opt[OPT_NEGATIVE].cap           |= SANE_CAP_INACTIVE; 

        scanner->opt[OPT_BIT_DEPTH].cap          |= SANE_CAP_INACTIVE;

        scanner->opt[OPT_CUSTOM_GAMMA].cap       |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_GAMMA_VECTOR].cap       |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_GAMMA_VECTOR_R].cap     |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_GAMMA_VECTOR_G].cap     |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_GAMMA_VECTOR_B].cap     |= SANE_CAP_INACTIVE;

        scanner->opt[OPT_CONTRAST].cap           |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_BRIGHTNESS].cap         |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_THRESHOLD].cap          |= SANE_CAP_INACTIVE;

        scanner->opt[OPT_RGB_BIND].cap           |= SANE_CAP_INACTIVE;

        scanner->opt[OPT_ANALOG_GAMMA].cap       |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_ANALOG_GAMMA_R].cap     |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_ANALOG_GAMMA_G].cap     |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_ANALOG_GAMMA_B].cap     |= SANE_CAP_INACTIVE;

        scanner->opt[OPT_HIGHLIGHT].cap          |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_HIGHLIGHT_R].cap        |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_HIGHLIGHT_G].cap        |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_HIGHLIGHT_B].cap        |= SANE_CAP_INACTIVE;

        scanner->opt[OPT_SHADOW].cap             |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_SHADOW_R].cap           |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_SHADOW_G].cap           |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_SHADOW_B].cap           |= SANE_CAP_INACTIVE;

        scanner->opt[OPT_CAL_EXPOS_TIME].cap     |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_CAL_EXPOS_TIME_R].cap   |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_CAL_EXPOS_TIME_G].cap   |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_CAL_EXPOS_TIME_B].cap   |= SANE_CAP_INACTIVE;

        scanner->opt[OPT_SCAN_EXPOS_TIME].cap    |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_SCAN_EXPOS_TIME_R].cap  |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_SCAN_EXPOS_TIME_G].cap  |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_SCAN_EXPOS_TIME_B].cap  |= SANE_CAP_INACTIVE;

        scanner->opt[OPT_HALFTONE_DIMENSION].cap |= SANE_CAP_INACTIVE;
        scanner->opt[OPT_HALFTONE_PATTERN].cap   |= SANE_CAP_INACTIVE;


        halftoning = (strcmp(val, HALFTONE_STR) == 0 || strcmp(val, COLOR_HALFTONE_STR) == 0);

        if (halftoning || strcmp(val, LINEART_STR) == 0 || strcmp(val, COLOR_LINEART_STR) == 0)
        {										    /* one bit modes */
          if (scanner->device->inquiry_reverse)
          {
            scanner->opt[OPT_NEGATIVE].cap  &= ~SANE_CAP_INACTIVE;
          }

          if (halftoning)
          {										 /* halftoning modes */
            scanner->opt[OPT_CONTRAST].cap   &= ~SANE_CAP_INACTIVE;
            scanner->opt[OPT_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;

	    if (scanner->device->inquiry_highlight)
            {
              scanner->opt[OPT_HIGHLIGHT].cap &= ~SANE_CAP_INACTIVE;
            }

	    if (scanner->device->inquiry_shadow)
            {
              scanner->opt[OPT_SHADOW].cap &= ~SANE_CAP_INACTIVE;
            }

/* disable halftone pattern download options */
#if 0
            scanner->opt[OPT_HALFTONE_DIMENSION].cap &= ~SANE_CAP_INACTIVE;

            if (scanner->val[OPT_HALFTONE_DIMENSION].w)
            {
              scanner->opt[OPT_HALFTONE_PATTERN].cap &= ~SANE_CAP_INACTIVE;
            }
#endif

            if (scanner->val[OPT_SELECT_EXPOSURE_TIME].w == SANE_TRUE)
            {
              scanner->opt[OPT_CAL_EXPOS_TIME].cap   &= ~SANE_CAP_INACTIVE; 
              scanner->opt[OPT_SCAN_EXPOS_TIME].cap  &= ~SANE_CAP_INACTIVE;
            }

            scanner->exposure_time_range.min = scanner->device->inquiry_exposure_time_h_min
	                                       * scanner->device->inquiry_exposure_time_step_unit;
          }
	  else
	  {										    /* lineart modes */
            scanner->opt[OPT_THRESHOLD].cap  &= ~SANE_CAP_INACTIVE;

            if (scanner->val[OPT_SELECT_EXPOSURE_TIME].w == SANE_TRUE)
            {
              scanner->opt[OPT_CAL_EXPOS_TIME].cap   &= ~SANE_CAP_INACTIVE;
              scanner->opt[OPT_SCAN_EXPOS_TIME].cap   &= ~SANE_CAP_INACTIVE;
            }

            scanner->exposure_time_range.min = scanner->device->inquiry_exposure_time_l_min
	                                       * scanner->device->inquiry_exposure_time_step_unit;
	  }
        }
        else
        {								   /* multi-bit modes(gray or color) */
          scanner->opt[OPT_BIT_DEPTH].cap &= ~SANE_CAP_INACTIVE;

          if (scanner->device->inquiry_highlight)
          {
            scanner->opt[OPT_HIGHLIGHT].cap &= ~SANE_CAP_INACTIVE;
          }

          if (scanner->device->inquiry_shadow)
          {
            scanner->opt[OPT_SHADOW].cap &= ~SANE_CAP_INACTIVE;
          }

          if (scanner->device->inquiry_reverse_multi)
          {
            scanner->opt[OPT_NEGATIVE].cap &= ~SANE_CAP_INACTIVE; 
          }

          if (scanner->device->inquiry_gamma_dwload)
          {
            scanner->opt[OPT_CUSTOM_GAMMA].cap &= ~SANE_CAP_INACTIVE;
          }
          else
          {
            scanner->val[OPT_CUSTOM_GAMMA].w = SANE_FALSE;
          }

          if (scanner->device->inquiry_analog_gamma)
          {
            scanner->opt[OPT_ANALOG_GAMMA].cap &= ~SANE_CAP_INACTIVE;
          }

          if (scanner->val[OPT_SELECT_EXPOSURE_TIME].w == SANE_TRUE)
          {
            scanner->opt[OPT_CAL_EXPOS_TIME].cap  &= ~SANE_CAP_INACTIVE;
            scanner->opt[OPT_SCAN_EXPOS_TIME].cap &= ~SANE_CAP_INACTIVE;
          }

          if (strcmp(val, COLOR_STR) == 0)
          {
            if ( (scanner->device->inquiry_analog_gamma) ||
                 (scanner->device->inquiry_highlight)    ||
                 (scanner->device->inquiry_shadow)       ||
                 (scanner->device->inquiry_exposure_adj) )
            {
              scanner->opt[OPT_RGB_BIND].cap &= ~SANE_CAP_INACTIVE;
            }

            scanner->exposure_time_range.min = scanner->device->inquiry_exposure_time_c_min
	                                       * scanner->device->inquiry_exposure_time_step_unit;
          }
	  else /* grayscale */
	  {
            scanner->exposure_time_range.min = scanner->device->inquiry_exposure_time_g_min
	                                       * scanner->device->inquiry_exposure_time_step_unit;
	  }
	}

        umax_set_rgb_bind(scanner);

        if (scanner->val[OPT_CUSTOM_GAMMA].w)
        {
          if (strcmp(val, GRAY_STR) == 0)
          {
            scanner->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
          }
          else if (strcmp(val, COLOR_STR) == 0)
          {
            scanner->opt[OPT_GAMMA_VECTOR].cap   &= ~SANE_CAP_INACTIVE;
            scanner->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
            scanner->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
            scanner->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
          }
	}
       return SANE_STATUS_GOOD;
      }

      case OPT_HALFTONE_DIMENSION: /* halftone pattern dimension affects halftone pattern option: */
      {
        unsigned dim = *(SANE_Word *) val;

         scanner->val[option].w = dim;

         if (info)
         {
           *info |= SANE_INFO_RELOAD_OPTIONS;
         }

         scanner->opt[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;

         if (dim > 0)
         {
           scanner->opt[OPT_HALFTONE_PATTERN].cap &= ~SANE_CAP_INACTIVE;
           scanner->opt[OPT_HALFTONE_PATTERN].size = dim * sizeof (SANE_Word);
         }
        return SANE_STATUS_GOOD;
      }

      case OPT_LAMP_ON:
      {
        if (!umax_set_lamp_status(handle, 1 /* lamp on */))
        {
          return SANE_STATUS_GOOD;
        }
        else
        {
          return SANE_STATUS_UNSUPPORTED;
        }
      }

      case OPT_LAMP_OFF:
      {
        if (!umax_set_lamp_status(handle, 0 /* lamp off */))
        {
          return SANE_STATUS_GOOD;
        }
        else
        {
          return SANE_STATUS_UNSUPPORTED;
        }
      }
    }
  } /* else */
 return SANE_STATUS_INVAL;
}


/* ------------------------------------------------------------ SANE GET PARAMETERS ------------------------ */


SANE_Status sane_get_parameters(SANE_Handle handle, SANE_Parameters *params)
{
 Umax_Scanner *scanner = handle;
 const char *mode;

  DBG(DBG_sane_info,"sane_get_parameters\n");

  if (!scanner->scanning)
  {								  /* not scanning, so lets use recent values */
    double width, length, x_dpi, y_dpi;

    memset(&scanner->params, 0, sizeof (scanner->params));

    width  = SANE_UNFIX(scanner->val[OPT_BR_X].w - scanner->val[OPT_TL_X].w);
    length = SANE_UNFIX(scanner->val[OPT_BR_Y].w - scanner->val[OPT_TL_Y].w);
    x_dpi  = SANE_UNFIX(scanner->val[OPT_X_RESOLUTION].w);
    y_dpi  = SANE_UNFIX(scanner->val[OPT_Y_RESOLUTION].w);

    if ( (scanner->val[OPT_RESOLUTION_BIND].w == SANE_TRUE) || (scanner->val[OPT_PREVIEW].w == SANE_TRUE) )
    {
      y_dpi = x_dpi;
    }

    if (x_dpi > 0.0 && y_dpi > 0.0 && width > 0.0 && length > 0.0)
    {
      double x_dots_per_mm = x_dpi / MM_PER_INCH;
      double y_dots_per_mm = y_dpi / MM_PER_INCH;

      scanner->params.pixels_per_line = width *  x_dots_per_mm;
      scanner->params.lines           = length * y_dots_per_mm;
    }
  }

  mode = scanner->val[OPT_MODE].s;

  if (strcmp(mode, LINEART_STR) == 0 || strcmp(mode, HALFTONE_STR) == 0)
  {
    scanner->params.format         = SANE_FRAME_GRAY;
    scanner->params.bytes_per_line = (scanner->params.pixels_per_line + 7) / 8;
    scanner->params.depth          = 1;
  }
  else if (strcmp(mode, GRAY_STR) == 0)
  {
    scanner->params.format         = SANE_FRAME_GRAY;
    scanner->params.bytes_per_line = scanner->params.pixels_per_line * scanner->output_bytes;
    scanner->params.depth          = 8 * scanner->output_bytes;
  }
  else if (strcmp(mode, COLOR_LINEART_STR) == 0 || strcmp(mode, COLOR_HALFTONE_STR) == 0 )
  {
    if (scanner->device->inquiry_one_pass_color)
    {
      scanner->device->three_pass = 0;
      scanner->params.format         = SANE_FRAME_RGB;
      scanner->params.bytes_per_line = 3 * scanner->params.pixels_per_line;
      scanner->params.depth          = 8;
    }
    else										 /* three pass color */
    {
      scanner->device->three_pass = 1;
      scanner->params.format         = SANE_FRAME_RED + scanner->device->three_pass_color - 1;
      scanner->params.bytes_per_line = scanner->params.pixels_per_line;
      scanner->params.depth          = 8;
    }
  }
  else												      /* RGB */
  {
    if (scanner->device->inquiry_one_pass_color)
    {
      scanner->device->three_pass = 0;
      scanner->params.format         = SANE_FRAME_RGB;
      scanner->params.bytes_per_line = 3 * scanner->params.pixels_per_line * scanner->output_bytes;
      scanner->params.depth          = 8 * scanner->output_bytes;
    }
    else										 /* three pass color */
    {
      scanner->device->three_pass = 1;
      scanner->params.format         = SANE_FRAME_RED + scanner->device->three_pass_color - 1;
      scanner->params.bytes_per_line = scanner->params.pixels_per_line * scanner->output_bytes;
      scanner->params.depth          = 8 * scanner->output_bytes;
    }
  }

  scanner->params.last_frame = (scanner->params.format != SANE_FRAME_RED && scanner->params.format != SANE_FRAME_GREEN);

  if (params)
  {
    *params = scanner->params;
  }

 return SANE_STATUS_GOOD;
}


/* ------------------------------------------------------------ SANE START --------------------------------- */


SANE_Status sane_start(SANE_Handle handle)
{
 Umax_Scanner *scanner = handle;
 const char *mode;
 double xbasedots, ybasedots;
 const char *scan_source;
 int pause;
 int status;
 int fds[2];

  DBG(DBG_sane_init,"sane_start\n");

  /* Initialize reader_pid to invalid so a subsequent error and following call
   * to do_cancel() won't trip over it. */
  scanner->reader_pid = -1;

  mode = scanner->val[OPT_MODE].s;

  if (scanner->device->sfd == -1)   /* first call, don`t run this routine again on multi frame or multi image scan */
  {
    umax_initialize_values(scanner->device);								    /* reset values */

    scanner->device->three_pass_color = 1;

    /* test for adf and uta */
    scan_source = scanner->val[OPT_SOURCE].s;

    if (strcmp(scan_source, UTA_STR) == 0)
    {
      if ( (scanner->device->inquiry_uta != 0) && (scanner->device->inquiry_transavail != 0) )
      {
        scanner->device->uta = 1;
      }
      else
      {
        DBG(DBG_error,"ERROR: Transparency Adapter not available\n");
        umax_scsi_close(scanner->device);
       return SANE_STATUS_INVAL;
      }
    }
    else /* Test if ADF is selected */
    {
      scanner->device->uta = 0;

      if (strcmp(scan_source, ADF_STR) == 0)
      {
        if ( (scanner->device->inquiry_adf) && (scanner->device->inquiry_adfmode) )
        {
          scanner->device->adf = 1;
        }
        else
        {
          DBG(DBG_error,"ERROR: Automatic Document Feeder not available\n");
         umax_scsi_close(scanner->device);
         return SANE_STATUS_INVAL;
        }
      }
      else
      {
        scanner->device->adf = 0;
      }
    }

    if (scanner->device->inquiry_GIB & 32)						/* 16 bit input mode */
    {
      scanner->device->gamma_input_bits_code = 32;
      DBG(DBG_sane_info, "Using 16 bits for gamma input\n");
    }
    else if (scanner->device->inquiry_GIB & 16)						/* 14 bit input mode */
    {
      scanner->device->gamma_input_bits_code = 16;
      DBG(DBG_sane_info, "Using 14 bits for gamma input\n");
    }
    else if (scanner->device->inquiry_GIB & 8)						/* 12 bit input mode */
    {
      scanner->device->gamma_input_bits_code = 8;
      DBG(DBG_sane_info, "Using 12 bits for gamma input\n");
    }
    else if (scanner->device->inquiry_GIB & 4)						/* 10 bit input mode */
    {
      scanner->device->gamma_input_bits_code = 4;
      DBG(DBG_sane_info, "Using 10 bits for gamma input\n");
    }
    else if (scanner->device->inquiry_GIB & 2)						 /* 9 bit input mode */
    {
      scanner->device->gamma_input_bits_code = 2;
      DBG(DBG_sane_info, "Using 9 bits for gamma input\n");
    }
    else										 /* 8 bit input mode */
    {
      scanner->device->gamma_input_bits_code = 1;
      DBG(DBG_sane_info, "Using 8 bits for gamma input\n");
    }

    if (scanner->val[OPT_BIT_DEPTH].w == 16)					       /* 16 bit output mode */
    {
      scanner->device->bits_per_pixel      = 16;
      scanner->device->bits_per_pixel_code = 32;
      scanner->device->max_value      = 65535;
      DBG(DBG_sane_info,"Using 16 bits for output\n");
    }
    else if (scanner->val[OPT_BIT_DEPTH].w == 14)				       /* 14 bit output mode */
    {
      scanner->device->bits_per_pixel      = 14;
      scanner->device->bits_per_pixel_code = 16;
      scanner->device->max_value      = 16383;
      DBG(DBG_sane_info,"Using 14 bits for output\n");
    }
    else if (scanner->val[OPT_BIT_DEPTH].w == 12)				       /* 12 bit output mode */
    {
      scanner->device->bits_per_pixel      = 12;
      scanner->device->bits_per_pixel_code = 8;
      scanner->device->max_value      = 4095;
      DBG(DBG_sane_info,"Using 12 bits for output\n");
    }
    else if (scanner->val[OPT_BIT_DEPTH].w == 10)				       /* 10 bit output mode */
    {
      scanner->device->bits_per_pixel      = 10;
      scanner->device->bits_per_pixel_code = 4;
      scanner->device->max_value      = 1023;
      DBG(DBG_sane_info,"Using 10 bits for output\n");
    }
    else if (scanner->val[OPT_BIT_DEPTH].w == 9)				        /* 9 bit output mode */
    {
      scanner->device->bits_per_pixel      = 9;
      scanner->device->bits_per_pixel_code = 2;
      scanner->device->max_value      = 511;
      DBG(DBG_sane_info,"Using 9 bits for output\n");
    }
    else										/* 8 bit output mode */
    {
      scanner->device->bits_per_pixel      = 8;
      scanner->device->bits_per_pixel_code = 1;
      scanner->device->max_value      = 255;
      DBG(DBG_sane_info,"Using 8 bits for output\n");
    }

    scanner->device->reverse = scanner->device->reverse_multi = scanner->val[OPT_NEGATIVE].w;

    scanner->device->threshold         = P_100_TO_255(scanner->val[OPT_THRESHOLD].w);
    scanner->device->brightness        = P_200_TO_255(scanner->val[OPT_BRIGHTNESS].w);
    scanner->device->contrast          = P_200_TO_255(scanner->val[OPT_CONTRAST].w);

    scanner->device->batch_scan        = ( scanner->val[OPT_BATCH_SCAN_START].w ||
                                           scanner->val[OPT_BATCH_SCAN_LOOP].w ||
                                           scanner->val[OPT_BATCH_SCAN_END].w );
    scanner->device->batch_end         = scanner->val[OPT_BATCH_SCAN_END].w;
    scanner->device->batch_next_tl_y   = SANE_UNFIX(scanner->val[OPT_BATCH_NEXT_TL_Y].w) * scanner->device->y_coordinate_base / MM_PER_INCH;

    if (scanner->val[OPT_BATCH_NEXT_TL_Y].w == 0xFFFF)  /* option not set: use br_y => scanhead stops at end of batch area */
    {
      scanner->device->batch_next_tl_y = SANE_UNFIX(scanner->val[OPT_BR_Y].w) * scanner->device->y_coordinate_base / MM_PER_INCH;
    }

    if ((scanner->device->batch_scan) && !scanner->val[OPT_BATCH_SCAN_START].w)
    {
      scanner->device->calibration = 9; /* no calibration - otherwise the scanhead will go into calibration position */
    }
    else
    {
      scanner->device->calibration = 0; /* calibration defined by image type */
    }

    scanner->device->quality           = scanner->val[OPT_QUALITY].w;
    scanner->device->dor               = scanner->val[OPT_DOR].w;
    scanner->device->preview           = scanner->val[OPT_PREVIEW].w;
    scanner->device->warmup            = scanner->val[OPT_WARMUP].w;

    scanner->device->fix_focus_position   = scanner->val[OPT_FIX_FOCUS_POSITION].w;
    scanner->device->lens_cal_in_doc_pos  = scanner->val[OPT_LENS_CALIBRATION_DOC_POS].w;
    scanner->device->disable_pre_focus    = scanner->val[OPT_DISABLE_PRE_FOCUS].w;
    scanner->device->holder_focus_pos_0mm = scanner->val[OPT_HOLDER_FOCUS_POS_0MM].w;
    scanner->device->manual_focus         = scanner->val[OPT_MANUAL_PRE_FOCUS].w;

    scanner->device->analog_gamma_r =
    scanner->device->analog_gamma_g =
    scanner->device->analog_gamma_b = umax_calculate_analog_gamma(SANE_UNFIX(scanner->val[OPT_ANALOG_GAMMA].w));

    scanner->device->highlight_r =
    scanner->device->highlight_g = 
    scanner->device->highlight_b = P_100_TO_255(scanner->val[OPT_HIGHLIGHT].w);

    scanner->device->shadow_r =
    scanner->device->shadow_g = 
    scanner->device->shadow_b = P_100_TO_255(scanner->val[OPT_SHADOW].w);

    if (scanner->val[OPT_SELECT_EXPOSURE_TIME].w == SANE_TRUE)
    {
      if (scanner->val[OPT_SELECT_CAL_EXPOSURE_TIME].w) /* separate calibration exposure time */
      {
        scanner->device->exposure_time_calibration_r =
        scanner->device->exposure_time_calibration_g =
        scanner->device->exposure_time_calibration_b = scanner->val[OPT_CAL_EXPOS_TIME].w;
      }
      else /* same exposure times for calibration as for scanning */
      {
        scanner->device->exposure_time_calibration_r =
        scanner->device->exposure_time_calibration_g =
        scanner->device->exposure_time_calibration_b = scanner->val[OPT_SCAN_EXPOS_TIME].w;
      }

      scanner->device->exposure_time_scan_r =
      scanner->device->exposure_time_scan_g =
      scanner->device->exposure_time_scan_b = scanner->val[OPT_SCAN_EXPOS_TIME].w;
    }

    if (scanner->val[OPT_SELECT_LAMP_DENSITY].w == SANE_TRUE)
    {
      scanner->device->c_density = P_100_TO_254(scanner->val[OPT_CAL_LAMP_DEN].w);
      scanner->device->s_density = P_100_TO_254(scanner->val[OPT_SCAN_LAMP_DEN].w);
    }

    if (strcmp(mode, LINEART_STR) == 0)
    {
      scanner->device->colormode = LINEART;
    }
    else if (strcmp(mode, HALFTONE_STR) == 0)
    {
      scanner->device->colormode = HALFTONE;
    }
    else if (strcmp(mode, GRAY_STR) == 0)
    {
      scanner->device->colormode = GRAYSCALE;
    }
    else if (strcmp(mode, COLOR_LINEART_STR) == 0)
    {
      scanner->device->colormode = RGB_LINEART;
    }
    else if (strcmp(mode, COLOR_HALFTONE_STR) == 0)
    {
      scanner->device->colormode = RGB_HALFTONE;
    }
    else if (strcmp(mode, COLOR_STR) == 0)
    {
      scanner->device->colormode = RGB;
      if (scanner->val[OPT_RGB_BIND].w == SANE_FALSE)
      {
        scanner->device->analog_gamma_r =
                 umax_calculate_analog_gamma( SANE_UNFIX(scanner->val[OPT_ANALOG_GAMMA_R].w) );
        scanner->device->analog_gamma_g =
                 umax_calculate_analog_gamma( SANE_UNFIX(scanner->val[OPT_ANALOG_GAMMA_G].w) );
        scanner->device->analog_gamma_b =
                 umax_calculate_analog_gamma( SANE_UNFIX(scanner->val[OPT_ANALOG_GAMMA_B].w) );

        scanner->device->highlight_r = P_100_TO_255(scanner->val[OPT_HIGHLIGHT_R].w);
        scanner->device->highlight_g = P_100_TO_255(scanner->val[OPT_HIGHLIGHT_G].w);
        scanner->device->highlight_b = P_100_TO_255(scanner->val[OPT_HIGHLIGHT_B].w);

        scanner->device->shadow_r = P_100_TO_255(scanner->val[OPT_SHADOW_R].w);
        scanner->device->shadow_g = P_100_TO_255(scanner->val[OPT_SHADOW_G].w);
        scanner->device->shadow_b = P_100_TO_255(scanner->val[OPT_SHADOW_B].w);

        if ((scanner->val[OPT_SELECT_EXPOSURE_TIME].w == SANE_TRUE) && (!scanner->device->exposure_time_rgb_bind))
        {
          if (scanner->val[OPT_SELECT_CAL_EXPOSURE_TIME].w) /* separate calibration exposure time */
          {
            scanner->device->exposure_time_calibration_r = scanner->val[OPT_CAL_EXPOS_TIME_R].w;
            scanner->device->exposure_time_calibration_g = scanner->val[OPT_CAL_EXPOS_TIME_G].w;
            scanner->device->exposure_time_calibration_b = scanner->val[OPT_CAL_EXPOS_TIME_B].w;
          }
          else /* same exposure times for calibration as for scanning */
          {
            scanner->device->exposure_time_calibration_r = scanner->val[OPT_SCAN_EXPOS_TIME_R].w;
            scanner->device->exposure_time_calibration_g = scanner->val[OPT_SCAN_EXPOS_TIME_G].w;
            scanner->device->exposure_time_calibration_b = scanner->val[OPT_SCAN_EXPOS_TIME_B].w;
          }

          scanner->device->exposure_time_scan_r = scanner->val[OPT_SCAN_EXPOS_TIME_R].w;
          scanner->device->exposure_time_scan_g = scanner->val[OPT_SCAN_EXPOS_TIME_G].w;
          scanner->device->exposure_time_scan_b = scanner->val[OPT_SCAN_EXPOS_TIME_B].w;
        }
      }
    }

    if (scanner->device->force_preview_bit_rgb != 0)          /* in RGB-mode set preview bit, eg. for UMAX S6E */
    {
      if (scanner->device->colormode == RGB)
      {
        DBG(DBG_sane_info,"setting preview bit = 1 (option force-preview-bit-rgb)\n");
        scanner->device->preview = SANE_TRUE;
      }
    }


#ifdef UMAX_CALIBRATION_MODE_SELECTABLE
    if (strcmp(scanner->val[OPT_CALIB_MODE].s, CALIB_MODE_0000) == 0)
    {
      scanner->device->calibration = 0;
    }
    else if (strcmp(scanner->val[OPT_CALIB_MODE].s, CALIB_MODE_1111) == 0)
    {
      scanner->device->calibration = 15;
    }
    else if (strcmp(scanner->val[OPT_CALIB_MODE].s, CALIB_MODE_1110) == 0)
    {
      scanner->device->calibration = 14;
    }
    else if (strcmp(scanner->val[OPT_CALIB_MODE].s, CALIB_MODE_1101) == 0)
    {
      scanner->device->calibration = 13;
    }
    else if (strcmp(scanner->val[OPT_CALIB_MODE].s, CALIB_MODE_1010) == 0)
    {
      scanner->device->calibration = 10;
    }
    else if (strcmp(scanner->val[OPT_CALIB_MODE].s, CALIB_MODE_1001) == 0)
    {
      scanner->device->calibration = 9;
    }
#endif

    /* get and set geometric values for scanning */
    scanner->device->x_resolution = SANE_UNFIX(scanner->val[OPT_X_RESOLUTION].w);
    scanner->device->y_resolution = SANE_UNFIX(scanner->val[OPT_Y_RESOLUTION].w);

    if ( (scanner->val[OPT_RESOLUTION_BIND].w == SANE_TRUE) || (scanner->val[OPT_PREVIEW].w == SANE_TRUE) )
    {
      scanner->device->y_resolution = scanner->device->x_resolution;
    }

    xbasedots = scanner->device->x_coordinate_base / MM_PER_INCH;
    ybasedots = scanner->device->y_coordinate_base / MM_PER_INCH;

#if 0
    scanner->device->upper_left_x = ((int) (SANE_UNFIX(scanner->val[OPT_TL_X].w) * xbasedots)) & 65534;
    scanner->device->upper_left_y = ((int) (SANE_UNFIX(scanner->val[OPT_TL_Y].w) * ybasedots)) & 65534;

    scanner->device->scanwidth    = ((int)((SANE_UNFIX(scanner->val[OPT_BR_X].w - scanner->val[OPT_TL_X].w)) * xbasedots)) & 65534;
    scanner->device->scanlength   = ((int)((SANE_UNFIX(scanner->val[OPT_BR_Y].w - scanner->val[OPT_TL_Y].w)) * ybasedots)) & 65534;
#endif

    scanner->device->upper_left_x = (int) (SANE_UNFIX(scanner->val[OPT_TL_X].w) * xbasedots);
    scanner->device->upper_left_y = (int) (SANE_UNFIX(scanner->val[OPT_TL_Y].w) * ybasedots);

    scanner->device->scanwidth    = (int)((SANE_UNFIX(scanner->val[OPT_BR_X].w - scanner->val[OPT_TL_X].w)) * xbasedots);
    scanner->device->scanlength   = (int)((SANE_UNFIX(scanner->val[OPT_BR_Y].w - scanner->val[OPT_TL_Y].w)) * ybasedots);


    if (umax_check_values(scanner->device) != 0)
    {
      DBG(DBG_error,"ERROR: invalid scan-values\n");
      scanner->scanning = SANE_FALSE;
     return SANE_STATUS_INVAL;
    }

    /* The scanner defines a x-origin-offset for DOR mode, this offset is used for the */
    /* x range in this backend, so the frontend/user knows the correct positions related to */
    /* scanner's surface. But the scanner wants x values from origin 0 instead */
    /* of the x-origin defined by the scanner`s inquiry */
    if (scanner->device->dor != 0) /* dor mode active */
    {
      DBG(DBG_info,"substracting DOR x-origin-offset from upper left x\n");
      scanner->device->upper_left_x -= scanner->device->inquiry_dor_x_off * scanner->device->x_coordinate_base; /* correct DOR x-origin */

      if (scanner->device->upper_left_x < 0) /* rounding errors may create a negative value */
      {
        scanner->device->upper_left_x = 0; /* but negative values are not allowed */
      }
    }

    scanner->params.bytes_per_line  = scanner->device->row_len;
    scanner->params.pixels_per_line = scanner->device->width_in_pixels; 
    scanner->params.lines           = scanner->device->length_in_pixels;


    /* set exposure times */
    if ( scanner->device->inquiry_exposure_adj )
    {
      umax_calculate_exposure_time(scanner->device, scanner->device->use_exposure_time_def_r, &scanner->device->exposure_time_calibration_r);
      umax_calculate_exposure_time(scanner->device, scanner->device->use_exposure_time_def_g, &scanner->device->exposure_time_calibration_g);
      umax_calculate_exposure_time(scanner->device, scanner->device->use_exposure_time_def_b, &scanner->device->exposure_time_calibration_b);

      umax_calculate_exposure_time(scanner->device, scanner->device->use_exposure_time_def_r, &scanner->device->exposure_time_scan_r);
      umax_calculate_exposure_time(scanner->device, scanner->device->use_exposure_time_def_g, &scanner->device->exposure_time_scan_g);
      umax_calculate_exposure_time(scanner->device, scanner->device->use_exposure_time_def_b, &scanner->device->exposure_time_scan_b);
    }
    else
    {
      scanner->device->exposure_time_calibration_r = scanner->device->exposure_time_calibration_g = scanner->device->exposure_time_calibration_b =
      scanner->device->exposure_time_scan_r = scanner->device->exposure_time_scan_g = scanner->device->exposure_time_scan_b = 0;
    }


    scanner->scanning = SANE_TRUE;
    sane_get_parameters(scanner, 0);

    DBG(DBG_sane_info,"x_resolution (dpi)      = %u\n", scanner->device->x_resolution);
    DBG(DBG_sane_info,"y_resolution (dpi)      = %u\n", scanner->device->y_resolution);
    DBG(DBG_sane_info,"x_coordinate_base (dpi) = %u\n", scanner->device->x_coordinate_base);
    DBG(DBG_sane_info,"y_coordinate_base (dpi) = %u\n", scanner->device->y_coordinate_base);
    DBG(DBG_sane_info,"upper_left_x (xbase)    = %d\n", scanner->device->upper_left_x);
    DBG(DBG_sane_info,"upper_left_y (ybase)    = %d\n", scanner->device->upper_left_y);
    DBG(DBG_sane_info,"scanwidth    (xbase)    = %u\n", scanner->device->scanwidth);
    DBG(DBG_sane_info,"scanlength   (ybase)    = %u\n", scanner->device->scanlength);
    DBG(DBG_sane_info,"width in pixels         = %u\n", scanner->device->width_in_pixels);
    DBG(DBG_sane_info,"length in pixels        = %u\n", scanner->device->length_in_pixels);
    DBG(DBG_sane_info,"bits per pixel/color    = %u\n", scanner->device->bits_per_pixel);
    DBG(DBG_sane_info,"bytes per line          = %d\n", scanner->params.bytes_per_line);
    DBG(DBG_sane_info,"pixels_per_line         = %d\n", scanner->params.pixels_per_line);
    DBG(DBG_sane_info,"lines                   = %d\n", scanner->params.lines);
    DBG(DBG_sane_info,"negative                = %d\n", scanner->device->reverse);
    DBG(DBG_sane_info,"threshold  (lineart)    = %d\n", scanner->device->threshold);
    DBG(DBG_sane_info,"brightness (halftone)   = %d\n", scanner->device->brightness);
    DBG(DBG_sane_info,"contrast   (halftone)   = %d\n", scanner->device->contrast);

    DBG(DBG_sane_info,"analog_gamma            = %d %d %d\n",
            scanner->device->analog_gamma_r,
            scanner->device->analog_gamma_g,
            scanner->device->analog_gamma_b);
    DBG(DBG_sane_info,"highlight               = %d %d %d\n",
            scanner->device->highlight_r,
            scanner->device->highlight_g,
            scanner->device->highlight_b);
    DBG(DBG_sane_info,"shadow                  = %d %d %d\n",
            scanner->device->shadow_r,
            scanner->device->shadow_g,
            scanner->device->shadow_b);
    DBG(DBG_sane_info,"calibrat. exposure time = %d %d %d\n",
            scanner->device->exposure_time_calibration_r,
            scanner->device->exposure_time_calibration_g,
            scanner->device->exposure_time_calibration_b);
    DBG(DBG_sane_info,"scan exposure time      = %d %d %d\n",
            scanner->device->exposure_time_scan_r,
            scanner->device->exposure_time_scan_g,
            scanner->device->exposure_time_scan_b);

#ifdef UMAX_CALIBRATION_MODE_SELECTABLE
    DBG(DBG_sane_info,"calibration             = %s\n", scanner->val[OPT_CALIB_MODE].s);
#endif
    DBG(DBG_sane_info,"calibration mode number = %d\n", scanner->device->calibration);

    DBG(DBG_sane_info,"batch scan              = %d\n", scanner->device->batch_scan);
    DBG(DBG_sane_info,"batch end               = %d\n", scanner->device->batch_end);
    DBG(DBG_sane_info,"batch next top left y   = %d\n", scanner->device->batch_next_tl_y);
    DBG(DBG_sane_info,"quality calibration     = %d\n", scanner->device->quality);
    DBG(DBG_sane_info,"warm up                 = %d\n", scanner->device->warmup);
    DBG(DBG_sane_info,"fast preview function   = %d\n", scanner->device->preview);
    DBG(DBG_sane_info,"DOR                     = %d\n", scanner->device->dor);
    DBG(DBG_sane_info,"ADF                     = %d\n", scanner->device->adf);
    DBG(DBG_sane_info,"manual focus            = %d\n", scanner->device->manual_focus);
    DBG(DBG_sane_info,"fix focus position      = %d\n", scanner->device->fix_focus_position);
    DBG(DBG_sane_info,"disable pre focus       = %d\n", scanner->device->disable_pre_focus);
    DBG(DBG_sane_info,"lens cal in doc pos     = %d\n", scanner->device->lens_cal_in_doc_pos);
    DBG(DBG_sane_info,"holder focus pos 0mm    = %d\n", scanner->device->holder_focus_pos_0mm);

    if (scanner->val[OPT_PREVIEW].w) /* preview mode */
    {
      scanner->device->lines_max = scanner->device->request_preview_lines;
    }
    else /* scan mode */
    {
      scanner->device->lines_max = scanner->device->request_scan_lines;
    }

#ifdef HAVE_SANEI_SCSI_OPEN_EXTENDED
    {
     unsigned int scsi_bufsize  = 0;

      scsi_bufsize = scanner->device->width_in_pixels * scanner->device->lines_max;

      if (scsi_bufsize == 0) /* no scsi buffer size, take scanner buffer size */
      {
        scsi_bufsize = scanner->device->inquiry_vidmem;
      }

      if (scsi_bufsize < scanner->device->scsi_buffer_size_min) /* make sure buffer has at least minimum size */
      {
        scsi_bufsize = scanner->device->scsi_buffer_size_min;
      }
      else if (scsi_bufsize > scanner->device->scsi_buffer_size_max) /* make sure buffer does not exceed maximum size */
      {
        scsi_bufsize = scanner->device->scsi_buffer_size_max;
      }

      if (umax_scsi_open_extended(scanner->device->sane.name, scanner->device, sense_handler,
                                   scanner->device, (int *) &scsi_bufsize) != 0)
      {
        DBG(DBG_error, "ERROR: sane_start: open failed\n");
        scanner->scanning = SANE_FALSE;
        return SANE_STATUS_INVAL;
      }

      if (scsi_bufsize < scanner->device->scsi_buffer_size_min) /* minimum size must be available */
      {
        DBG(DBG_error, "ERROR: sane_start: umax_scsi_open_extended returned too small scsi buffer\n");
        umax_scsi_close((scanner->device));
        scanner->scanning = SANE_FALSE;
        return SANE_STATUS_NO_MEM;
      }
      DBG(DBG_info, "sane_start: umax_scsi_open_extended returned scsi buffer size = %d\n", scsi_bufsize);

      if (scsi_bufsize < scanner->device->width_in_pixels) /* print warning when buffer is smaller than one scanline */
      {
        DBG(DBG_warning, "WARNING: sane_start: scsi buffer is smaller than one scanline\n");
      }

      if (scsi_bufsize != scanner->device->bufsize)
      {
        DBG(DBG_info, "sane_start: buffer size has changed, reallocating buffer\n");

        if (scanner->device->buffer[0])
        {
          DBG(DBG_info, "sane_start: freeing SCSI buffer[0]\n");
          free(scanner->device->buffer[0]);									     /* free buffer */
        }

        scanner->device->bufsize = scsi_bufsize;

        DBG(DBG_info, "sane_start: allocating SCSI buffer[0]\n");
        scanner->device->buffer[0]  = malloc(scanner->device->bufsize);					  /* allocate buffer */

        if (!scanner->device->buffer[0]) /* malloc failed */
        {
          DBG(DBG_error, "ERROR: sane_start: could not allocate buffer[0]\n");
          umax_scsi_close(scanner->device);
          scanner->device->bufsize = 0;
          scanner->scanning = SANE_FALSE;
         return SANE_STATUS_NO_MEM;
        }
      }
    }
#else
    if ( umax_scsi_open(scanner->device->sane.name, scanner->device, sense_handler,
                         scanner->device) != SANE_STATUS_GOOD )
    {
      scanner->scanning = SANE_FALSE;
      DBG(DBG_error, "ERROR: sane_start: open of %s failed:\n", scanner->device->sane.name);
      return SANE_STATUS_INVAL;
    }

    /* there is no need to reallocate the buffer because the size is fixed */
#endif

    /* grab scanner */
    if (umax_grab_scanner(scanner->device))
    {
      umax_scsi_close(scanner->device);
      scanner->scanning = SANE_FALSE;
      DBG(DBG_warning,"WARNING: unable to reserve scanner: device busy\n");
     return SANE_STATUS_DEVICE_BUSY;
    }

/* halftone pattern download is not ready in this version */
#if 0
										     /* send halftonepattern */
    if ( (strcmp(mode, HALFTONE_STR) == 0) || (strcmp(mode, COLOR_HALFTONE_STR) == 0) )
    {
      umax_send_halftone_pattern(scanner->device, (char *) &(scanner->halftone_pattern[0]),
                                 scanner->val[OPT_HALFTONE_DIMENSION].w );
      scanner->device->halftone = WD_halftone_download;
    }									      /* end of send halftonepattern */
#endif
 
  } /* ------------ end of first call -------------- */


  /* send gammacurves */
  if (scanner->val[OPT_CUSTOM_GAMMA].w == SANE_TRUE)
  {
    if (strcmp(mode, COLOR_STR) == 0)
    {
      if (scanner->device->three_pass == 0)					      /* one pass color scan */
      {
       unsigned int i, dest, color, value;
       char *gamma;

        gamma = malloc( (size_t) (3 * scanner->gamma_length * scanner->output_bytes) );
        if (gamma == NULL)
        {
          DBG(DBG_warning,"WARNING: not able to allocate memory for gamma table, gamma ignored !!!\n");
        }
        else
        {
          dest=0;
          for(color=1; color <= 3; color++)
          {
            for(i=0; i < scanner->gamma_length; i++)
            {
              value = scanner->gamma_table[color][i];
              if (scanner->output_bytes == 2)
              {
                gamma[dest++] = scanner->gamma_table[0][value] / 256;
              }
              gamma[dest++] = (scanner->gamma_table[0][value] & 255);
            }
          }

          DBG(DBG_sane_info,"sending 3 * %d bytes of gamma data for RGB\n",
              scanner->gamma_length * scanner->output_bytes);

          umax_send_gamma_data(scanner->device, &gamma[0], 3);
          scanner->device->digital_gamma_r =
          scanner->device->digital_gamma_g =
          scanner->device->digital_gamma_b = WD_gamma_download;
          free(gamma);
        }
      }
      else									    /* three pass color scan */
      {
       unsigned int i, dest, color, value;
       char *gamma;

        gamma = malloc( (size_t) (scanner->gamma_length * scanner->output_bytes) );
        if (gamma == NULL)
        {
          DBG(DBG_warning,"not able to allocate memory for gamma table, gamma ignored !!!\n");
        }
        else
        {
          dest  = 0;
          color = scanner->device->three_pass_color;

          for(i = 0; i < scanner->gamma_length; i++)
          {
            value = scanner->gamma_table[color][i];

            if (scanner->output_bytes == 2)
            {
              gamma[dest++] = scanner->gamma_table[0][value] / 256;
            }
            gamma[dest++] = (scanner->gamma_table[0][value] & 255);
          }

          DBG(DBG_sane_info,"sending %d bytes of gamma data for color %d\n",
              scanner->gamma_length * scanner->output_bytes, color);

          umax_send_gamma_data(scanner->device, &gamma[0], 1);
          scanner->device->digital_gamma_r =
          scanner->device->digital_gamma_g =
          scanner->device->digital_gamma_b = WD_gamma_download;
          free(gamma);
        }
      }
    }
    else if (strcmp(mode, GRAY_STR) == 0) /* grayscale scan */
    {
     unsigned int i, dest;
     char *gamma;

      gamma = malloc( (size_t) (scanner->gamma_length * scanner->output_bytes) );
      if (gamma == NULL)
      {
        DBG(DBG_warning,"WARNING: not able to allocate memory for gamma table, gamma ignored !!!\n");
      }
      else
      {
        dest=0;
        for(i=0; i < scanner->gamma_length; i++)
        {
            if (scanner->output_bytes == 2)
            {
              gamma[dest++] = scanner->gamma_table[0][i] / 256;
            }
            gamma[dest++] = (scanner->gamma_table[0][i] & 255);
        }

        DBG(DBG_sane_info,"sending %d bytes of gamma data for gray\n",
            scanner->gamma_length * scanner->output_bytes);

        umax_send_gamma_data(scanner->device, &gamma[0], 1);
        scanner->device->digital_gamma_r = WD_gamma_download;
	free(gamma);
      }
    }
  }										  /* end of send gammacurves */

  if ( scanner->device->three_pass_color > WD_wid_red) /* three pass scan, not first pass */
  {
    umax_reposition_scanner(scanner->device);
  }

  umax_set_window_param(scanner->device);
  status = umax_start_scan(scanner->device);
  if (status) /* errror */
  {
    umax_give_scanner(scanner->device); /* reposition and release scanner */
    return status;
  }

  pause = scanner->device->pause_for_color_calibration;

  if (scanner->device->colormode != RGB)
  {
    pause = scanner->device->pause_for_gray_calibration;
  }

  if (pause) /* Astra 2400S needs this pause (7sec in color, 4sec in gray mode) */
  {
    DBG(DBG_info2,"pause for calibration %d msec ...\n", pause);
    usleep(((long) pause) * 1000); /* time in ms */
    DBG(DBG_info2,"pause done\n");
  }

  status = umax_do_calibration(scanner->device);
  if (status) /* errror */
  {
    umax_give_scanner(scanner->device); /* reposition and release scanner */
    return status;
  }

  if (scanner->device->pause_after_calibration) /* may be usefull */
  {
    DBG(DBG_info2,"pause after calibration %d msec ...\n", scanner->device->pause_after_calibration);
    usleep(((long) scanner->device->pause_after_calibration) * 1000); /* time in ms */
    DBG(DBG_info2,"pause done\n");
  }


  if (pipe(fds) < 0)
  {
    DBG(DBG_error,"ERROR: could not create pipe\n");
    scanner->scanning = SANE_FALSE;
    umax_give_scanner(scanner->device); /* reposition and release scanner */
    umax_scsi_close(scanner->device);
   return SANE_STATUS_IO_ERROR;
  }

  scanner->pipe_read_fd  = fds[0]; 
  scanner->pipe_write_fd = fds[1];

  /* start reader_process, deponds on OS if fork() or threads are used */
  scanner->reader_pid = sanei_thread_begin(reader_process, (void *) scanner);

  if (scanner->reader_pid == -1)
  {
    DBG(DBG_error, "ERROR: sanei_thread_begin failed (%s)\n", strerror(errno));
    scanner->scanning = SANE_FALSE;
    umax_give_scanner(scanner->device); /* reposition and release scanner */
    umax_scsi_close(scanner->device);
    return SANE_STATUS_NO_MEM; /* any other reason than no memory possible ? */
  }

  if (sanei_thread_is_forked())
  {
    close(scanner->pipe_write_fd);
    scanner->pipe_write_fd = -1;
  }

 return SANE_STATUS_GOOD;
}


/* ------------------------------------------------------------ SANE READ ---------------------------------- */


SANE_Status sane_read(SANE_Handle handle, SANE_Byte *buf, SANE_Int max_len, SANE_Int *len)
{
 Umax_Scanner *scanner = handle;
 ssize_t nread;

  *len = 0;

  nread = read(scanner->pipe_read_fd, buf, max_len);

  DBG(DBG_sane_info, "sane_read: read %ld bytes\n", (long) nread);

  if (!(scanner->scanning)) /* OOPS, not scanning */
  {
    return do_cancel(scanner);
  }

  if (nread < 0)
  {
    if (errno == EAGAIN)
    {
      DBG(DBG_sane_info, "sane_read: EAGAIN\n");
      return SANE_STATUS_GOOD;
    }
    else
    {
      do_cancel(scanner); /* we had an error, stop scanner */
     return SANE_STATUS_IO_ERROR;
    }
  }

  *len = nread;

  if (nread == 0) /* EOF */
  {
    if ( (scanner->device->three_pass == 0) ||
         (scanner->device->colormode <= RGB_LINEART) ||
         (++(scanner->device->three_pass_color) > 3) )
    {
      do_cancel(scanner);
    }

    DBG(DBG_sane_proc,"closing read end of pipe\n");

    if (scanner->pipe_read_fd >= 0)
    {
      close(scanner->pipe_read_fd);
      scanner->pipe_read_fd = -1;
    }

    return SANE_STATUS_EOF;    
  }

 return SANE_STATUS_GOOD;
}


/* ------------------------------------------------------------ SANE CANCEL -------------------------------- */


void sane_cancel(SANE_Handle handle)
{
 Umax_Scanner *scanner = handle;

  DBG(DBG_sane_init,"sane_cancel\n");

  if (scanner->scanning)
  {
    do_cancel(scanner);
  }
}


/* ------------------------------------------------------------ SANE SET IO MODE --------------------------- */


SANE_Status sane_set_io_mode(SANE_Handle handle, SANE_Bool non_blocking)
{
 Umax_Scanner *scanner = handle;

  DBG(DBG_sane_init,"sane_set_io_mode: non_blocking=%d\n", non_blocking);

  if (!scanner->scanning) { return SANE_STATUS_INVAL; }

  if (fcntl(scanner->pipe_read_fd, F_SETFL, non_blocking ? O_NONBLOCK : 0) < 0)
  {
    return SANE_STATUS_IO_ERROR;
  }  
 return SANE_STATUS_GOOD;  
}


/* ------------------------------------------------------------ SANE GET SELECT FD ------------------------- */


SANE_Status sane_get_select_fd(SANE_Handle handle, SANE_Int *fd)
{
 Umax_Scanner *scanner = handle;

  DBG(DBG_sane_init,"sane_get_select_fd\n");

  if (!scanner->scanning)
  {
    return SANE_STATUS_INVAL;
  }

  *fd = scanner->pipe_read_fd;

 return SANE_STATUS_GOOD;
}

/* ------------------------------------------------------------ EOF ---------------------------------------- */
