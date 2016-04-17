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

   --------------------------------------------------------------------------

   This file implements a SANE backend for various large Kodak scanners.

   The source code is divided in sections which you can easily find by
   searching for the tag "@@".

   Section 1 - Init & static stuff
   Section 2 - sane_init, _get_devices, _open & friends
   Section 3 - sane_*_option functions
   Section 4 - sane_start, _get_param, _read & friends
   Section 5 - sane_close functions
   Section 6 - misc functions

   Changes:
      v0 through v5 2008-01-15, MAN
         - development versions
      v6 2009-06-22, MAN
         - improved set_window() to build desciptor from scratch
         - initial release
      v7 2010-02-10, MAN
         - add SANE_I18N to static strings
         - don't fail if scsi buffer is too small

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

#include "sane/config.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

#include <sys/types.h>
#include <unistd.h>
#ifdef HAVE_LIBC_H
# include <libc.h>              /* NeXTStep/OpenStep */
#endif

#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_scsi.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_config.h"

#include "kodak-cmd.h"
#include "kodak.h"

#define DEBUG 1
#define BUILD 7 

/* values for SANE_DEBUG_KODAK env var:
 - errors           5
 - function trace  10
 - function detail 15
 - get/setopt cmds 20
 - scsi cmd trace  25 
 - scsi cmd detail 30
 - useless noise   35
*/

/* ------------------------------------------------------------------------- */
#define STRING_ADFFRONT SANE_I18N("ADF Front")
#define STRING_ADFBACK SANE_I18N("ADF Back")
#define STRING_ADFDUPLEX SANE_I18N("ADF Duplex")

#define STRING_LINEART SANE_VALUE_SCAN_MODE_LINEART
#define STRING_HALFTONE SANE_VALUE_SCAN_MODE_HALFTONE
#define STRING_GRAYSCALE SANE_VALUE_SCAN_MODE_GRAY
#define STRING_COLOR SANE_VALUE_SCAN_MODE_COLOR

/* Also set via config file. */
static int global_buffer_size = DEFAULT_BUFFER_SIZE;

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
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, BUILD);

  DBG (5, "sane_init: kodak backend %d.%d.%d, from %s\n",
    V_MAJOR, V_MINOR, BUILD, PACKAGE_STRING);

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
/* Read the config file, find scanners with help from sanei_*
 * store in two global lists of device structs
 */
SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  struct scanner *dev;
  char line[PATH_MAX];
  const char *lp;
  FILE *fp;
  int num_devices=0;
  int i=0;

  local_only = local_only;        /* get rid of compiler warning */

  DBG (10, "sane_get_devices: start\n");

  /* set this to default before reading the file */
  global_buffer_size = DEFAULT_BUFFER_SIZE;

  fp = sanei_config_open (KODAK_CONFIG_FILE);

  if (fp) {

      DBG (15, "sane_get_devices: reading config file %s\n", KODAK_CONFIG_FILE);

      while (sanei_config_read (line, PATH_MAX, fp)) {
    
          lp = line;
    
          /* ignore comments */
          if (*lp == '#')
            continue;
    
          /* skip empty lines */
          if (*lp == 0)
            continue;
    
          if ((strncmp ("option", lp, 6) == 0) && isspace (lp[6])) {
    
              lp += 6;
              lp = sanei_config_skip_whitespace (lp);
    
              /* we allow setting buffersize too big */
              if ((strncmp (lp, "buffer-size", 11) == 0) && isspace (lp[11])) {
    
                  int buf;
                  lp += 11;
                  lp = sanei_config_skip_whitespace (lp);
                  buf = atoi (lp);
    
                  if (buf < 4096) {
                    DBG (5, "sane_get_devices: config option \"buffer-size\" \
                      (%d) is < 4096, ignoring!\n", buf);
                    continue;
                  }
    
                  if (buf > DEFAULT_BUFFER_SIZE) {
                    DBG (5, "sane_get_devices: config option \"buffer-size\" \
                      (%d) is > %d, warning!\n", buf, DEFAULT_BUFFER_SIZE);
                  }
    
                  DBG (15, "sane_get_devices: setting \"buffer-size\" to %d\n",
                    buf);
                  global_buffer_size = buf;
              }
              else {
                  DBG (5, "sane_get_devices: config option \"%s\" \
                    unrecognized\n", lp);
              }
          }
          else if ((strncmp ("scsi", lp, 4) == 0) && isspace (lp[4])) {
              DBG (15, "sane_get_devices: looking for '%s'\n", lp);
              sanei_config_attach_matching_devices (lp, attach_one);
          }
          else{
              DBG (5, "sane_get_devices: config line \"%s\" unrecognized\n",
                lp);
          }
      }
      fclose (fp);
  }

  else {
      DBG (5, "sane_get_devices: no config file '%s', using defaults\n",
        KODAK_CONFIG_FILE);
      DBG (15, "sane_get_devices: looking for 'scsi KODAK'\n");
      sanei_config_attach_matching_devices ("scsi KODAK", attach_one);
  }

  for (dev = scanner_devList; dev; dev=dev->next) {
    DBG (15, "sane_get_devices: found scanner %s\n",dev->device_name);
    num_devices++;
  }

  DBG (15, "sane_get_devices: found %d scanner(s)\n",num_devices);

  sane_devArray = calloc (num_devices + 1, sizeof (SANE_Device*));
  if (!sane_devArray)
    return SANE_STATUS_NO_MEM;

  for (dev = scanner_devList; dev; dev=dev->next) {
    sane_devArray[i++] = (SANE_Device *)&dev->sane;
  }

  sane_devArray[i] = 0;

  if(device_list){
    *device_list = sane_devArray;
  }

  DBG (10, "sane_get_devices: finish\n");

  return SANE_STATUS_GOOD;
}

/* build the scanner struct and link to global list 
 * unless struct is already loaded, then pretend 
 */
static SANE_Status
attach_one (const char *device_name)
{
  struct scanner *s;
  int ret;

  DBG (10, "attach_one: start\n");
  DBG (15, "attach_one: looking for '%s'\n", device_name);

  for (s = scanner_devList; s; s = s->next) {
    if (strcmp (s->sane.name, device_name) == 0) {
      DBG (10, "attach_one: already attached!\n");
      return SANE_STATUS_GOOD;
    }
  }

  /* build a struct to hold it */
  if ((s = calloc (sizeof (*s), 1)) == NULL)
    return SANE_STATUS_NO_MEM;

  /* scsi command/data buffer */
  s->buffer_size = global_buffer_size;

  /* copy the device name */
  s->device_name = strdup (device_name);
  if (!s->device_name){
    free (s);
    return SANE_STATUS_NO_MEM;
  }

  /* connect the fd */
  s->fd = -1;
  ret = connect_fd(s);
  if(ret != SANE_STATUS_GOOD){
    free (s->device_name);
    free (s);
    return ret;
  }

  /* Now query the device to load its vendor/model/version */
  ret = init_inquire (s);
  if (ret != SANE_STATUS_GOOD) {
    disconnect_fd(s);
    free (s->device_name);
    free (s);
    DBG (5, "attach_one: inquiry failed\n");
    return ret;
  }

  /* clean up the scanner struct based on model */
  /* this is the only piece of model specific code */
  ret = init_model (s);
  if (ret != SANE_STATUS_GOOD) {
    disconnect_fd(s);
    free (s->device_name);
    free (s);
    DBG (5, "attach_one: model failed\n");
    return ret;
  }

  /* sets user 'values' to good defaults */
  ret = init_user (s);
  if (ret != SANE_STATUS_GOOD) {
    disconnect_fd(s);
    free (s->device_name);
    free (s);
    DBG (5, "attach_one: user failed\n");
    return ret;
  }

  /* sets SANE option 'values' to good defaults */
  ret = init_options (s);
  if (ret != SANE_STATUS_GOOD) {
    disconnect_fd(s);
    free (s->device_name);
    free (s);
    DBG (5, "attach_one: options failed\n");
    return ret;
  }

  /* we close the connection, so that another backend can talk to scanner */
  disconnect_fd(s);

  /* load info into sane_device struct */
  s->sane.name = s->device_name;
  s->sane.vendor = s->vendor_name;
  s->sane.model = s->product_name;
  s->sane.type = "scanner";

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
  SANE_Status ret = SANE_STATUS_GOOD;
  int buffer_size = s->buffer_size;

  DBG (10, "connect_fd: start\n");

  if(s->fd > -1){
    DBG (5, "connect_fd: already open\n");
    ret = SANE_STATUS_GOOD;
  }
  else {
    ret = sanei_scsi_open_extended (s->device_name, &(s->fd), sense_handler,
      s, &s->buffer_size);
    if(!ret && buffer_size != s->buffer_size){
      DBG (5, "connect_fd: cannot get requested buffer size (%d/%d)\n",
        buffer_size, s->buffer_size);
    }
    else{
      DBG (15, "connect_fd: opened SCSI device\n");
    }
  }

  DBG (10, "connect_fd: finish %d\n", ret);

  return ret;
}

/*
 * This routine will check if a certain device is a Kodak scanner
 * It also copies interesting data from INQUIRY into the handle structure
 */
