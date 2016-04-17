#ifndef SP15C_H
#define SP15C_H

static const char RCSid_h[] = "$Header$";
/* sane - Scanner Access Now Easy.

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

   This file implements a SANE backend for Fujitsu ScanParner 15c
   flatbed/ADF scanners.  It was derived from the COOLSCAN driver.
   Written by Randolph Bentson <bentson@holmsjoen.com> */

/* ------------------------------------------------------------------------- */
/*
 * $Log$
 * Revision 1.8  2008/05/15 12:50:24  ellert-guest
 * Fix for bug #306751: sanei-thread with pthreads on 64 bit
 *
 * Revision 1.7  2005-09-19 19:57:48  fzago-guest
 * Replaced __unused__ with __sane_unused__ to avoid a namespace conflict.
 *
 * Revision 1.6  2004/11/13 19:53:04  fzago-guest
 * Fixes some warnings.
 *
 * Revision 1.5  2004/05/23 17:28:56  hmg-guest
 * Use sanei_thread instead of fork() in the unmaintained backends.
 * Patches from Mattias Ellert (bugs: 300635, 300634, 300633, 300629).
 *
 * Revision 1.4  2003/12/27 17:48:38  hmg-guest
 * Silenced some compilation warnings.
 *
 * Revision 1.3  2000/08/12 15:09:42  pere
 * Merge devel (v1.0.3) into head branch.
 *
 * Revision 1.1.2.3  2000/03/14 17:47:14  abel
 * new version of the Sharp backend added.
 *
 * Revision 1.1.2.2  2000/01/26 03:51:50  pere
 * Updated backends sp15c (v1.12) and m3096g (v1.11).
 *
 * Revision 1.7  2000/01/05 05:22:26  bentson
 * indent to barfable GNU style
 *
 * Revision 1.6  1999/12/03 20:57:13  bentson
 * add MEDIA CHECK command
 *
 * Revision 1.5  1999/11/24 15:55:56  bentson
 * remove some debug stuff; rename function
 *
 * Revision 1.4  1999/11/23 18:54:26  bentson
 * tidy up function types for constraint checking
 *
 * Revision 1.3  1999/11/23 06:41:54  bentson
 * add debug flag to interface
 *
 * Revision 1.2  1999/11/22 18:15:20  bentson
 * more work on color support
 *
 * Revision 1.1  1999/11/19 15:09:08  bentson
 * cribbed from m3096g
 *
 */

static const SANE_Device **devlist = NULL;
static int num_devices;
static struct sp15c *first_dev;

enum sp15c_Option
  {
    OPT_NUM_OPTS = 0,

    OPT_MODE_GROUP,
    OPT_SOURCE,
    OPT_MODE,
    OPT_TYPE,
    OPT_X_RES,
    OPT_Y_RES,
    OPT_PRESCAN,
    OPT_PREVIEW_RES,

    OPT_GEOMETRY_GROUP,
    OPT_TL_X,			/* in mm/2^16 */
    OPT_TL_Y,			/* in mm/2^16 */
    OPT_BR_X,			/* in mm/2^16 */
    OPT_BR_Y,			/* in mm/2^16 */

    OPT_ENHANCEMENT_GROUP,
    OPT_AVERAGING,
    OPT_BRIGHTNESS,
    OPT_THRESHOLD,

    OPT_ADVANCED_GROUP,
    OPT_PREVIEW,

    /* must come last: */
    NUM_OPTIONS
  };