static SANE_Status
init_inquire (struct scanner *s)
{
  int i;
  SANE_Status ret;

  unsigned char cmd[INQUIRY_len];
  size_t cmdLen = INQUIRY_len;

  unsigned char in[I_data_len];
  size_t inLen = I_data_len;

  DBG (10, "init_inquire: start\n");

  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd, INQUIRY_code);
  set_I_evpd (cmd, 0);
  set_I_page_code (cmd, I_page_code_default);
  set_I_data_length (cmd, inLen);
 
  ret = do_cmd (
    s, 1, 0, 
    cmd, cmdLen,
    NULL, 0,
    in, &inLen
  );

  if (ret != SANE_STATUS_GOOD){
    return ret;
  }

  if (get_I_periph_qual(in) != I_periph_qual_valid){
    DBG (5, "The device at '%s' has invalid periph_qual.\n", s->device_name);
    return SANE_STATUS_INVAL;
  }

  if (get_I_periph_devtype(in) != I_periph_devtype_scanner){
    DBG (5, "The device at '%s' is not a scanner.\n", s->device_name);
    return SANE_STATUS_INVAL;
  }

  get_I_vendor (in, s->vendor_name);
  get_I_product (in, s->product_name);
  get_I_version (in, s->version_name);
  get_I_build (in, s->build_name);

  s->vendor_name[8] = 0;
  s->product_name[16] = 0;
  s->version_name[4] = 0;
  s->build_name[2] = 0;

  /* gobble trailing spaces */
  for (i = 7; s->vendor_name[i] == ' ' && i >= 0; i--)
    s->vendor_name[i] = 0;
  for (i = 15; s->product_name[i] == ' ' && i >= 0; i--)
    s->product_name[i] = 0;
  for (i = 3; s->version_name[i] == ' ' && i >= 0; i--)
    s->version_name[i] = 0;
  for (i = 2; s->build_name[i] == ' ' && i >= 0; i--)
    s->build_name[i] = 0;

  if (strcmp ("KODAK", s->vendor_name)) {
    DBG (5, "The device at '%s' is reported to be made by '%s'\n", s->device_name, s->vendor_name);
    DBG (5, "This backend only supports Kodak products.\n");
    return SANE_STATUS_INVAL;
  }

  DBG (15, "init_inquire: Found '%s' '%s' '%s' '%s' at '%s'\n",
    s->vendor_name, s->product_name, s->version_name, s->build_name,
    s->device_name);

  /*defined in SCSI spec*/
  DBG (15, "standard inquiry options\n");

  /*FIXME: do we need to save these?*/
  DBG (15, "  PQ: %d\n",get_I_periph_qual(in));
  DBG (15, "  PDT: %d\n",get_I_periph_devtype(in));

  DBG (15, "  RMB: %d\n",get_I_rmb(in));
  DBG (15, "  DTQ: %d\n",get_I_devtype_qual(in));

  DBG (15, "  ISO: %d\n",get_I_iso_version(in));
  DBG (15, "  ECMA: %d\n",get_I_ecma_version(in));
  DBG (15, "  ANSI: %d\n",get_I_ansi_version(in));

  DBG (15, "  AENC: %d\n",get_I_aenc(in));
  DBG (15, "  TrmIOP: %d\n",get_I_trmiop(in));
  DBG (15, "  RDF: %d\n",get_I_resonse_format(in));

  DBG (15, "  Length: %d\n",get_I_length(in));

  DBG (15, "  RelAdr: %d\n",get_I_reladr(in));
  DBG (15, "  WBus32: %d\n",get_I_wbus32(in));
  DBG (15, "  WBus16: %d\n",get_I_wbus16(in));
  DBG (15, "  Sync: %d\n",get_I_sync(in));
  DBG (15, "  Linked: %d\n",get_I_linked(in));
  DBG (15, "  CmdQue: %d\n",get_I_cmdque(in));
  DBG (15, "  SftRe: %d\n",get_I_sftre(in));

  /*kodak specific*/
  DBG (15, "vendor inquiry options\n");

  DBG (15, "  MF Disable: %d\n",get_I_mf_disable(in));
  DBG (15, "  Checkdigit: %d\n",get_I_checkdigit(in));
  DBG (15, "  Front Prism: %d\n",get_I_front_prism(in));
  DBG (15, "  Comp Gray: %d\n",get_I_compressed_gray(in));
  DBG (15, "  Front Toggle: %d\n",get_I_front_toggle(in));
  DBG (15, "  Front DP1: %d\n",get_I_front_dp1(in));
  DBG (15, "  Front Color: %d\n",get_I_front_color(in));
  DBG (15, "  Front ATP: %d\n",get_I_front_atp(in));

  DBG (15, "  DP1 180: %d\n",get_I_dp1_180(in));
  DBG (15, "  MF Pause: %d\n",get_I_mf_pause(in));
  DBG (15, "  Rear Prism: %d\n",get_I_rear_prism(in));
  DBG (15, "  Uncomp Gray: %d\n",get_I_uncompressed_gray(in));
  DBG (15, "  Rear Toggle: %d\n",get_I_rear_toggle(in));
  DBG (15, "  Rear DP1: %d\n",get_I_rear_dp1(in));
  DBG (15, "  Rear Color: %d\n",get_I_rear_color(in));
  DBG (15, "  Rear ATP: %d\n",get_I_rear_atp(in));

  /* we actually care about these */
  DBG (15, "  Min Binary Res: %d\n",get_I_min_bin_res(in));
  s->s_res_min[MODE_LINEART] = get_I_min_bin_res(in);
  DBG (15, "  Max Binary Res: %d\n",get_I_max_bin_res(in));
  s->s_res_max[MODE_LINEART] = get_I_max_bin_res(in);
  DBG (15, "  Min Color Res: %d\n",get_I_min_col_res(in));
  s->s_res_min[MODE_COLOR] = get_I_min_col_res(in);
  DBG (15, "  Max Color Res: %d\n",get_I_max_col_res(in));
  s->s_res_max[MODE_COLOR] = get_I_max_col_res(in);

  DBG (15, "  Max Width: %d\n",get_I_max_image_width(in));
  s->s_width_max = get_I_max_image_width(in);
  DBG (15, "  Max Length: %d\n",get_I_max_image_length(in));
  s->s_length_max = get_I_max_image_length(in);

  /*FIXME: do we need to save these?*/
  DBG (15, "  Finecrop: %d\n",get_I_finecrop(in));
  DBG (15, "  iThresh: %d\n",get_I_ithresh(in));
  DBG (15, "  ECD: %d\n",get_I_ecd(in));
  DBG (15, "  VBLR: %d\n",get_I_vblr(in));
  DBG (15, "  Elevator: %d\n",get_I_elevator(in));
  DBG (15, "  RelCrop: %d\n",get_I_relcrop(in));

  DBG (15, "  CDeskew: %d\n",get_I_cdeskew(in));
  DBG (15, "  IA: %d\n",get_I_ia(in));
  DBG (15, "  Patch: %d\n",get_I_patch(in));
  DBG (15, "  Null Mode: %d\n",get_I_nullmode(in));
  DBG (15, "  SABRE: %d\n",get_I_sabre(in));
  DBG (15, "  LDDDS: %d\n",get_I_lddds(in));
  DBG (15, "  UDDDS: %d\n",get_I_uddds(in));
  DBG (15, "  Fixed Gap: %d\n",get_I_fixedgap(in));

  DBG (15, "  HR Printer: %d\n",get_I_hr_printer(in));
  DBG (15, "  Elev 100/250: %d\n",get_I_elev_100_250(in));
  DBG (15, "  UDDS Individual: %d\n",get_I_udds_individual(in));
  DBG (15, "  Auto Color: %d\n",get_I_auto_color(in));
  DBG (15, "  WB: %d\n",get_I_wb(in));
  DBG (15, "  ES: %d\n",get_I_es(in));
  DBG (15, "  FC: %d\n",get_I_fc(in));

  DBG (15, "  Max Rate: %d\n",get_I_max_rate(in));
  DBG (15, "  Buffer Size: %d\n",get_I_buffer_size(in));

  DBG (10, "init_inquire: finish\n");

  return SANE_STATUS_GOOD;
}

/*
 * get model specific info that is not in vpd, and correct
 * errors in vpd data. struct is already initialized to 0.
 */
static SANE_Status
init_model (struct scanner *s)
{

  DBG (10, "init_model: start\n");

  s->s_mode[MODE_LINEART] = 1;
  s->s_mode[MODE_HALFTONE] = 1;
  s->s_mode[MODE_GRAYSCALE] = 1;
  s->s_mode[MODE_COLOR] = 1;
 
  /* scanner did not tell us these */
  s->s_res_min[MODE_HALFTONE] = s->s_res_min[MODE_LINEART];
  s->s_res_max[MODE_HALFTONE] = s->s_res_max[MODE_LINEART];

  s->s_res_min[MODE_GRAYSCALE] = s->s_res_min[MODE_COLOR];
  s->s_res_max[MODE_GRAYSCALE] = s->s_res_max[MODE_COLOR];

  s->s_width_min = 96;
  s->s_length_min = 96;

  s->s_brightness_steps = 0;
  s->s_contrast_steps = 255;
  s->s_threshold_steps = 255;
  s->s_rif = 1;

  DBG (10, "init_model: finish\n");

  return SANE_STATUS_GOOD;
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
  s->u_source = SOURCE_ADF_FRONT;

  /* scan mode */
  s->u_mode = MODE_LINEART;

  /*res, minimum for this mode*/
  s->u_res = s->s_res_min[s->u_mode];

  /* page width US-Letter */
  s->u_page_width = 8.5 * 1200;
  if(s->u_page_width > s->s_width_max){
    s->u_page_width = s->s_width_max;
  }

  /* page height US-Letter */
  s->u_page_height = 11 * 1200;
  if(s->u_page_height > s->s_length_max){
    s->u_page_height = s->s_length_max;
  }

  /* bottom-right x */
  s->u_br_x = s->u_page_width;

  /* bottom-right y */
  s->u_br_y = s->u_page_height;

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
  unsigned char cmd[SEND_len];
  size_t cmdLen = SEND_len;
  unsigned char out[SR_len_time]; /*longest used in this function*/
  int try=0;
  time_t gmt_tt;
  struct tm * gmt_tm_p;
  struct tm * local_tm_p;
 
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
      if (strcmp (dev->sane.name, name) == 0) {
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

  /*send the end batch (GX) command*/
  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd,SEND_code);
  set_SR_datatype_code(cmd,SR_datatype_random);
  set_SR_datatype_qual(cmd,SR_qual_end);
  set_SR_xfer_length(cmd,SR_len_end);

  /*start the following loop*/
  ret = SANE_STATUS_DEVICE_BUSY;
  s->rs_info = 0;

  /*loop until scanner is ready*/
  while(ret == SANE_STATUS_DEVICE_BUSY){
    DBG (15, "sane_open: GX, try %d, sleep %lu\n", try, (unsigned long)s->rs_info);
    try++;
    sleep(s->rs_info);
    ret = do_cmd (
      s, 1, 0,
      cmd, cmdLen,
      NULL, 0,
      NULL, NULL
    );
    if(try > 5){
      break;
    }
  }
  if(ret){
    DBG (5, "sane_open: GX error %d\n",ret);
    return ret;
  }

  /*send the clear buffer (CB) command*/
  DBG (15, "sane_open: CB\n");
  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd,SEND_code);
  set_SR_datatype_code(cmd,SR_datatype_random);
  set_SR_datatype_qual(cmd,SR_qual_clear);
  set_SR_xfer_length(cmd,SR_len_clear);

  ret = do_cmd (
    s, 1, 0,
    cmd, cmdLen,
    NULL, 0,
    NULL, NULL
  );
  if(ret){
    DBG (5, "sane_open: CB error %d\n",ret);
    return ret;
  }

  /*send the GT command*/
  DBG (15, "sane_open: GT\n");
  gmt_tt = time(NULL);
  gmt_tm_p = gmtime(&gmt_tt);

  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd,SEND_code);
  set_SR_datatype_code(cmd,SR_datatype_random);
  set_SR_datatype_qual(cmd,SR_qual_gmt);
  set_SR_xfer_length(cmd,SR_len_time);

  memset(out,0,SR_len_time);
  set_SR_payload_len(out,SR_len_time);
  set_SR_time_hour(out,gmt_tm_p->tm_hour);
  set_SR_time_min(out,gmt_tm_p->tm_min);
  set_SR_time_mon(out,gmt_tm_p->tm_mon);
  set_SR_time_day(out,gmt_tm_p->tm_mday);
  set_SR_time_year(out,gmt_tm_p->tm_year+1900);

  ret = do_cmd (
    s, 1, 0,
    cmd, cmdLen,
    out, SR_len_time,
    NULL, NULL
  );
  if(ret){
    DBG (5, "sane_open: GT error %d\n",ret);
    return ret;
  }

  /*FIXME: read the LC command? */

  /*send the LC command*/
  DBG (15, "sane_open: LC\n");
  gmt_tt = time(NULL);
  local_tm_p = localtime(&gmt_tt);

  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd,SEND_code);
  set_SR_datatype_code(cmd,SR_datatype_random);
  set_SR_datatype_qual(cmd,SR_qual_clock);
  set_SR_xfer_length(cmd,SR_len_time);

  memset(out,0,SR_len_time);
  set_SR_payload_len(out,SR_len_time);
  set_SR_time_hour(out,local_tm_p->tm_hour);
  set_SR_time_min(out,local_tm_p->tm_min);
  set_SR_time_mon(out,local_tm_p->tm_mon);
  set_SR_time_day(out,local_tm_p->tm_mday);
  set_SR_time_year(out,local_tm_p->tm_year+1900);

  ret = do_cmd (
    s, 1, 0,
    cmd, cmdLen,
    out, SR_len_time,
    NULL, NULL
  );
  if(ret){
    DBG (5, "sane_open: LC error %d\n",ret);
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
  if(option==OPT_MODE_GROUP){
    opt->title = "Scan Mode";
    opt->desc = "";
    opt->type = SANE_TYPE_GROUP;
    opt->constraint_type = SANE_CONSTRAINT_NONE;
  }

  /* source */
  if(option==OPT_SOURCE){
    i=0;
    s->o_source_list[i++]=STRING_ADFFRONT;
    s->o_source_list[i++]=STRING_ADFBACK;
    s->o_source_list[i++]=STRING_ADFDUPLEX;
    s->o_source_list[i]=NULL;

    opt->name = SANE_NAME_SCAN_SOURCE;
    opt->title = SANE_TITLE_SCAN_SOURCE;
    opt->desc = SANE_DESC_SCAN_SOURCE;
    opt->type = SANE_TYPE_STRING;
    opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
    opt->constraint.string_list = s->o_source_list;
    opt->size = maxStringSize (opt->constraint.string_list);
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }

  /* scan mode */
  if(option==OPT_MODE){
    i=0;
    if(s->s_mode[MODE_LINEART]){
      s->o_mode_list[i++]=STRING_LINEART;
    }
    if(s->s_mode[MODE_HALFTONE]){
      s->o_mode_list[i++]=STRING_HALFTONE;
    }
    if(s->s_mode[MODE_GRAYSCALE]){
      s->o_mode_list[i++]=STRING_GRAYSCALE;
    }
    if(s->s_mode[MODE_COLOR]){
      s->o_mode_list[i++]=STRING_COLOR;
    }
    s->o_mode_list[i]=NULL;
  
    opt->name = SANE_NAME_SCAN_MODE;
    opt->title = SANE_TITLE_SCAN_MODE;
    opt->desc = SANE_DESC_SCAN_MODE;
    opt->type = SANE_TYPE_STRING;
    opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
    opt->constraint.string_list = s->o_mode_list;
    opt->size = maxStringSize (opt->constraint.string_list);
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }

  /* resolution */
  /* build a list of possible choices for current mode */
  if(option==OPT_RES){
    int reslist[]={100,150,200,240,300,400};
    int j;

    i=0;
    for(j=0;j<6;j++){
      if(reslist[j] >= s->s_res_min[s->u_mode] 
        && reslist[j] <= s->s_res_max[s->u_mode]){
          s->o_res_list[s->u_mode][++i] = reslist[j];
      }
    }
    s->o_res_list[s->u_mode][0] = i;
  
    opt->name = SANE_NAME_SCAN_RESOLUTION;
    opt->title = SANE_TITLE_SCAN_RESOLUTION;
    opt->desc = SANE_DESC_SCAN_RESOLUTION;
    opt->type = SANE_TYPE_INT;
    opt->unit = SANE_UNIT_DPI;
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    opt->constraint_type = SANE_CONSTRAINT_WORD_LIST;
    opt->constraint.word_list = s->o_res_list[s->u_mode];
  }

  /* "Geometry" group ---------------------------------------------------- */
  if(option==OPT_GEOMETRY_GROUP){
    opt->title = "Geometry";
    opt->desc = "";
    opt->type = SANE_TYPE_GROUP;
    opt->constraint_type = SANE_CONSTRAINT_NONE;
  }

  /* top-left x */
  if(option==OPT_TL_X){
    /* values stored in 1200 dpi units */
    /* must be converted to MM for sane */
    s->o_tl_x_range.min = SCANNER_UNIT_TO_FIXED_MM(s->s_width_min);
    s->o_tl_x_range.max = SCANNER_UNIT_TO_FIXED_MM(s->s_width_max);
    s->o_tl_x_range.quant = MM_PER_UNIT_FIX;
  
    opt->name = SANE_NAME_SCAN_TL_X;
    opt->title = SANE_TITLE_SCAN_TL_X;
    opt->desc = SANE_DESC_SCAN_TL_X;
    opt->type = SANE_TYPE_FIXED;
    opt->unit = SANE_UNIT_MM;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &(s->o_tl_x_range);
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }

  /* top-left y */
  if(option==OPT_TL_Y){
    /* values stored in 1200 dpi units */
    /* must be converted to MM for sane */
    s->o_tl_y_range.min = SCANNER_UNIT_TO_FIXED_MM(s->s_length_min);
    s->o_tl_y_range.max = SCANNER_UNIT_TO_FIXED_MM(s->s_length_max);
    s->o_tl_y_range.quant = MM_PER_UNIT_FIX;
  
    opt->name = SANE_NAME_SCAN_TL_Y;
    opt->title = SANE_TITLE_SCAN_TL_Y;
    opt->desc = SANE_DESC_SCAN_TL_Y;
    opt->type = SANE_TYPE_FIXED;
    opt->unit = SANE_UNIT_MM;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &(s->o_tl_y_range);
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }

  /* bottom-right x */
  if(option==OPT_BR_X){
    /* values stored in 1200 dpi units */
    /* must be converted to MM for sane */
    s->o_br_x_range.min = SCANNER_UNIT_TO_FIXED_MM(s->s_width_min);
    s->o_br_x_range.max = SCANNER_UNIT_TO_FIXED_MM(s->s_width_max);
    s->o_br_x_range.quant = MM_PER_UNIT_FIX;
  
    opt->name = SANE_NAME_SCAN_BR_X;
    opt->title = SANE_TITLE_SCAN_BR_X;
    opt->desc = SANE_DESC_SCAN_BR_X;
    opt->type = SANE_TYPE_FIXED;
    opt->unit = SANE_UNIT_MM;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &(s->o_br_x_range);
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }

  /* bottom-right y */
  if(option==OPT_BR_Y){
    /* values stored in 1200 dpi units */
    /* must be converted to MM for sane */
    s->o_br_y_range.min = SCANNER_UNIT_TO_FIXED_MM(s->s_length_min);
    s->o_br_y_range.max = SCANNER_UNIT_TO_FIXED_MM(s->s_length_max);
    s->o_br_y_range.quant = MM_PER_UNIT_FIX;
  
    opt->name = SANE_NAME_SCAN_BR_Y;
    opt->title = SANE_TITLE_SCAN_BR_Y;
    opt->desc = SANE_DESC_SCAN_BR_Y;
    opt->type = SANE_TYPE_FIXED;
    opt->unit = SANE_UNIT_MM;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &(s->o_br_y_range);
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }

  /* page width */
  if(option==OPT_PAGE_WIDTH){
    /* values stored in 1200 dpi units */
    /* must be converted to MM for sane */
    s->o_page_x_range.min = SCANNER_UNIT_TO_FIXED_MM(s->s_width_min);
    s->o_page_x_range.max = SCANNER_UNIT_TO_FIXED_MM(s->s_width_max);
    s->o_page_x_range.quant = MM_PER_UNIT_FIX;

    opt->name = "pagewidth";
    opt->title = "ADF paper width";
    opt->desc = "Must be set properly to align scanning window";
    opt->type = SANE_TYPE_FIXED;
    opt->unit = SANE_UNIT_MM;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->o_page_x_range;
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }

  /* page height */
  if(option==OPT_PAGE_HEIGHT){
    /* values stored in 1200 dpi units */
    /* must be converted to MM for sane */
    s->o_page_y_range.min = SCANNER_UNIT_TO_FIXED_MM(s->s_length_min);
    s->o_page_y_range.max = SCANNER_UNIT_TO_FIXED_MM(s->s_length_max);
    s->o_page_y_range.quant = MM_PER_UNIT_FIX;

    opt->name = "pageheight";
    opt->title = "ADF paper length";
    opt->desc = "Must be set properly to eject pages";
    opt->type = SANE_TYPE_FIXED;
    opt->unit = SANE_UNIT_MM;
    opt->constraint_type = SANE_CONSTRAINT_RANGE;
    opt->constraint.range = &s->o_page_y_range;
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }

  /* "Enhancement" group ------------------------------------------------- */
  if(option==OPT_ENHANCEMENT_GROUP){
    opt->title = "Enhancement";
    opt->desc = "";
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
    opt->constraint.range = &s->o_brightness_range;
    s->o_brightness_range.quant=1;
    s->o_brightness_range.min=-(s->s_brightness_steps/2);
    s->o_brightness_range.max=s->s_brightness_steps/2;
    if(opt->constraint.range->max > opt->constraint.range->min){
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
    opt->constraint.range = &s->o_contrast_range;
    s->o_contrast_range.quant=1;
    s->o_contrast_range.min=-(s->s_contrast_steps/2);
    s->o_contrast_range.max=s->s_contrast_steps/2;
    if(opt->constraint.range->max > opt->constraint.range->min){
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }
    else{
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
    opt->constraint.range = &s->o_threshold_range;
    s->o_threshold_range.min=0;
    s->o_threshold_range.max=s->s_threshold_steps;
    s->o_threshold_range.quant=1;
    if(opt->constraint.range->max > opt->constraint.range->min){
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }
    else{
      opt->cap = SANE_CAP_INACTIVE;
    }
  }

  /*rif*/
  if(option==OPT_RIF){
    opt->name = "rif";
    opt->title = "RIF";
    opt->desc = "Reverse image format";
    opt->type = SANE_TYPE_BOOL;
    opt->unit = SANE_UNIT_NONE;
    if (s->s_rif)
      opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    else
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
          if(s->u_source == SOURCE_ADF_FRONT){
            strcpy (val, STRING_ADFFRONT);
          }
          else if(s->u_source == SOURCE_ADF_BACK){
            strcpy (val, STRING_ADFBACK);
          }
          else if(s->u_source == SOURCE_ADF_DUPLEX){
            strcpy (val, STRING_ADFDUPLEX);
          }
          else{
            DBG(5,"missing option val for source\n"); 
          }
          return SANE_STATUS_GOOD;

        case OPT_MODE:
          if(s->u_mode == MODE_LINEART){
            strcpy (val, STRING_LINEART);
          }
          else if(s->u_mode == MODE_HALFTONE){
            strcpy (val, STRING_HALFTONE);
          }
          else if(s->u_mode == MODE_GRAYSCALE){
            strcpy (val, STRING_GRAYSCALE);
          }
          else if(s->u_mode == MODE_COLOR){
            strcpy (val, STRING_COLOR);
          }
          return SANE_STATUS_GOOD;

        case OPT_RES:
          *val_p = s->u_res;
          return SANE_STATUS_GOOD;

        case OPT_TL_X:
          *val_p = SCANNER_UNIT_TO_FIXED_MM(s->u_tl_x);
          return SANE_STATUS_GOOD;

        case OPT_TL_Y:
          *val_p = SCANNER_UNIT_TO_FIXED_MM(s->u_tl_y);
          return SANE_STATUS_GOOD;

        case OPT_BR_X:
          *val_p = SCANNER_UNIT_TO_FIXED_MM(s->u_br_x);
          return SANE_STATUS_GOOD;

        case OPT_BR_Y:
          *val_p = SCANNER_UNIT_TO_FIXED_MM(s->u_br_y);
          return SANE_STATUS_GOOD;

        case OPT_PAGE_WIDTH:
          *val_p = SCANNER_UNIT_TO_FIXED_MM(s->u_page_width);
          return SANE_STATUS_GOOD;

        case OPT_PAGE_HEIGHT:
          *val_p = SCANNER_UNIT_TO_FIXED_MM(s->u_page_height);
          return SANE_STATUS_GOOD;

        case OPT_BRIGHTNESS:
          *val_p = s->u_brightness;
          return SANE_STATUS_GOOD;

        case OPT_CONTRAST:
          *val_p = s->u_contrast;
          return SANE_STATUS_GOOD;

        case OPT_THRESHOLD:
          *val_p = s->u_threshold;
          return SANE_STATUS_GOOD;

        case OPT_RIF:
          *val_p = s->u_rif;
          return SANE_STATUS_GOOD;
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
          else{
            tmp = SOURCE_ADF_DUPLEX;
          }

          if (s->u_source != tmp) {
            s->u_source = tmp;
            *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          }
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

          if (tmp != s->u_mode){
            s->u_mode = tmp;
            *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          }
          return SANE_STATUS_GOOD;

        case OPT_RES:

          if (s->u_res != val_c) {
            s->u_res = val_c;
            *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          }
          return SANE_STATUS_GOOD;

        /* Geometry Group */
        case OPT_TL_X:
          if (s->u_tl_x != FIXED_MM_TO_SCANNER_UNIT(val_c)){
            s->u_tl_x = FIXED_MM_TO_SCANNER_UNIT(val_c);
            *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          }
          return SANE_STATUS_GOOD;

        case OPT_TL_Y:
          if (s->u_tl_y != FIXED_MM_TO_SCANNER_UNIT(val_c)){
            s->u_tl_y = FIXED_MM_TO_SCANNER_UNIT(val_c);
            *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          }
          return SANE_STATUS_GOOD;

        case OPT_BR_X:
          if (s->u_br_x != FIXED_MM_TO_SCANNER_UNIT(val_c)){
            s->u_br_x = FIXED_MM_TO_SCANNER_UNIT(val_c);
            *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          }
          return SANE_STATUS_GOOD;

        case OPT_BR_Y:
          if (s->u_br_y != FIXED_MM_TO_SCANNER_UNIT(val_c)){
            s->u_br_y = FIXED_MM_TO_SCANNER_UNIT(val_c);
            *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          }
          return SANE_STATUS_GOOD;

        case OPT_PAGE_WIDTH:
          if (s->u_page_width != FIXED_MM_TO_SCANNER_UNIT(val_c)){
            s->u_page_width = FIXED_MM_TO_SCANNER_UNIT(val_c);
            *info |= SANE_INFO_RELOAD_OPTIONS;
          }
          return SANE_STATUS_GOOD;

        case OPT_PAGE_HEIGHT:
          if (s->u_page_height != FIXED_MM_TO_SCANNER_UNIT(val_c)){
            s->u_page_height = FIXED_MM_TO_SCANNER_UNIT(val_c);
            *info |= SANE_INFO_RELOAD_OPTIONS;
          }
          return SANE_STATUS_GOOD;

        /* Enhancement Group */
        case OPT_BRIGHTNESS:
          if (s->u_brightness != val_c){
            s->u_brightness = val_c;
          }
          return SANE_STATUS_GOOD;

        case OPT_CONTRAST:
          if (s->u_contrast != val_c){
            s->u_contrast = val_c;
          }
          return SANE_STATUS_GOOD;

        case OPT_THRESHOLD:
          if (s->u_threshold != val_c){
            s->u_threshold = val_c;
          }
          return SANE_STATUS_GOOD;

        case OPT_RIF:
          if (s->u_rif != val_c){
            s->u_rif = val_c;
          }
          return SANE_STATUS_GOOD;

      }                       /* switch */
  }                           /* else */

  return SANE_STATUS_INVAL;
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
/* SANE_Parameters is defined as a struct containing:
	SANE_Frame format;
	SANE_Bool last_frame;
	SANE_Int lines;
	SANE_Int depth; ( binary=1, gray=8, color=8 (!24) )
	SANE_Int pixels_per_line;
	SANE_Int bytes_per_line;
*/
SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
    SANE_Status ret = SANE_STATUS_GOOD;
    struct scanner *s = (struct scanner *) handle;
  
    DBG (10, "sane_get_parameters: start\n");
  
    /* started? get param data from image header */
    if(s->started){
        DBG (15, "sane_get_parameters: image settings:\n");

        DBG (15, "  tlx=%d, brx=%d, iw=%d, maxx=%d\n",
          s->i_tlx, (s->i_tlx+s->i_width), s->i_width, s->s_width_max/1200);
        DBG (15, "  tly=%d, bry=%d, il=%d, maxy=%d\n",
          s->i_tly, (s->i_tly+s->i_length), s->i_length, s->s_length_max/1200);
        DBG (15, "  res=%d, id=%d, bytes=%d\n",
          s->i_dpi, s->i_id, s->i_bytes);

        params->last_frame = 1;
        params->lines = s->i_length;
        params->pixels_per_line = s->i_width;
    
        /* bitonal */
        if (s->i_bpp == 1) {
            params->format = SANE_FRAME_GRAY;
            params->depth = 1;
            params->bytes_per_line = params->pixels_per_line / 8;

#ifdef SANE_FRAME_G42D
	    /*G4 fax compression*/
            if (s->i_compr) {
                params->format = SANE_FRAME_G42D;
	    }
#endif
        }
        /* gray */
        else if (s->i_bpp == 8) {
            params->format = SANE_FRAME_GRAY;
            params->depth = 8;
            params->bytes_per_line = params->pixels_per_line;

#ifdef SANE_FRAME_JPEG
	    /*jpeg compression*/
            if (s->i_compr) {
                params->format = SANE_FRAME_JPEG;
	    }
#endif
        }
        /* color */
        else if (s->i_bpp == 24 || s->i_bpp == 96) {
            params->format = SANE_FRAME_RGB;
            params->depth = 8;
            params->bytes_per_line = params->pixels_per_line * 3;

#ifdef SANE_FRAME_JPEG
	    /*jpeg compression*/
            if (s->i_compr) {
                params->format = SANE_FRAME_JPEG;
	    }
#endif
        }
        else{
	    DBG(5,"sane_get_parameters: unsupported depth %d\n", s->i_bpp);
	    return SANE_STATUS_INVAL;
        }
    }

    /* not started? get param data from user input */
    else{

        DBG (15, "sane_get_parameters: user settings:\n");

        DBG (15, "  tlx=%d, brx=%d, pw=%d, maxx=%d\n",
          s->u_tl_x, s->u_br_x, s->u_page_width, s->s_width_max);
        DBG (15, "  tly=%d, bry=%d, ph=%d, maxy=%d\n",
          s->u_tl_y, s->u_br_y, s->u_page_height, s->s_length_max);
        DBG (15, "  res=%d, user_x=%d, user_y=%d\n",
          s->u_res, (s->u_res * (s->u_br_x - s->u_tl_x) / 1200),
          (s->u_res * (s->u_br_y - s->u_tl_y) / 1200));
    
        if (s->u_mode == MODE_COLOR) {
            params->format = SANE_FRAME_RGB;
            params->depth = 8;
        }
        else if (s->u_mode == MODE_GRAYSCALE) {
            params->format = SANE_FRAME_GRAY;
            params->depth = 8;
        }
        else {
            params->format = SANE_FRAME_GRAY;
            params->depth = 1;
        }

        params->last_frame = 1;
        params->lines = s->u_res * (s->u_br_y - s->u_tl_y) / 1200;
        params->pixels_per_line = s->u_res * (s->u_br_x - s->u_tl_x) / 1200;
    
        /* bytes per line differs by mode */
        if (s->u_mode == MODE_COLOR) {
            params->bytes_per_line = params->pixels_per_line * 3;
        }
        else if (s->u_mode == MODE_GRAYSCALE) {
            params->bytes_per_line = params->pixels_per_line;
        }
        else {
            params->bytes_per_line = params->pixels_per_line / 8;
        }

    }
  
    DBG (15, "sane_get_parameters: returning:\n");
    DBG (15, "  scan_x=%d, Bpl=%d, depth=%d\n", 
      params->pixels_per_line, params->bytes_per_line, params->depth );
      
    DBG (15, "  scan_y=%d, frame=%d, last=%d\n", 
      params->lines, params->format, params->last_frame );

    DBG (10, "sane_get_parameters: finish\n");
  
    return ret;
}

/*
 * Called by SANE when a page acquisition operation is to be started.
 * commands: scanner control (lampon), send (lut), send (dither),
 * set window, object pos, and scan
 *
 * this will be called before each image, including duplex backsides, 
 * and at the start of adf batch.
 * hence, we spend alot of time playing with s->started, etc.
 */
SANE_Status
sane_start (SANE_Handle handle)
{
  struct scanner *s = handle;
  SANE_Status ret;

  DBG (10, "sane_start: start\n");

  DBG (15, "started=%d, source=%d\n", s->started, s->u_source);

  /* batch already running */
  if(s->started){
      /* not finished with current image, error */
      if (s->bytes_tx != s->i_bytes) {
          DBG(5,"sane_start: previous transfer not finished?");
          return do_cancel(s);
      }
  }

  /* first page of batch */
  else{

      unsigned char cmd[SCAN_len];
      unsigned char pay[SR_len_startstop];

      /* set window command */
      ret = set_window(s);
      if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: ERROR: cannot set window\n");
        do_cancel(s);
        return ret;
      }
    
      /* read/send JQ command */

      /* read/send SC command */
      ret = send_sc(s);
      if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: ERROR: cannot send SC\n");
        do_cancel(s);
        return ret;
      }
    
      /* read/send CT command */

      DBG (15, "sane_start: send SCAN\n");
      memset(cmd, 0, SCAN_len);
      set_SCSI_opcode(cmd, SCAN_code);
    
      ret = do_cmd (
        s, 1, 0,
        cmd, SCAN_len,
        NULL, 0,
        NULL, NULL
      );
      if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: ERROR sending SCAN\n");
        do_cancel(s);
        return ret;
      }

      /* send SS command */
      DBG (15, "sane_start: send SS\n");
      memset(cmd,0,SEND_len);
      set_SCSI_opcode(cmd,SEND_code);
      set_SR_datatype_code(cmd,SR_datatype_random);
      set_SR_datatype_qual(cmd,SR_qual_startstop);
      set_SR_xfer_length(cmd,SR_len_startstop);
    
      memset(pay,0,SR_len_startstop);
      set_SR_payload_len(pay,SR_len_startstop);
      set_SR_startstop_cmd(pay,1);
    
      ret = do_cmd (
        s, 1, 0,
        cmd, SEND_len,
        pay, SR_len_startstop,
        NULL, NULL
      );
      if(ret){
        DBG (5, "sane_open: SS error %d\n",ret);
        return ret;
      }

      DBG (15, "sane_start: sleeping\n");
      sleep(2);

      s->started=1;
  }

  ret = read_imageheader(s);
  if(ret){
    DBG (5, "sane_open: error reading imageheader %d\n",ret);
    return ret;
  }

  /* set clean defaults */
  s->bytes_rx  = 0;
  s->bytes_tx  = 0;

  /* make large buffer to hold the images */
  DBG (15, "sane_start: setup buffer\n");
    
  /* free current buffer if too small */
  if (s->buffer && s->bytes_buf < s->i_bytes) {
      DBG (15, "sane_start: free buffer.\n");
      free(s->buffer);
      s->buffer = NULL;
      s->bytes_buf = 0;
  }
    
  /* grab new buffer if dont have one */
  if (!s->buffer) {
      DBG (15, "sane_start: calloc buffer.\n");
      s->buffer = calloc (1,s->i_bytes);
      if (!s->buffer) {
          DBG (5, "sane_start: Error, no buffer\n");
          do_cancel(s);
          return SANE_STATUS_NO_MEM;
      }
  }

  DBG (15, "started=%d, source=%d\n", s->started, s->u_source);

  DBG (10, "sane_start: finish\n");

  return SANE_STATUS_GOOD;
}