struct sp15c
  {
    struct sp15c *next;

    SANE_Option_Descriptor opt[NUM_OPTIONS];
    SANE_Device sane;

    char vendor[9];
    char product[17];
    char version[5];

    char *devicename;		/* name of the scanner device */
    int sfd;			/* output file descriptor, scanner device */
    int pipe;
    int reader_pipe;

    int scanning;		/* "in progress" flag */
    int autofeeder;		/* detected */
    int use_adf;		/* requested */
    SANE_Pid reader_pid;	/* child is running */
    int prescan;		/* ??? */

/***** terms for "set window" command *****/
    int x_res;			/* resolution in */
    int y_res;			/* pixels/inch */
    int tl_x;			/* top left position, */
    int tl_y;			/* in inch/1200 units */
    int br_x;			/* bottom right position, */
    int br_y;			/* in inch/1200 units */

    int brightness;
    int threshold;
    int contrast;
    int composition;
    int bitsperpixel;		/* at the scanner interface */
    int halftone;
    int rif;
    int bitorder;
    int compress_type;
    int compress_arg;
    int vendor_id_code;
    int outline;
    int emphasis;
    int auto_sep;
    int mirroring;
    int var_rate_dyn_thresh;
    int white_level_follow;
    int subwindow_list;
    int paper_size;
    int paper_width_X;
    int paper_length_Y;
/***** end of "set window" terms *****/

    /* buffer used for scsi-transfer */
    unsigned char *buffer;
    unsigned int row_bufsize;

  };

/* ------------------------------------------------------------------------- */

#define length_quant SANE_UNFIX(SANE_FIX(MM_PER_INCH / 1200.0))
#define mmToIlu(mm) ((mm) / length_quant)
#define iluToMm(ilu) ((ilu) * length_quant)
#define SP15C_CONFIG_FILE "sp15c.conf"

/* ------------------------------------------------------------------------- */

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize);

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only);

SANE_Status
sane_open (SANE_String_Const name, SANE_Handle * handle);

SANE_Status
sane_set_io_mode (SANE_Handle h, SANE_Bool non_blocking);

SANE_Status
sane_get_select_fd (SANE_Handle h, SANE_Int * fdp);

const SANE_Option_Descriptor *
  sane_get_option_descriptor (SANE_Handle handle, SANE_Int option);

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info);

SANE_Status
sane_start (SANE_Handle handle);

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params);

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf,
	   SANE_Int max_len, SANE_Int * len);

void
  sane_cancel (SANE_Handle h);

void
  sane_close (SANE_Handle h);

void
  sane_exit (void);

/* ------------------------------------------------------------------------- */

static SANE_Status
  attach_scanner (const char *devicename, struct sp15c **devp);

static SANE_Status
  sense_handler (int scsi_fd, u_char * result, void *arg);

static int
  request_sense_parse (u_char * sensed_data);

static SANE_Status
  sp15c_identify_scanner (struct sp15c *s);

static SANE_Status
  sp15c_do_inquiry (struct sp15c *s);

static SANE_Status
  do_scsi_cmd (int fd, unsigned char *cmd, int cmd_len, unsigned char *out, size_t out_len);

static void
  hexdump (int level, char *comment, unsigned char *p, int l);

static SANE_Status
  init_options (struct sp15c *scanner);

static int
  sp15c_check_values (struct sp15c *s);

static int
  sp15c_grab_scanner (struct sp15c *s);

static int
  sp15c_free_scanner (struct sp15c *s);

static int
  wait_scanner (struct sp15c *s);

static int __sane_unused__
  sp15c_object_position (struct sp15c *s);

static SANE_Status
  do_cancel (struct sp15c *scanner);

static void
  swap_res (struct sp15c *s);

static int __sane_unused__
  sp15c_object_discharge (struct sp15c *s);

static int
  sp15c_set_window_param (struct sp15c *s, int prescan);

static size_t
  max_string_size (const SANE_String_Const strings[]);

static int
  sp15c_start_scan (struct sp15c *s);

static int
  reader_process (void *scanner);

static SANE_Status
  do_eof (struct sp15c *scanner);

static int
  pixels_per_line (struct sp15c *s);

static int
  lines_per_scan (struct sp15c *s);

static int
  bytes_per_line (struct sp15c *s);

static void
  sp15c_trim_rowbufsize (struct sp15c *s);

static int
  sp15c_read_data_block (struct sp15c *s, unsigned int length);

static SANE_Status
  attach_one (const char *name);

static void
  adjust_width (struct sp15c *s, SANE_Int * info);

static SANE_Status
  apply_constraints (struct sp15c *s, SANE_Int opt,
		     SANE_Int * target, SANE_Word * info);

static int
  sp15c_media_check (struct sp15c *s);

#endif /* SP15C_H */