/*
 * This routine issues a SCSI SET WINDOW command to the scanner, using the
 * values currently in the scanner data structure.
 * the scanner has 4 separate windows, and all must be set similarly,
 * even if you dont intend to aquire images from all of them.
 */
static SANE_Status
set_window (struct scanner *s)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  unsigned char cmd[SET_WINDOW_len];
  size_t cmdLen = SET_WINDOW_len;

  /* the data phase has a header, followed by a window desc block 
   * the header specifies the number of bytes in 1 window desc block */
  unsigned char pay[WINDOW_HEADER_len + WINDOW_DESCRIPTOR_len];
  size_t payLen = WINDOW_HEADER_len + WINDOW_DESCRIPTOR_len;

  unsigned char * desc = pay + WINDOW_HEADER_len;

  int width = (s->u_br_x - s->u_tl_x) * s->u_res/1200;
  int length = (s->u_br_y - s->u_tl_y) * s->u_res/1200;

  DBG (10, "set_window: start\n");

  /* binary window settings */
  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd,SET_WINDOW_code);
  set_SW_xferlen(cmd,payLen);

  memset(pay,0,payLen);
  set_WH_desc_len(pay,WINDOW_DESCRIPTOR_len);

  set_WD_wid(desc,WD_wid_front_binary);

  /* common settings */
  set_WD_Xres (desc, s->u_res);
  set_WD_Yres (desc, s->u_res);

  set_WD_ULX (desc, s->u_tl_x);
  set_WD_ULY (desc, s->u_tl_y);

  /* width % 32 == 0 && length % 1 == 0 */
  width -= width % 32;
  width = width*1200/s->u_res;

  length = length*1200/s->u_res;

  set_WD_width (desc, width);
  set_WD_length (desc, length);

  /* brightness not supported? */
  set_WD_brightness (desc, 0);
  set_WD_threshold (desc, s->u_threshold);
  set_WD_contrast (desc, 0);
  if(s->s_contrast_steps){
    /*convert our common -127 to +127 range into HW's range
     *FIXME: this code assumes hardware range of 1-255 */
    set_WD_contrast (desc, s->u_contrast+128);
  }

  if(s->u_mode == MODE_HALFTONE){
    set_WD_composition (desc, WD_compo_HALFTONE);
    set_WD_bitsperpixel (desc, 1);
  }
  else{
    set_WD_composition (desc, WD_compo_LINEART);
    set_WD_bitsperpixel (desc, 1);
  }

  /* FIXME ht pattern */

  set_WD_rif (desc, s->u_rif);

  set_WD_bitorder (desc, 1);

  /* compression options */
  if(s->u_compr)
    set_WD_compress_type (desc, WD_compr_FAXG4);

  /*FIXME: noise filter */

  set_WD_allow_zero(desc,1);
  set_WD_cropping (desc, WD_crop_RELATIVE);

  /*FIXME: more settings here*/

  hexdump(15, "front binary window:", desc, WINDOW_DESCRIPTOR_len);

  DBG (15, "set_window: set window binary back\n");
  ret = do_cmd (
    s, 1, 0,
    cmd, cmdLen,
    pay, payLen,
    NULL, NULL
  );
  if(ret){
    DBG (5, "set_window: error setting binary front window %d\n",ret);
    return ret;
  }

  /*send the window for backside too*/
  set_WD_wid(desc,WD_wid_back_binary);

  DBG (15, "set_window: set window binary back\n");
  ret = do_cmd (
      s, 1, 0,
      cmd, cmdLen,
      pay, payLen,
      NULL, NULL
  );
  if(ret){
    DBG (5, "set_window: error setting binary back window %d\n",ret);
    return ret;
  }

#if 0
  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd,GET_WINDOW_code);
  set_GW_single(cmd,1);
  set_GW_wid(cmd,WD_wid_front_color);
  set_GW_xferlen(cmd,payLen);

  ret = do_cmd (
      s, 1, 0,
      cmd, cmdLen,
      NULL, 0,
      pay, &payLen
  );
  if(ret){
    DBG (5, "set_window: error getting window %d\n",ret);
    return ret;
  }
  hexdump(15,"foo",pay,payLen);
#endif

  /* color window settings */
  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd,SET_WINDOW_code);
  set_SW_xferlen(cmd,payLen);

  memset(pay,0,payLen);
  set_WH_desc_len(pay,WINDOW_DESCRIPTOR_len);

  set_WD_wid(desc,WD_wid_front_color);

  /* common settings */
  set_WD_Xres (desc, s->u_res);
  set_WD_Yres (desc, s->u_res);

  set_WD_ULX (desc, s->u_tl_x);
  set_WD_ULY (desc, s->u_tl_y);

  set_WD_width (desc, width);
  set_WD_length (desc, length);

  /*gray mode*/
  if(s->u_mode == MODE_GRAYSCALE){
    /*
     gamma
     width % 8 == 0 && length % 8 == 0
     */
    set_WD_composition (desc, WD_compo_MULTILEVEL);
    set_WD_bitsperpixel (desc, 8);
  }
  /*color mode or color window in binary mode*/
  else{
    /*
     width % 16 == 0 && length % 8 == 0
     */
    set_WD_composition (desc, WD_compo_MULTILEVEL);
    set_WD_bitsperpixel (desc, 24);

    /* compression options */
    if(s->u_compr)
      set_WD_compress_type (desc, WD_compr_JPEG);
  }

  set_WD_bitorder (desc, 1);

  /*FIXME: noise filter */

  set_WD_allow_zero(desc,1);
  set_WD_cropping (desc, WD_crop_RELATIVE);

  /*FIXME: more settings here*/

  DBG (15, "set_window: set window color front\n");

  ret = do_cmd (
    s, 1, 0,
    cmd, cmdLen,
    pay, payLen,
    NULL, NULL
  );
  if(ret){
    DBG (5, "set_window: error setting color front window %d\n",ret);
    return ret;
  }

  /*send the window for backside too*/
  set_WD_wid(desc,WD_wid_back_color);

  DBG (15, "set_window: set window color back\n");

  ret = do_cmd (
      s, 1, 0,
      cmd, cmdLen,
      pay, payLen,
      NULL, NULL
  );
  if(ret){
    DBG (5, "set_window: error setting color back window %d\n",ret);
    return ret;
  }

  DBG (10, "set_window: finish\n");

  return ret;
}

/*
 * This routine reads the SC (scanner config) data from the scanner
 * modifies a few params based on user data, and sends it back
 */
static SANE_Status
send_sc(struct scanner *s)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  unsigned char cmd[READ_len];
  size_t cmdLen = READ_len;
  unsigned char pay[SR_len_config];
  size_t payLen = SR_len_config;

  /* send SC command */
  DBG (10, "send_sc: start\n");

  DBG (15, "send_sc: reading config\n");
  memset(cmd,0,READ_len);
  set_SCSI_opcode(cmd,READ_code);

  set_SR_datatype_code(cmd,SR_datatype_random);
  set_SR_datatype_qual(cmd,SR_qual_config);
  set_SR_xfer_length(cmd,SR_len_config);
  
  ret = do_cmd (
    s, 1, 0,
    cmd, cmdLen,
    NULL, 0,
    pay, &payLen
  );
  if(ret || !payLen){
    DBG (5, "send_sc: error reading: %d\n",ret);
    return ret;
  }

  memset(cmd,0,SEND_len);
  set_SCSI_opcode(cmd,SEND_code);

  set_SR_datatype_code(cmd,SR_datatype_random);
  set_SR_datatype_qual(cmd,SR_qual_config);
  set_SR_xfer_length(cmd,payLen);
  
  if(s->u_source == SOURCE_ADF_FRONT){
    if(s->u_mode == MODE_COLOR || s->u_mode == MODE_GRAYSCALE){
      set_SR_sc_io1(pay,SR_sc_io_front_color);
    }
    else{
      set_SR_sc_io1(pay,SR_sc_io_front_binary);
    }
    set_SR_sc_io2(pay,SR_sc_io_none);
    set_SR_sc_io3(pay,SR_sc_io_none);
    set_SR_sc_io4(pay,SR_sc_io_none);
  }
  else if(s->u_source == SOURCE_ADF_BACK){
    if(s->u_mode == MODE_COLOR || s->u_mode == MODE_GRAYSCALE){
      set_SR_sc_io1(pay,SR_sc_io_rear_color);
    }
    else{
      set_SR_sc_io1(pay,SR_sc_io_rear_binary);
    }
    set_SR_sc_io2(pay,SR_sc_io_none);
    set_SR_sc_io3(pay,SR_sc_io_none);
    set_SR_sc_io4(pay,SR_sc_io_none);
  }
  else{
    if(s->u_mode == MODE_COLOR || s->u_mode == MODE_GRAYSCALE){
      set_SR_sc_io1(pay,SR_sc_io_front_color);
      set_SR_sc_io2(pay,SR_sc_io_rear_color);
    }
    else{
      set_SR_sc_io1(pay,SR_sc_io_front_binary);
      set_SR_sc_io2(pay,SR_sc_io_rear_binary);
    }
    set_SR_sc_io3(pay,SR_sc_io_none);
    set_SR_sc_io4(pay,SR_sc_io_none);
  }

  /*FIXME: there are hundreds of other settings in this payload*/

  ret = do_cmd (
    s, 1, 0,
    cmd, cmdLen,
    pay, payLen,
    NULL, NULL
  );

  DBG (10, "send_sc: finish %d\n",ret);

  return ret;
}

/*
 * This routine reads the image header from the scanner, and updates
 * values currently in the scanner data structure.
 */
static SANE_Status
read_imageheader (struct scanner *s)
{
  SANE_Status ret = SANE_STATUS_GOOD;

  unsigned char cmd[READ_len];
  unsigned char pay[SR_len_imageheader];
  size_t payLen = SR_len_imageheader;
  int pass = 0;

  /* read img header */
  DBG (10, "read_imageheader: start\n");

  memset(cmd,0,READ_len);
  set_SCSI_opcode(cmd,READ_code);
  set_SR_datatype_code(cmd,SR_datatype_imageheader);
  set_SR_xfer_length(cmd,SR_len_imageheader);

  while (pass++ < 1000){
  
    DBG (15, "read_imageheader: pass %d\n", pass);

    payLen = SR_len_imageheader;

    ret = do_cmd (
      s, 1, 0,
      cmd, READ_len,
      NULL, 0,
      pay, &payLen
    );

    DBG (15, "read_imageheader: pass status %d\n", ret);

    if(ret != SANE_STATUS_DEVICE_BUSY){
      break;
    }

    usleep(50000);
  }

  if (ret == SANE_STATUS_GOOD){

    DBG (15, "image header:\n");
  
    DBG (15, "  bytes: %d\n",get_SR_ih_image_length(pay));
    s->i_bytes = get_SR_ih_image_length(pay);
  
    DBG (15, "  id: %d\n",get_SR_ih_image_id(pay));
    s->i_id = get_SR_ih_image_id(pay);
  
    DBG (15, "  dpi: %d\n",get_SR_ih_resolution(pay));
    s->i_dpi = get_SR_ih_resolution(pay);
  
    DBG (15, "  tlx: %d\n",get_SR_ih_ulx(pay));
    s->i_tlx = get_SR_ih_ulx(pay);
  
    DBG (15, "  tly: %d\n",get_SR_ih_uly(pay));
    s->i_tly = get_SR_ih_uly(pay);
  
    DBG (15, "  width: %d\n",get_SR_ih_width(pay));
    s->i_width = get_SR_ih_width(pay);
  
    DBG (15, "  length: %d\n",get_SR_ih_length(pay));
    s->i_length = get_SR_ih_length(pay);
  
    DBG (15, "  bpp: %d\n",get_SR_ih_bpp(pay));
    s->i_bpp = get_SR_ih_bpp(pay);
  
    DBG (15, "  comp: %d\n",get_SR_ih_comp_type(pay));
    s->i_compr = get_SR_ih_comp_type(pay);
  
    /*FIXME: there are alot more of these?*/
  }

  DBG (10, "read_imageheader: finish %d\n", ret);

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
  SANE_Status ret=0;

  DBG (10, "sane_read: start\n");

  *len=0;

  /* maybe cancelled? */
  if(!s->started){
      DBG (5, "sane_read: not started, call sane_start\n");
      return SANE_STATUS_CANCELLED;
  }

  /* sane_start required between images */
  if(s->bytes_tx == s->i_bytes){
      DBG (15, "sane_read: returning eof\n");
      return SANE_STATUS_EOF;
  }

  if(s->i_bytes > s->bytes_rx ){
      ret = read_from_scanner(s);
      if(ret){
          DBG(5,"sane_read: returning %d\n",ret);
          return ret;
      }
  }

  /* copy a block from buffer to frontend */
  ret = read_from_buffer(s,buf,max_len,len);

  DBG (10, "sane_read: finish\n");

  return ret;
}

static SANE_Status
read_from_scanner(struct scanner *s)
{
    SANE_Status ret=SANE_STATUS_GOOD;
    int bytes = s->buffer_size;
    int remain = s->i_bytes - s->bytes_rx;
    unsigned char * buf;
    size_t inLen = 0;
  
    unsigned char cmd[READ_len];
    int cmdLen=READ_len;

    DBG (10, "read_from_scanner: start\n");
  
    memset(cmd, 0, cmdLen);
    set_SCSI_opcode(cmd, READ_code);

    /* figure out the max amount to transfer */
    if(bytes > remain){
        bytes = remain;
    }
  
    DBG(15, "read_from_scanner: to:%d rx:%d re:%d bu:%d pa:%d\n",
      s->i_bytes, s->bytes_rx, remain, s->buffer_size, bytes);
  
    if(ret){
        return ret;
    }
  
    inLen = bytes;
  
    buf = malloc(bytes);
    if(!buf){
        DBG(5, "read_from_scanner: not enough mem for buffer: %d\n",bytes);
        return SANE_STATUS_NO_MEM;
    }
  
    set_SR_datatype_code (cmd, SR_datatype_imagedata);
    set_SR_xfer_length (cmd, bytes);
  
    ret = do_cmd (
      s, 1, 0,
      cmd, cmdLen,
      NULL, 0,
      buf, &inLen
    );
  
    if (ret == SANE_STATUS_GOOD) {
        DBG(15, "read_from_scanner: got GOOD, returning GOOD\n");
    }
    else if (ret == SANE_STATUS_EOF) {
        DBG(15, "read_from_scanner: got EOF, finishing\n");
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
  
    if(inLen){
        copy_buffer (s, buf, inLen);
    }
  
    free(buf);
  
    if(ret == SANE_STATUS_EOF){
      DBG (5, "read_from_scanner: unexpected EOF, shortening image\n");
      s->i_bytes = s->bytes_rx;
      ret = SANE_STATUS_GOOD;
    }

    DBG (10, "read_from_scanner: finish\n");
  
    return ret;
}

static SANE_Status
copy_buffer(struct scanner *s, unsigned char * buf, int len)
{
  SANE_Status ret=SANE_STATUS_GOOD;

  DBG (10, "copy_buffer: start\n");

  memcpy(s->buffer+s->bytes_rx,buf,len);
  s->bytes_rx += len;

  DBG (10, "copy_buffer: finish\n");

  return ret;
}

static SANE_Status
read_from_buffer(struct scanner *s, SANE_Byte * buf,
  SANE_Int max_len, SANE_Int * len)
{
    SANE_Status ret=SANE_STATUS_GOOD;
    int bytes = max_len;
    int remain = s->bytes_rx - s->bytes_tx;
  
    DBG (10, "read_from_buffer: start\n");
  
    /* figure out the max amount to transfer */
    if(bytes > remain){
        bytes = remain;
    }
  
    *len = bytes;
  
    DBG(15, "read_from_buffer: to:%d tx:%d re:%d bu:%d pa:%d\n",
      s->i_bytes, s->bytes_tx, remain, max_len, bytes);
  
    /*FIXME this needs to timeout eventually */
    if(!bytes){
        DBG(5,"read_from_buffer: nothing to do\n");
        return SANE_STATUS_GOOD;
    }
  
    memcpy(buf,s->buffer+s->bytes_tx,bytes);
  
    s->bytes_tx += *len;
      
    DBG (10, "read_from_buffer: finish\n");
  
    return ret;
}


/*
 * @@ Section 4 - SANE cleanup functions
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
 * operations only (such as image is acquisition). It is safe to call
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
  DBG (10, "sane_cancel: start\n");
  do_cancel ((struct scanner *) handle);
  DBG (10, "sane_cancel: finish\n");
}

/*
 * Performs cleanup.
 * FIXME: do better cleanup if scanning is ongoing...
 */
static SANE_Status
do_cancel (struct scanner *s)
{
  DBG (10, "do_cancel: start\n");

  s->started = 0;

  DBG (10, "do_cancel: finish\n");

  return SANE_STATUS_CANCELLED;
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
  DBG (10, "sane_close: start\n");

  do_cancel((struct scanner *) handle);
  disconnect_fd((struct scanner *) handle);

  DBG (10, "sane_close: finish\n");
}

static SANE_Status
disconnect_fd (struct scanner *s)
{
  DBG (10, "disconnect_fd: start\n");

  if(s->fd > -1){
    DBG (15, "disconnecting scsi device\n");
    sanei_scsi_close (s->fd);
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
      free (dev->device_name);
      free (dev);
  }

  if (sane_devArray)
    free (sane_devArray);

  scanner_devList = NULL;
  sane_devArray = NULL;

  DBG (10, "sane_exit: finish\n");
}


/*
 * @@ Section 5 - misc helper functions
 */
/*
 * Called by the SANE SCSI core on device errors
 * parses the request sense return data buffer,
 * decides the best SANE_Status for the problem
 * and produces debug msgs
 */
static SANE_Status
sense_handler (int fd, unsigned char * sensed_data, void *arg)
{
  struct scanner *s = arg;
  unsigned int ili = get_RS_ILI (sensed_data);
  unsigned int sk = get_RS_sense_key (sensed_data);
  unsigned int asc = get_RS_ASC (sensed_data);
  unsigned int ascq = get_RS_ASCQ (sensed_data);

  DBG (5, "sense_handler: start\n");

  /* kill compiler warning */
  fd = fd;

  /* save for later */
  s->rs_info = get_RS_information (sensed_data);

  DBG (5, "SK=%#02x, ASC=%#02x, ASCQ=%#02x, ILI=%d, info=%#08lx\n",
       sk, asc, ascq, ili, (unsigned long)s->rs_info);

  switch (sk) {

    /* no sense */
    case 0x0:
      if (0x00 != asc) {
        DBG (5, "No sense: unknown asc\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (0x00 != ascq) {
        DBG (5, "No sense: unknown ascq\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (ili) {
        DBG (5, "No sense: ILI set\n");
        return SANE_STATUS_EOF;
      }
      DBG  (5, "No sense: ready\n");
      return SANE_STATUS_GOOD;

    /* not ready */
    case 0x2:
      if (0x80 != asc) {
        DBG (5, "Not ready: unknown asc\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (0x00 != ascq) {
        DBG (5, "Not ready: unknown ascq\n");
        return SANE_STATUS_IO_ERROR;
      }
      DBG (5, "Not ready: end of job\n");
      return SANE_STATUS_NO_DOCS;
      break;

    /* hardware error */
    case 0x4:
      if (0x3b != asc) {
        DBG (5, "Hardware error: unknown asc\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (0x05 == ascq) {
        DBG (5, "Hardware error: paper jam\n");
        return SANE_STATUS_JAMMED;
      }
      if (0x80 == ascq) {
        DBG (5, "Hardware error: multi-feed\n");
        return SANE_STATUS_JAMMED;
      }
      DBG (5, "Hardware error: unknown ascq\n");
      return SANE_STATUS_IO_ERROR;
      break;

    /* illegal request */
    case 0x5:
      if (asc != 0x20 && asc != 0x24 && asc != 0x25 && asc != 0x26
      && asc != 0x83 && asc != 0x8f) {
        DBG (5, "Illegal request: unknown asc\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (0x20 == asc && 0x00 == ascq) {
        DBG  (5, "Illegal request: invalid opcode\n");
        return SANE_STATUS_INVAL;
      }
      if (0x24 == asc && 0x00 == ascq) {
        DBG  (5, "Illegal request: invalid field in CDB\n");
        return SANE_STATUS_INVAL;
      }
      if (0x25 == asc && 0x00 == ascq) {
        DBG  (5, "Illegal request: invalid LUN\n");
        return SANE_STATUS_INVAL;
      }
      if (0x26 == asc && 0x00 == ascq) {
        DBG  (5, "Illegal request: invalid field in params\n");
        return SANE_STATUS_INVAL;
      }
      if (0x83 == asc && 0x00 == ascq) {
        DBG  (5, "Illegal request: command failed, check log\n");
        return SANE_STATUS_INVAL;
      }
      if (0x83 == asc && 0x01 == ascq) {
        DBG  (5, "Illegal request: command failed, invalid state\n");
        return SANE_STATUS_INVAL;
      }
      if (0x83 == asc && 0x02 == ascq) {
        DBG  (5, "Illegal request: command failed, critical error\n");
        return SANE_STATUS_INVAL;
      }
      if (0x8f == asc && 0x00 == ascq) {
        DBG  (5, "Illegal request: no image\n");
        return SANE_STATUS_DEVICE_BUSY;
      }
      DBG  (5, "Illegal request: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;
      break;

    /* unit attention */
    case 0x6:
      if (asc != 0x29 && asc != 0x80) {
        DBG (5, "Unit attention: unknown asc\n");
        return SANE_STATUS_IO_ERROR;
      }
      if (0x29 == asc && 0x60 == ascq) {
        DBG  (5, "Unit attention: device reset\n");
        return SANE_STATUS_GOOD;
      }
      if (0x80 == asc && 0x00 == ascq) {
        DBG  (5, "Unit attention: Energy Star warm up\n");
        return SANE_STATUS_DEVICE_BUSY;
      }
      if (0x80 == asc && 0x01 == ascq) {
        DBG  (5, "Unit attention: lamp warm up for scan\n");
        return SANE_STATUS_DEVICE_BUSY;
      }
      if (0x80 == asc && 0x02 == ascq) {
        DBG  (5, "Unit attention: lamp warm up for cal\n");
        return SANE_STATUS_DEVICE_BUSY;
      }
      if (0x80 == asc && 0x04 == ascq) {
        DBG  (5, "Unit attention: calibration failed\n");
        return SANE_STATUS_INVAL;
      }
      DBG  (5, "Unit attention: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;
      break;

    /* ia overflow */
    case 0x9:
      if (0x80 == asc && 0x00 == ascq) {
        DBG  (5, "IA overflow: IA field overflow\n");
        return SANE_STATUS_IO_ERROR;
      }
      DBG  (5, "IA overflow: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;
      break;

    /* volume overflow */
    case 0xd:
      if (0x80 == asc && 0x00 == ascq) {
        DBG  (5, "Volume overflow: Image buffer full\n");
        return SANE_STATUS_IO_ERROR;
      }
      DBG  (5, "Volume overflow: unknown asc/ascq\n");
      return SANE_STATUS_IO_ERROR;
      break;

    default:
      DBG (5, "Unknown Sense Code\n");
      return SANE_STATUS_IO_ERROR;
  }

  DBG (5, "sense_handler: should never happen!\n");

  return SANE_STATUS_IO_ERROR;
}

/*
SANE_Status
do_rs(scanner * s)
{
  SANE_Status ret;
  unsigned char cmd[REQUEST_SENSE_len];
  size_t cmdLen = REQUEST_SENSE_len;
 
  DBG (10, "do_rs: start\n");
  
  memset(cmd,0,cmdLen);
  set_SCSI_opcode(cmd,REQUEST_SENSE_code);
  set_SR_datatype_code(cmd,SR_datatype_random);
  set_SR_datatype_qual(cmd,SR_qual_end);

  ret = do_cmd (
    s, 1, 0,
    cmd, cmdLen,
    NULL, 0,
    NULL, NULL
  );

  while(ret == SANE_STATUS_DEVICE_BUSY){
    ret = run_rs(s);
  }

  DBG (10, "do_rs: finish\n");

  return SANE_STATUS_GOOD;
}
*/

SANE_Status
do_cmd(struct scanner *s, int runRS, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t * inLen
)
{
  SANE_Status ret = SANE_STATUS_GOOD;
  size_t actLen = 0;

  /*shut up compiler*/
  runRS=runRS;
  shortTime=shortTime;

  DBG(10, "do_cmd: start\n");

  DBG(25, "cmd: writing %d bytes\n", (int)cmdLen);
  hexdump(30, "cmd: >>", cmdBuff, cmdLen);

  if(outBuff && outLen){
    DBG(25, "out: writing %d bytes\n", (int)outLen);
    hexdump(30, "out: >>", outBuff, outLen);
  }
  if (inBuff && inLen){
    DBG(25, "in: reading %d bytes\n", (int)*inLen);
    actLen = *inLen;
  }

  ret = sanei_scsi_cmd2(s->fd, cmdBuff, cmdLen, outBuff, outLen, inBuff, inLen);

  if(ret != SANE_STATUS_GOOD && ret != SANE_STATUS_EOF){
    DBG(5,"do_cmd: return '%s'\n",sane_strstatus(ret));
    return ret;
  }

  /* FIXME: should we look at s->rs_info here? */
  if (inBuff && inLen){
    hexdump(30, "in: <<", inBuff, *inLen);
    DBG(25, "in: read %d bytes\n", (int)*inLen);
  }

  DBG(10, "do_cmd: finish\n");

  return ret;
}

#if 0 /* unused */
static SANE_Status
wait_scanner(struct scanner *s) 
{
  int ret;

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
#endif /* 0 - unused */

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

/**
 * Prints a hex dump of the given buffer onto the debug output stream.
 */
static void
hexdump (int level, char *comment, unsigned char *p, int l)
{
  int i;
  char line[128];
  char *ptr;

  if(DBG_LEVEL < level)
    return;

  DBG (level, "%s\n", comment);
  ptr = line;
  for (i = 0; i < l; i++, p++)
    {
      if ((i % 16) == 0)
        {
          if (ptr != line)
            {
              *ptr = '\0';
              DBG (level, "%s\n", line);
              ptr = line;
            }
          sprintf (ptr, "%3.3x:", i);
          ptr += 4;
        }
      sprintf (ptr, " %2.2x", *p);
      ptr += 3;
    }
  *ptr = '\0';
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
