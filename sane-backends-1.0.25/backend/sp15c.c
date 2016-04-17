static const char RCSid[] = "$Header$";
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

   This file implements a SANE backend for Fujitsu ScanParner 15C
   flatbed/ADF scanners.  It was derived from the COOLSCAN driver.
   Written by Randolph Bentson <bentson@holmsjoen.com> */

/*
 * $Log$
 * Revision 1.17  2008/11/26 21:21:30  kitno-guest
 * * backend/ *.[ch]: nearly every backend used V_MAJOR
 * instead of SANE_CURRENT_MAJOR in sane_init()
 * * backend/snapscan.c: remove EXPECTED_VERSION check
 * since new SANE standard is forward compatible
 *
 * Revision 1.16  2008-07-26 03:53:44  kitno-guest
 * separate x-resolution from resolution, and update all backends that use
 * it, to prevent ui change
 *
 * Revision 1.15  2007-11-18 10:59:18  ellert-guest
 * Fix handling of valid "negative" PIDs
 *
 * Revision 1.14  2007-10-26 14:56:38  jblache
 * OPT_NUM_OPTS must be of type SANE_TYPE_INT.
 *
 * Revision 1.13  2006/03/29 20:48:50  hmg-guest
 * Fixed ADF support. Patch from Andreas Degert <ad@papyrus-gmbh.de>.
 *
 * Revision 1.12  2005/10/01 17:06:25  hmg-guest
 * Fixed some warnings (bug #302290).
 *
 * Revision 1.11  2005/07/15 18:12:49  hmg-guest
 * Better 4->8 bit depth expansion algorithm (from Mattias Ellert
 * <mattias.ellert@tsl.uu.se>).
 *
 * Revision 1.10  2004/10/06 15:59:40  hmg-guest
 * Don't eject medium twice after each page.
 *
 * Revision 1.9  2004/06/20 00:34:10  ellert-guest
 * Missed one...
 *
 * Revision 1.8  2004/05/29 10:27:47  hmg-guest
 * Fixed the fix of the sanei_thread fix (from Mattias Ellert).
 *
 * Revision 1.7  2004/05/28 18:52:53  hmg-guest
 * Fixed sanei_thread fix (bug #300634, by Mattias Ellert).
 *
 * Revision 1.6  2004/05/23 17:28:56  hmg-guest
 * Use sanei_thread instead of fork() in the unmaintained backends.
 * Patches from Mattias Ellert (bugs: 300635, 300634, 300633, 300629).
 *
 * Revision 1.5  2003/12/27 17:48:38  hmg-guest
 * Silenced some compilation warnings.
 *
 * Revision 1.4  2001/05/31 18:01:39  hmg
 * Fixed config_line[len-1] bug which could generate an access
 * violation if len==0.
 * Henning Meier-Geinitz <henning@meier-geinitz.de>
 *
 * Revision 1.3  2000/08/12 15:09:38  pere
 * Merge devel (v1.0.3) into head branch.
 *
 * Revision 1.1.2.6  2000/07/30 11:16:06  hmg
 * 2000-07-30  Henning Meier-Geinitz <hmg@gmx.de>
 *
 * 	* backend/mustek.*: Update to Mustek backend 1.0-95. Changed from
 * 	  wait() to waitpid() and removed unused code.
 * 	* configure configure.in backend/m3096g.c backend/sp15c.c: Reverted
 * 	  the V_REV patch. V_REV should not be used in backends.
 *
 * Revision 1.1.2.5  2000/07/29 21:38:13  hmg
 * 2000-07-29  Henning Meier-Geinitz <hmg@gmx.de>
 *
 * 	* backend/sp15.c backend/m3096g.c: Replace fgets with
 * 	  sanei_config_read, return V_REV as part of version_code string
 * 	  (patch from Randolph Bentson).
 *
 * Revision 1.1.2.4  2000/07/25 21:47:46  hmg
 * 2000-07-25  Henning Meier-Geinitz <hmg@gmx.de>
 *
 * 	* backend/snapscan.c: Use DBG(0, ...) instead of fprintf (stderr, ...).
 * 	* backend/abaton.c backend/agfafocus.c backend/apple.c backend/dc210.c
 *  	  backend/dll.c backend/dmc.c backend/microtek2.c backend/pint.c
 * 	  backend/qcam.c backend/ricoh.c backend/s9036.c backend/snapscan.c
 * 	  backend/tamarack.c: Use sanei_config_read instead of fgets.
 * 	* backend/dc210.c backend/microtek.c backend/pnm.c: Added
 * 	  #include "../include/sane/config.h".
 * 	* backend/dc25.c backend/m3096.c  backend/sp15.c
 *  	  backend/st400.c: Moved #include "../include/sane/config.h" to the beginning.
 * 	* AUTHORS: Changed agfa to agfafocus.
 *
 * Revision 1.1.2.3  2000/03/14 17:47:12  abel
 * new version of the Sharp backend added.
 *
 * Revision 1.1.2.2  2000/01/26 03:51:48  pere
 * Updated backends sp15c (v1.12) and m3096g (v1.11).
 *
 * Revision 1.12  2000/01/25 16:23:13  bentson
 * tab expansion; add one debug message
 *
 * Revision 1.11  2000/01/05 05:21:37  bentson
 * indent to barfable GNU style
 *
 * Revision 1.10  1999/12/06 17:36:55  bentson
 * show default value for scan size at the start
 *
 * Revision 1.9  1999/12/04 00:30:35  bentson
 * fold in 1.8.1.x versions
 *
 * Revision 1.8.1.2  1999/12/04 00:19:43  bentson
 * bunch of changes to complete MEDIA CHECK use
 *
 * Revision 1.8.1.1  1999/12/03 20:44:56  bentson
 * trial changes to use MEDIA CHECK command
 *
 * Revision 1.8  1999/12/03 18:30:56  bentson
 * cosmetic changes
 *
 * Revision 1.7  1999/11/24 20:09:25  bentson
 * fold in 1.6.1.x changes
 *
 * Revision 1.6.1.3  1999/11/24 15:56:48  bentson
 * remove some debugging; final :-) fix to option constraint processing
 *
 * Revision 1.6.1.2  1999/11/24 15:37:42  bentson
 * more constraint debugging
 *
 * Revision 1.6.1.1  1999/11/24 14:35:24  bentson
 * fix some of the constraint handling
 *
 * Revision 1.6  1999/11/23 18:48:27  bentson
 * add constraint checking and enforcement
 *
 * Revision 1.5  1999/11/23 08:26:03  bentson
 * basic color seems to work
 *
 * Revision 1.4  1999/11/23 06:41:26  bentson
 * 4-bit Grayscale works; now working on color
 *
 * Revision 1.3  1999/11/22 18:15:07  bentson
 * more work on color support
 *
 * Revision 1.2  1999/11/19 17:30:54  bentson
 * enhance control (works with xscanimage)
 *
 * Revision 1.1  1999/11/19 15:09:08  bentson
 * cribbed from m3096g
 *
 */

/* SANE-FLOW-DIAGRAM

   - sane_init() : initialize backend, attach scanners
   . - sane_get_devices() : query list of scanner-devices
   . - sane_open() : open a particular scanner-device
   . . - sane_set_io_mode : set blocking-mode
   . . - sane_get_select_fd : get scanner-fd
   . . - sane_get_option_descriptor() : get option informations
   . . - sane_control_option() : change option values
   . .
   . . - sane_start() : start image aquisition
   . .   - sane_get_parameters() : returns actual scan-parameters
   . .   - sane_read() : read image-data (from pipe)
   . .
   . . - sane_cancel() : cancel operation
   . - sane_close() : close opened scanner-device
   - sane_exit() : terminate use of backend
 */

/* ------------------------------------------------------------------------- */

#include "../include/sane/config.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>

#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_scsi.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_thread.h"

#include "sp15c-scsi.h"
#include "sp15c.h"

/* ------------------------------------------------------------------------- */

static const char negativeStr[] = "Negative";
static const char positiveStr[] = "Positive";
static SANE_String_Const type_list[] =
{positiveStr, negativeStr, 0};

static SANE_String_Const source_list[] =
{"ADF", "FB", NULL};

static const SANE_Int resolution_list[] =
{11, 0, 60, 75, 80, 100, 120, 150, 200, 240, 300, 600};

static const SANE_Int x_res_list[] =
{11, 0, 60, 75, 80, 100, 120, 150, 200, 240, 300, 600};

static const SANE_Int y_res_list[] =
{11, 0, 60, 75, 80, 100, 120, 150, 200, 240, 300, 600};

static const char lineStr[] = SANE_VALUE_SCAN_MODE_LINEART;
static const char halfStr[] = SANE_VALUE_SCAN_MODE_HALFTONE;
static const char gray4Str[] = "4-bit Gray";
static const char gray8Str[] = "8-bit Gray";
static const char colorStr[] = SANE_VALUE_SCAN_MODE_COLOR;
static SANE_String_Const scan_mode_list[] =
{lineStr, halfStr, gray4Str, gray8Str, colorStr, NULL};

/* how do the following work? */
static const SANE_Range brightness_range =
{0, 255, 32};
static const SANE_Range threshold_range =
{0, 255, 4};
static const SANE_Range x_range =
{0, SANE_FIX (216), 1};
static const SANE_Range y_range_fb =
{0, SANE_FIX (295), 1};
static const SANE_Range y_range_adf =
{0, SANE_FIX (356), 1};

/* ################# externally visible routines ################{ */


SANE_Status                     /* looks like frontend ignores results */
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  char dev_name[PATH_MAX];
  size_t len;
  FILE *fp;
  authorize = authorize; /* silence compilation warnings */

  DBG_INIT ();
  DBG (10, "sane_init\n");

  sanei_thread_init ();

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, 0);
  fp = sanei_config_open (SP15C_CONFIG_FILE);
  if (!fp)
    {
      attach_scanner ("/dev/scanner", 0);       /* no config-file: /dev/scanner */
      return SANE_STATUS_GOOD;
    }

  while (sanei_config_read (dev_name, sizeof (dev_name), fp))
    {
      if (dev_name[0] == '#')
        continue;
      len = strlen (dev_name);
      if (!len)
        continue;
      sanei_config_attach_matching_devices (dev_name, attach_one);
    }

  fclose (fp);
  return SANE_STATUS_GOOD;
}                               /* sane_init */


SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  struct sp15c *dev;
  int i;
  
  local_only = local_only; /* silence compilation warnings */

  DBG (10, "sane_get_devices\n");

  if (devlist)
    free (devlist);
  devlist = calloc (num_devices + 1, sizeof (devlist[0]));
  if (!devlist)
    return SANE_STATUS_NO_MEM;

  for (dev = first_dev, i = 0; i < num_devices; dev = dev->next)
    devlist[i++] = &dev->sane;
  devlist[i++] = 0;

  *device_list = devlist;

  return SANE_STATUS_GOOD;
}                               /* sane_get_devices */


SANE_Status
sane_open (SANE_String_Const name, SANE_Handle * handle)
{
  struct sp15c *dev = first_dev;

  name = name; /* silence compilation warnings */
  /* Strange, name is not used? */

  DBG (10, "sane_open\n");

  if (!dev)
    return SANE_STATUS_INVAL;

  init_options (dev);
  *handle = dev;

  dev->use_adf = SANE_TRUE;

  dev->x_res = 200;
  dev->y_res = 200;
  dev->tl_x = 0;
  dev->tl_y = 0;
  dev->br_x = 1200 * 17 / 2;
  dev->br_y = 1200 * 11;
  dev->brightness = 128;
  dev->threshold = 128;
  dev->contrast = 128;
  dev->composition = WD_comp_LA;
  dev->opt[OPT_BRIGHTNESS].cap = SANE_CAP_INACTIVE;
  dev->opt[OPT_THRESHOLD].cap = SANE_CAP_SOFT_DETECT
    | SANE_CAP_SOFT_SELECT;
  dev->bitsperpixel = 1;
  dev->halftone = 0;
  dev->rif = 0;
  dev->bitorder = 0;
  dev->compress_type = 0;
  dev->compress_arg = 0;
  dev->vendor_id_code = 0;
  dev->outline = 0;
  dev->emphasis = 0;
  dev->auto_sep = 0;
  dev->mirroring = 0;
  dev->var_rate_dyn_thresh = 0;
  dev->white_level_follow = 0;
  dev->paper_size = 0x87;
  dev->paper_width_X = 1200 * 17 / 2;
  dev->paper_length_Y = 1200 * 11;
  dev->opt[OPT_TL_Y].constraint.range = &y_range_adf;
  dev->opt[OPT_BR_Y].constraint.range = &y_range_adf;
  adjust_width (dev, 0);

  return SANE_STATUS_GOOD;
}                               /* sane_open */


SANE_Status
sane_set_io_mode (SANE_Handle h, SANE_Bool non_blocking)
{
  h = h; 
  non_blocking = non_blocking; /* silence compilation warnings */

  DBG (10, "sane_set_io_mode\n");
  return SANE_STATUS_UNSUPPORTED;
}                               /* sane_set_io_mode */


SANE_Status
sane_get_select_fd (SANE_Handle h, SANE_Int * fdp)
{
  h = h;
  fdp = fdp; /* silence compilation warnings */

  DBG (10, "sane_get_select_fd\n");
  return SANE_STATUS_UNSUPPORTED;
}                               /* sane_get_select_fd */


const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  struct sp15c *scanner = handle;

  DBG (10, "sane_get_option_descriptor: \"%s\"\n",
       scanner->opt[option].name);

  if ((unsigned) option >= NUM_OPTIONS)
    return 0;
  return &scanner->opt[option];
}                               /* sane_get_option_descriptor */


SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
                     SANE_Action action, void *val,
                     SANE_Int * info)
{
  struct sp15c *scanner = handle;
  SANE_Status status;
  SANE_Word cap;

  if (info)
    *info = 0;

  if (scanner->scanning == SANE_TRUE)
    {
      DBG (5, "sane_control_option: device busy\n");
      return SANE_STATUS_IO_ERROR;
    }

  if (option >= NUM_OPTIONS)
    return SANE_STATUS_INVAL;

  cap = scanner->opt[option].cap;

  if (action == SANE_ACTION_GET_VALUE)
    {
      DBG (10, "sane_control_option: get value \"%s\"\n",
           scanner->opt[option].name);
      DBG (11, "\tcap = %d\n", cap);

      if (!SANE_OPTION_IS_ACTIVE (cap))
        {
          DBG (10, "\tinactive\n");
          return SANE_STATUS_INVAL;
        }

      switch (option)
        {

        case OPT_NUM_OPTS:
          *(SANE_Word *) val = NUM_OPTIONS;
          return SANE_STATUS_GOOD;

        case OPT_SOURCE:
          if (scanner->use_adf == SANE_TRUE)
            {
              strcpy (val, "ADF");
            }
          else
            {
              strcpy (val, "FB");
            }
          return SANE_STATUS_GOOD;

        case OPT_MODE:
          switch (scanner->composition)
            {
            case WD_comp_LA:
              strcpy (val, lineStr);
              break;
            case WD_comp_HT:
              strcpy (val, halfStr);
              break;
            case WD_comp_G4:
              strcpy (val, gray4Str);
              break;
            case WD_comp_G8:
              strcpy (val, gray8Str);
              break;
            case WD_comp_MC:
              strcpy (val, colorStr);
              break;
            default:
              return SANE_STATUS_INVAL;
            }
          return SANE_STATUS_GOOD;

        case OPT_TYPE:
          return SANE_STATUS_INVAL;

        case OPT_PRESCAN:
          return SANE_STATUS_INVAL;

        case OPT_X_RES:
          *(SANE_Word *) val = scanner->x_res;
          return SANE_STATUS_GOOD;

        case OPT_Y_RES:
          *(SANE_Word *) val = scanner->y_res;
          return SANE_STATUS_GOOD;

        case OPT_PREVIEW_RES:
          return SANE_STATUS_INVAL;

        case OPT_TL_X:
          *(SANE_Word *) val = SANE_FIX (iluToMm (scanner->tl_x));
          return SANE_STATUS_GOOD;

        case OPT_TL_Y:
          *(SANE_Word *) val = SANE_FIX (iluToMm (scanner->tl_y));
          return SANE_STATUS_GOOD;

        case OPT_BR_X:
          *(SANE_Word *) val = SANE_FIX (iluToMm (scanner->br_x));
          return SANE_STATUS_GOOD;

        case OPT_BR_Y:
          *(SANE_Word *) val = SANE_FIX (iluToMm (scanner->br_y));
          return SANE_STATUS_GOOD;

        case OPT_AVERAGING:
          return SANE_STATUS_INVAL;

        case OPT_BRIGHTNESS:
          *(SANE_Word *) val = scanner->brightness;
          return SANE_STATUS_GOOD;

        case OPT_THRESHOLD:
          *(SANE_Word *) val = scanner->threshold;
          return SANE_STATUS_GOOD;

        case OPT_PREVIEW:
          return SANE_STATUS_INVAL;

        }

    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      DBG (10, "sane_control_option: set value \"%s\"\n",
           scanner->opt[option].name);

      if (!SANE_OPTION_IS_ACTIVE (cap))
        {
          DBG (10, "\tinactive\n");
          return SANE_STATUS_INVAL;
        }

      if (!SANE_OPTION_IS_SETTABLE (cap))
        {
          DBG (10, "\tnot settable\n");
          return SANE_STATUS_INVAL;
        }

      status = sanei_constrain_value (scanner->opt + option, val, info);
      if (status != SANE_STATUS_GOOD)
        {
          DBG (10, "\tbad value\n");
          return status;
        }

      switch (option)
        {

        case OPT_NUM_OPTS:
          return SANE_STATUS_GOOD;

        case OPT_SOURCE:
          if (strcmp (val, "ADF") == 0)
            {
              if (scanner->use_adf == SANE_TRUE)
                return SANE_STATUS_GOOD;
              scanner->use_adf = SANE_TRUE;
              scanner->opt[OPT_TL_Y].constraint.range = &y_range_adf;
              scanner->opt[OPT_BR_Y].constraint.range = &y_range_adf;
              apply_constraints (scanner, OPT_TL_Y, &scanner->tl_y, info);
              apply_constraints (scanner, OPT_BR_Y, &scanner->br_y, info);
            }
          else if (strcmp (val, "FB") == 0)
            {
              if (scanner->use_adf == SANE_FALSE)
                return SANE_STATUS_GOOD;
              scanner->use_adf = SANE_FALSE;
              scanner->opt[OPT_TL_Y].constraint.range = &y_range_fb;
              scanner->opt[OPT_BR_Y].constraint.range = &y_range_fb;
              apply_constraints (scanner, OPT_TL_Y, &scanner->tl_y, info);
              apply_constraints (scanner, OPT_BR_Y, &scanner->br_y, info);
            }
          else
            {
              return SANE_STATUS_INVAL;
            }
          if (info)
            {
              *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
            }
          return SANE_STATUS_GOOD;

        case OPT_MODE:
          if (strcmp (val, lineStr) == 0)
            {
              if (scanner->composition == WD_comp_LA)
                return SANE_STATUS_GOOD;
              scanner->composition = WD_comp_LA;
              scanner->bitsperpixel = 1;
              scanner->opt[OPT_BRIGHTNESS].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_THRESHOLD].cap = SANE_CAP_SOFT_DETECT
                | SANE_CAP_SOFT_SELECT;
              scanner->vendor_id_code = 0;
            }
          else if (strcmp (val, halfStr) == 0)
            {
              if (scanner->composition == WD_comp_HT)
                return SANE_STATUS_GOOD;
              scanner->composition = WD_comp_HT;
              scanner->bitsperpixel = 1;
              scanner->opt[OPT_BRIGHTNESS].cap = SANE_CAP_SOFT_DETECT
                | SANE_CAP_SOFT_SELECT;
              scanner->opt[OPT_THRESHOLD].cap = SANE_CAP_INACTIVE;
              scanner->vendor_id_code = 0;
            }
          else if (strcmp (val, gray4Str) == 0)
            {
              if (scanner->composition == WD_comp_G4)
                return SANE_STATUS_GOOD;
              scanner->composition = WD_comp_G4;
              scanner->bitsperpixel = 4;
              scanner->opt[OPT_BRIGHTNESS].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_THRESHOLD].cap = SANE_CAP_INACTIVE;
              scanner->vendor_id_code = 0;
            }
          else if (strcmp (val, gray8Str) == 0)
            {
              if (scanner->composition == WD_comp_G8)
                return SANE_STATUS_GOOD;
              scanner->composition = WD_comp_G8;
              scanner->bitsperpixel = 8;
              scanner->opt[OPT_BRIGHTNESS].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_THRESHOLD].cap = SANE_CAP_INACTIVE;
              scanner->vendor_id_code = 0;
            }
          else if (strcmp (val, colorStr) == 0)
            {
              if (scanner->composition == WD_comp_MC)
                return SANE_STATUS_GOOD;
              scanner->composition = WD_comp_MC;
              scanner->bitsperpixel = 8;
              scanner->opt[OPT_BRIGHTNESS].cap = SANE_CAP_INACTIVE;
              scanner->opt[OPT_THRESHOLD].cap = SANE_CAP_INACTIVE;
              scanner->vendor_id_code = 0xff;
            }
          else
            {
              return SANE_STATUS_INVAL;
            }
          if (info)
            {
              *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
            }
          return SANE_STATUS_GOOD;

        case OPT_TYPE:
          return SANE_STATUS_INVAL;

        case OPT_PRESCAN:
          return SANE_STATUS_INVAL;

        case OPT_X_RES:
          scanner->x_res = (*(SANE_Word *) val);
          adjust_width (scanner, info);
          if (info)
            {
              *info |= SANE_INFO_RELOAD_PARAMS;
            }
          return SANE_STATUS_GOOD;

        case OPT_Y_RES:
          scanner->y_res = (*(SANE_Word *) val);
          if (info)
            {
              *info |= SANE_INFO_RELOAD_PARAMS;
            }
          return SANE_STATUS_GOOD;

        case OPT_PREVIEW_RES:
          return SANE_STATUS_INVAL;

        case OPT_TL_X:
          scanner->tl_x = mmToIlu (SANE_UNFIX (*(SANE_Word *) val));
          adjust_width (scanner, info);
          *(SANE_Word *) val = SANE_FIX (iluToMm (scanner->tl_x));
          if (info)
            {
              *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_INEXACT;
            }
          return SANE_STATUS_GOOD;

        case OPT_TL_Y:
          scanner->tl_y = mmToIlu (SANE_UNFIX (*(SANE_Word *) val));
          *(SANE_Word *) val = SANE_FIX (iluToMm (scanner->tl_y));
          if (info)
            {
              *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_INEXACT;
            }
          return SANE_STATUS_GOOD;

        case OPT_BR_X:
          scanner->br_x = mmToIlu (SANE_UNFIX (*(SANE_Word *) val));
          adjust_width (scanner, info);
          *(SANE_Word *) val = SANE_FIX (iluToMm (scanner->br_x));
          if (info)
            {
              *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_INEXACT;
            }
          return SANE_STATUS_GOOD;

        case OPT_BR_Y:
          scanner->br_y = mmToIlu (SANE_UNFIX (*(SANE_Word *) val));
          *(SANE_Word *) val = SANE_FIX (iluToMm (scanner->br_y));
          if (info)
            {
              *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_INEXACT;
            }
          return SANE_STATUS_GOOD;

        case OPT_AVERAGING:
          return SANE_STATUS_INVAL;

        case OPT_BRIGHTNESS:
          scanner->brightness = *(SANE_Word *) val;
          return SANE_STATUS_GOOD;

        case OPT_THRESHOLD:
          scanner->threshold = *(SANE_Word *) val;
          return SANE_STATUS_GOOD;

        }                       /* switch */
    }                           /* else */
  return SANE_STATUS_INVAL;
}                               /* sane_control_option */


SANE_Status
sane_start (SANE_Handle handle)
{
  struct sp15c *scanner = handle;
  int fds[2];
  int ret;

  DBG (10, "sane_start\n");
  if (scanner->scanning == SANE_TRUE)
    {
      DBG (5, "sane_start: device busy\n");
      return SANE_STATUS_DEVICE_BUSY;
    }

  if (scanner->sfd < 0)
    {                           /* first call */
      if (sanei_scsi_open (scanner->sane.name, &(scanner->sfd),
                           sense_handler, 0) != SANE_STATUS_GOOD)
        {
          DBG (1, "sane_start: open of %s failed:\n",
               scanner->sane.name);
          return SANE_STATUS_INVAL;
        }
    }
  scanner->scanning = SANE_TRUE;


  if ((ret = sp15c_check_values (scanner)) != 0)
    {                           /* Verify values */
      DBG (1, "sane_start: ERROR: invalid scan-values\n");
      sanei_scsi_close (scanner->sfd);
      scanner->scanning = SANE_FALSE;
      scanner->sfd = -1;
      return SANE_STATUS_INVAL;
    }

  if ((ret = sp15c_grab_scanner (scanner)))
    {
      DBG (5, "sane_start: unable to reserve scanner\n");
      sanei_scsi_close (scanner->sfd);
      scanner->scanning = SANE_FALSE;
      scanner->sfd = -1;
      return ret;
    }

  if ((ret = sp15c_set_window_param (scanner, 0)))
    {
      DBG (5, "sane_start: ERROR: failed to set window\n");
      sp15c_free_scanner (scanner);
      sanei_scsi_close (scanner->sfd);
      scanner->scanning = SANE_FALSE;
      scanner->sfd = -1;
      return ret;
    }

  /* Since the SET WINDOW can specify the use of the ADF, and since the
     ScanPartner 15C automatically pre-loads sheets from the ADF, it
     is only necessary to see if any paper is available.  The OEM
     Manual offers the OBJECT POSITION command, but it causes the
     carrier unit into a "homing" cycle.  The undocumented MEDIA CHECK
     command avoids the "homing" cycle.  (Note the SET WINDOW command
     had to use the color scanning vendor unique parameters, regardless
     of scanning mode, so that it could invoke the ADF.) */
  if (scanner->use_adf == SANE_TRUE
      && (ret = sp15c_media_check (scanner)))
    {
      DBG (5, "sane_start: WARNING: ADF empty\n");
      sp15c_free_scanner (scanner);
      sanei_scsi_close (scanner->sfd);
      scanner->scanning = SANE_FALSE;
      scanner->sfd = -1;
      return ret;
    }

  swap_res (scanner);

  DBG (10, "\tbytes per line = %d\n", bytes_per_line (scanner));
  DBG (10, "\tpixels_per_line = %d\n", pixels_per_line (scanner));
  DBG (10, "\tlines = %d\n", lines_per_scan (scanner));
  DBG (10, "\tbrightness (halftone) = %d\n", scanner->brightness);
  DBG (10, "\tthreshold (line art) = %d\n", scanner->threshold);

  if (scanner->composition == WD_comp_MC
      && (ret = sp15c_start_scan (scanner)))
    {
      DBG (5, "sane_start: start_scan failed\n");
      sp15c_free_scanner (scanner);
      sanei_scsi_close (scanner->sfd);
      scanner->scanning = SANE_FALSE;
      scanner->sfd = -1;
      return SANE_STATUS_INVAL;
    }

  /* create a pipe, fds[0]=read-fd, fds[1]=write-fd */
  if (pipe (fds) < 0)
    {
      DBG (1, "ERROR: could not create pipe\n");
      swap_res (scanner);
      scanner->scanning = SANE_FALSE;
      sp15c_free_scanner (scanner);
      sanei_scsi_close (scanner->sfd);
      scanner->sfd = -1;
      return SANE_STATUS_IO_ERROR;
    }

  scanner->pipe = fds[0];
  scanner->reader_pipe = fds[1];
  scanner->reader_pid = sanei_thread_begin (reader_process, (void *) scanner);

  if (sanei_thread_is_forked ())
    close (scanner->reader_pipe);

  DBG (10, "sane_start: ok\n");
  return SANE_STATUS_GOOD;
}                               /* sane_start */


SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  struct sp15c *scanner = handle;

  DBG (10, "sane_get_parameters\n");
  if (scanner->composition == WD_comp_MC)
    {
      params->format = SANE_FRAME_RGB;
      params->depth = 8;
    }
  else if (scanner->composition == WD_comp_LA
           || scanner->composition == WD_comp_HT)
    {
      params->format = SANE_FRAME_GRAY;
      params->depth = 1;
    }
  else
    {
      params->format = SANE_FRAME_GRAY;
      params->depth = 8;
    }

  params->pixels_per_line = pixels_per_line (scanner);
  params->lines = lines_per_scan (scanner);
  params->bytes_per_line = bytes_per_line (scanner);
  params->last_frame = 1;

  DBG (10, "\tdepth %d\n", params->depth);
  DBG (10, "\tlines %d\n", params->lines);
  DBG (10, "\tpixels_per_line %d\n", params->pixels_per_line);
  DBG (10, "\tbytes_per_line %d\n", params->bytes_per_line);
/*************************/
  DBG (10, "\tlength %d\n", scanner->br_y - scanner->tl_y);
  DBG (10, "\t(nom.) width %d\n", scanner->br_x - scanner->tl_x);
  DBG (10, "\tx res %d\n", scanner->x_res);
  DBG (10, "\ty res %d\n", scanner->y_res);
/*************************/

  return SANE_STATUS_GOOD;
}                               /* sane_get_parameters */


SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf,
           SANE_Int max_len, SANE_Int * len)
{
  struct sp15c *scanner = handle;
  ssize_t nread;

  DBG (10, "sane_read\n");
  *len = 0;

  nread = read (scanner->pipe, buf, max_len);
  DBG (10, "sane_read: read %ld bytes of %ld\n",
       (long) nread, (long) max_len);

  if (scanner->scanning == SANE_FALSE)
    {
      /* PREDICATE WAS (!(scanner->scanning))  */
      return do_cancel (scanner);
    }

  if (nread < 0)
    {
      if (errno == EAGAIN)
        {
          return SANE_STATUS_GOOD;
        }
      else
        {
          do_cancel (scanner);
          return SANE_STATUS_IO_ERROR;
        }
    }

  *len = nread;

  if (nread == 0)
    return do_eof (scanner);    /* close pipe */

  return SANE_STATUS_GOOD;
}                               /* sane_read */


void
sane_cancel (SANE_Handle h)
{
  DBG (10, "sane_cancel\n");
  do_cancel ((struct sp15c *) h);
}                               /* sane_cancel */


void
sane_close (SANE_Handle handle)
{
  DBG (10, "sane_close\n");
  if (((struct sp15c *) handle)->scanning == SANE_TRUE)
    do_cancel (handle);
}                               /* sane_close */


void
sane_exit (void)
{
  struct sp15c *dev, *next;

  DBG (10, "sane_exit\n");

  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      free (dev->devicename);
      free (dev->buffer);
      free (dev);
    }
  
  if (devlist)
    free (devlist);
}                               /* sane_exit */

/* }################ internal (support) routines ################{ */

static SANE_Status
attach_scanner (const char *devicename, struct sp15c **devp)
{
  struct sp15c *dev;
  int sfd;

  DBG (15, "attach_scanner: %s\n", devicename);

  for (dev = first_dev; dev; dev = dev->next)
    {
      if (strcmp (dev->sane.name, devicename) == 0)
        {
          if (devp)
            {
              *devp = dev;
            }
          DBG (5, "attach_scanner: scanner already attached (is ok)!\n");
          return SANE_STATUS_GOOD;
        }
    }

  DBG (15, "attach_scanner: opening %s\n", devicename);
  if (sanei_scsi_open (devicename, &sfd, sense_handler, 0) != 0)
    {
      DBG (5, "attach_scanner: open failed\n");
      return SANE_STATUS_INVAL;
    }

  if (NULL == (dev = malloc (sizeof (*dev))))
    return SANE_STATUS_NO_MEM;

  dev->row_bufsize = (sanei_scsi_max_request_size < (64 * 1024))
    ? sanei_scsi_max_request_size
    : 64 * 1024;

  if ((dev->buffer = malloc (dev->row_bufsize)) == NULL)
    return SANE_STATUS_NO_MEM;

  dev->devicename = strdup (devicename);
  dev->sfd = sfd;

  if (sp15c_identify_scanner (dev) != 0)
    {
      DBG (5, "attach_scanner: scanner-identification failed\n");
      sanei_scsi_close (dev->sfd);
      free (dev->buffer);
      free (dev);
      return SANE_STATUS_INVAL;
    }

#if 0
  /* Get MUD (via mode_sense), internal info (via get_internal_info), and
     * initialize values */
  coolscan_initialize_values (dev);
#endif

  /* Why? */
  sanei_scsi_close (dev->sfd);
  dev->sfd = -1;

  dev->sane.name = dev->devicename;
  dev->sane.vendor = dev->vendor;
  dev->sane.model = dev->product;
  dev->sane.type = "flatbed/ADF scanner";

  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    {
      *devp = dev;
    }

  DBG (15, "attach_scanner: done\n");

  return SANE_STATUS_GOOD;
}                               /* attach_scanner */

static SANE_Status
attach_one (const char *name)
{
  return attach_scanner (name, 0);
}                               /* attach_one */

static SANE_Status
sense_handler (int scsi_fd, u_char * result, void *arg)
{
  scsi_fd = scsi_fd;
  arg = arg; /* silence compilation warnings */

  return request_sense_parse (result);
}                               /* sense_handler */

static int
request_sense_parse (u_char * sensed_data)
{
  unsigned int ret, sense, asc, ascq;
  sense = get_RS_sense_key (sensed_data);
  asc = get_RS_ASC (sensed_data);
  ascq = get_RS_ASCQ (sensed_data);

  ret = SANE_STATUS_IO_ERROR;

  switch (sense)
    {
    case 0x0:                   /* No Sense */
      DBG (5, "\t%d/%d/%d: Scanner ready\n", sense, asc, ascq);
      return SANE_STATUS_GOOD;

    case 0x2:                   /* Not Ready */
      if ((0x00 == asc) && (0x00 == ascq))
        {
          DBG (1, "\t%d/%d/%d: Not Ready \n", sense, asc, ascq);
        }
      else
        {
          DBG (1, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", sense, asc, ascq);
        }
      break;

    case 0x3:                   /* Medium Error */
      if ((0x80 == asc) && (0x01 == ascq))
        {
          DBG (1, "\t%d/%d/%d: Jam \n", sense, asc, ascq);
          ret = SANE_STATUS_JAMMED;
        }
      else if ((0x80 == asc) && (0x02 == ascq))
        {
          DBG (1, "\t%d/%d/%d: ADF cover open \n", sense, asc, ascq);
          ret = SANE_STATUS_COVER_OPEN;
        }
      else if ((0x80 == asc) && (0x03 == ascq))
        {
          DBG (1, "\t%d/%d/%d: ADF empty \n", sense, asc, ascq);
          ret = SANE_STATUS_NO_DOCS;
        }
      else
        {
          DBG (1, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", sense, asc, ascq);
        }
      break;

    case 0x4:                   /* Hardware Error */
      if ((0x80 == asc) && (0x01 == ascq))
        {
          DBG (1, "\t%d/%d/%d: FB motor fuse \n", sense, asc, ascq);
        }
      else if ((0x80 == asc) && (0x02 == ascq))
        {
          DBG (1, "\t%d/%d/%d: heater fuse \n", sense, asc, ascq);
        }
      else if ((0x80 == asc) && (0x04 == ascq))
        {
          DBG (1, "\t%d/%d/%d: ADF motor fuse \n", sense, asc, ascq);
        }
      else if ((0x80 == asc) && (0x05 == ascq))
        {
          DBG (1, "\t%d/%d/%d: mechanical alarm \n", sense, asc, ascq);
        }
      else if ((0x80 == asc) && (0x06 == ascq))
        {
          DBG (1, "\t%d/%d/%d: optical alarm \n", sense, asc, ascq);
        }
      else if ((0x44 == asc) && (0x00 == ascq))
        {
          DBG (1, "\t%d/%d/%d: abnormal internal target \n", sense, asc, ascq);
        }
      else if ((0x47 == asc) && (0x00 == ascq))
        {
          DBG (1, "\t%d/%d/%d: SCSI parity error \n", sense, asc, ascq);
        }
      else
        {
          DBG (1, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", sense, asc, ascq);
        }
      break;

    case 0x5:                   /* Illegal Request */
      if ((0x20 == asc) && (0x00 == ascq))
        {
          DBG (1, "\t%d/%d/%d: Invalid command \n", sense, asc, ascq);
          ret = SANE_STATUS_INVAL;
        }
      else if ((0x24 == asc) && (0x00 == ascq))
        {
          DBG (1, "\t%d/%d/%d: Invalid field in CDB \n", sense, asc, ascq);
          ret = SANE_STATUS_INVAL;
        }
      else if ((0x25 == asc) && (0x00 == ascq))
        {
          DBG (1, "\t%d/%d/%d: Unsupported logical unit \n", sense, asc, ascq);
          ret = SANE_STATUS_UNSUPPORTED;
        }
      else if ((0x26 == asc) && (0x00 == ascq))
        {
          DBG (1, "\t%d/%d/%d: Invalid field in parm list \n", sense, asc, ascq);
          ret = SANE_STATUS_INVAL;
        }
      else if ((0x2C == asc) && (0x02 == ascq))
        {
          DBG (1, "\t%d/%d/%d: wrong window combination \n", sense, asc, ascq);
          ret = SANE_STATUS_INVAL;
        }
      else
        {
          DBG (1, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", sense, asc, ascq);
        }
      break;

    case 0x6:                   /* Unit Attention */
      if ((0x00 == asc) && (0x00 == ascq))
        {
          DBG (1, "\t%d/%d/%d: UNIT ATTENTION \n", sense, asc, ascq);
        }
      else
        {
          DBG (1, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", sense, asc, ascq);
        }
      break;

    case 0xb:                   /* Aborted Command */
      if ((0x43 == asc) && (0x00 == ascq))
        {
          DBG (1, "\t%d/%d/%d: Message error \n", sense, asc, ascq);
        }
      else if ((0x80 == asc) && (0x01 == ascq))
        {
          DBG (1, "\t%d/%d/%d: Image transfer error \n", sense, asc, ascq);
        }
      else
        {
          DBG (1, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", sense, asc, ascq);
        }
      break;

    default:
      DBG (1, "\tUnknown - Sense=%d, ASC=%d, ASCQ=%d\n", sense, asc, ascq);
    }
  return ret;
}                               /* request_sense_parse */

static SANE_Status
sp15c_identify_scanner (struct sp15c *s)
{
  char vendor[9];
  char product[0x11];
  char version[5];
  char *pp;
  SANE_Status ret;

  DBG (10, "identify_scanner\n");

  vendor[8] = product[0x10] = version[4] = 0;

  if ((ret = sp15c_do_inquiry (s)))
    {
      DBG (5, "identify_scanner: inquiry failed\n");
      return ret;
    }
  if (get_IN_periph_devtype (s->buffer) != IN_periph_devtype_scanner)
    {
      DBG (5, "identify_scanner: not a scanner\n");
      return SANE_STATUS_INVAL;
    }

  get_IN_vendor ((char *) s->buffer, vendor);
  get_IN_product ((char *)s->buffer, product);
  get_IN_version ((char *)s->buffer, version);

  if (strncmp ("FCPA    ", vendor, 8))
    {
      DBG (5, "identify_scanner: \"%s\" isn't a Fujitsu product\n", vendor);
      return 1;
    }

  pp = &vendor[8];
  vendor[8] = ' ';
  while (*pp == ' ')
    {
      *pp-- = '\0';
    }

  pp = &product[0x10];
  product[0x10] = ' ';
  while (*(pp - 1) == ' ')
    {
      *pp-- = '\0';
    }                           /* leave one blank at the end! */

  pp = &version[4];
  version[4] = ' ';
  while (*pp == ' ')
    {
      *pp-- = '\0';
    }

  if (get_IN_adf (s->buffer))
    {
      s->autofeeder = 1;
    }
  else
    {
      s->autofeeder = 0;
    }

  DBG (10, "Found %s scanner %s version %s on device %s  %x/%x/%x\n",
       vendor, product, version, s->devicename,
       s->autofeeder, get_IN_color_mode (s->buffer),
       get_IN_color_seq (s->buffer));

  vendor[8] = '\0';
  product[16] = '\0';
  version[4] = '\0';

  strncpy (s->vendor, vendor, 9);
  strncpy (s->product, product, 17);
  strncpy (s->version, version, 5);

  return SANE_STATUS_GOOD;
}                               /* sp15c_identify_scanner */

static SANE_Status
sp15c_do_inquiry (struct sp15c *s)
{
  static SANE_Status ret;
  
  DBG (10, "do_inquiry\n");

  memset (s->buffer, '\0', 256);        /* clear buffer */
  set_IN_return_size (inquiryB.cmd, 96);

  ret = do_scsi_cmd (s->sfd, inquiryB.cmd, inquiryB.size, s->buffer, 96);
  return ret;
}                               /* sp15c_do_inquiry */

static SANE_Status
do_scsi_cmd (int fd, unsigned char *cmd, int cmd_len, unsigned char *out, size_t out_len)
{
  SANE_Status ret;
  size_t ol = out_len;

  hexdump (20, "<cmd<", cmd, cmd_len);

  ret = sanei_scsi_cmd (fd, cmd, cmd_len, out, &ol);
  if ((out_len != 0) && (out_len != ol))
    {
      DBG (1, "sanei_scsi_cmd: asked %lu bytes, got %lu\n",
           (u_long) out_len, (u_long) ol);
    }
  if (ret)
    {
      DBG (1, "sanei_scsi_cmd: returning 0x%08x\n", ret);
    }
  DBG (10, "sanei_scsi_cmd: returning %lu bytes:\n", (u_long) ol);
  if (out != NULL && out_len != 0)
    hexdump (15, ">rslt>", out, (out_len > 0x60) ? 0x60 : out_len);

  return ret;
}                               /* do_scsi_cmd */

static void
hexdump (int level, char *comment, unsigned char *p, int l)
{
  int i;
  char line[128];
  char *ptr;

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
          sprintf (ptr, "%3.3d:", i);
          ptr += 4;
        }
      sprintf (ptr, " %2.2x", *p);
      ptr += 3;
    }
  *ptr = '\0';
  DBG (level, "%s\n", line);
}                               /* hexdump */

static SANE_Status
init_options (struct sp15c *scanner)
{
  int i;

  DBG (10, "init_options\n");

  memset (scanner->opt, 0, sizeof (scanner->opt));

  for (i = 0; i < NUM_OPTIONS; ++i)
    {
      scanner->opt[i].size = sizeof (SANE_Word);
      scanner->opt[i].cap = SANE_CAP_INACTIVE;
    }

  scanner->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  scanner->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  scanner->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  scanner->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;

/************** "Mode" group: **************/
  scanner->opt[OPT_MODE_GROUP].title = "Scan Mode";
  scanner->opt[OPT_MODE_GROUP].desc = "";
  scanner->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  scanner->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* source */

  scanner->opt[OPT_SOURCE].name = SANE_NAME_SCAN_SOURCE;
  scanner->opt[OPT_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
  scanner->opt[OPT_SOURCE].desc = SANE_DESC_SCAN_SOURCE;
  scanner->opt[OPT_SOURCE].type = SANE_TYPE_STRING;
  scanner->opt[OPT_SOURCE].size = max_string_size (source_list);
  scanner->opt[OPT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_SOURCE].constraint.string_list = source_list;
  if (scanner->autofeeder)
    {
      scanner->opt[OPT_SOURCE].cap = SANE_CAP_SOFT_SELECT
        | SANE_CAP_SOFT_DETECT;
    }

  /* scan mode */
  scanner->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  scanner->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  scanner->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  scanner->opt[OPT_MODE].type = SANE_TYPE_STRING;
  scanner->opt[OPT_MODE].size = max_string_size (scan_mode_list);
  scanner->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_MODE].constraint.string_list = scan_mode_list;
  scanner->opt[OPT_MODE].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  /* negative */
  scanner->opt[OPT_TYPE].name = "type";
  scanner->opt[OPT_TYPE].title = "Film type";
  scanner->opt[OPT_TYPE].desc = "positive or negative image";
  scanner->opt[OPT_TYPE].type = SANE_TYPE_STRING;
  scanner->opt[OPT_TYPE].size = max_string_size (type_list);
  scanner->opt[OPT_TYPE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_TYPE].constraint.string_list = type_list;

  scanner->opt[OPT_PRESCAN].name = "prescan";
  scanner->opt[OPT_PRESCAN].title = "Prescan";
  scanner->opt[OPT_PRESCAN].desc = "Perform a prescan during preview";
  scanner->opt[OPT_PRESCAN].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_PRESCAN].unit = SANE_UNIT_NONE;

  /* resolution */
  scanner->opt[OPT_X_RES].name = SANE_NAME_SCAN_RESOLUTION;
  scanner->opt[OPT_X_RES].title = SANE_TITLE_SCAN_X_RESOLUTION;
  scanner->opt[OPT_X_RES].desc = SANE_DESC_SCAN_X_RESOLUTION;
  scanner->opt[OPT_X_RES].type = SANE_TYPE_INT;
  scanner->opt[OPT_X_RES].unit = SANE_UNIT_DPI;
  scanner->opt[OPT_X_RES].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  scanner->opt[OPT_X_RES].constraint.word_list = x_res_list;
  scanner->opt[OPT_X_RES].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_Y_RES].name = SANE_NAME_SCAN_Y_RESOLUTION;
  scanner->opt[OPT_Y_RES].title = SANE_TITLE_SCAN_Y_RESOLUTION;
  scanner->opt[OPT_Y_RES].desc = SANE_DESC_SCAN_Y_RESOLUTION;
  scanner->opt[OPT_Y_RES].type = SANE_TYPE_INT;
  scanner->opt[OPT_Y_RES].unit = SANE_UNIT_DPI;
  scanner->opt[OPT_Y_RES].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  scanner->opt[OPT_Y_RES].constraint.word_list = y_res_list;
  scanner->opt[OPT_Y_RES].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_PREVIEW_RES].name = "preview-resolution";
  scanner->opt[OPT_PREVIEW_RES].title = "Preview resolution";
  scanner->opt[OPT_PREVIEW_RES].desc = SANE_DESC_SCAN_RESOLUTION;
  scanner->opt[OPT_PREVIEW_RES].type = SANE_TYPE_INT;
  scanner->opt[OPT_PREVIEW_RES].unit = SANE_UNIT_DPI;
  scanner->opt[OPT_PREVIEW_RES].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  scanner->opt[OPT_PREVIEW_RES].constraint.word_list = resolution_list;

/************** "Geometry" group: **************/
  scanner->opt[OPT_GEOMETRY_GROUP].title = "Geometry";
  scanner->opt[OPT_GEOMETRY_GROUP].desc = "";
  scanner->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  scanner->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* top-left x */
  scanner->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  scanner->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  scanner->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  scanner->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  scanner->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  scanner->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_TL_X].constraint.range = &x_range;
  scanner->opt[OPT_TL_X].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  /* top-left y */
  scanner->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  scanner->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  scanner->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  scanner->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  scanner->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  scanner->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_TL_Y].constraint.range = &y_range_fb;
  scanner->opt[OPT_TL_Y].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  /* bottom-right x */
  scanner->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  scanner->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  scanner->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  scanner->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  scanner->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  scanner->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_BR_X].constraint.range = &x_range;
  scanner->opt[OPT_BR_X].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  /* bottom-right y */
  scanner->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  scanner->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  scanner->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  scanner->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  scanner->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  scanner->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_BR_Y].constraint.range = &y_range_fb;
  scanner->opt[OPT_BR_Y].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;


  /* ------------------------------ */

/************** "Enhancement" group: **************/
  scanner->opt[OPT_ENHANCEMENT_GROUP].title = "Enhancement";
  scanner->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  scanner->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  scanner->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  scanner->opt[OPT_AVERAGING].name = "averaging";
  scanner->opt[OPT_AVERAGING].title = "Averaging";
  scanner->opt[OPT_AVERAGING].desc = "Averaging";
  scanner->opt[OPT_AVERAGING].type = SANE_TYPE_BOOL;
  scanner->opt[OPT_AVERAGING].unit = SANE_UNIT_NONE;

  scanner->opt[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
  scanner->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  scanner->opt[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
  scanner->opt[OPT_BRIGHTNESS].type = SANE_TYPE_INT;
  scanner->opt[OPT_BRIGHTNESS].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_BRIGHTNESS].constraint.range = &brightness_range;
  scanner->opt[OPT_BRIGHTNESS].cap = SANE_CAP_SOFT_DETECT;

  scanner->opt[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
  scanner->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
  scanner->opt[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
  scanner->opt[OPT_THRESHOLD].type = SANE_TYPE_INT;
  scanner->opt[OPT_THRESHOLD].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_THRESHOLD].constraint.range = &threshold_range;
  scanner->opt[OPT_THRESHOLD].cap = SANE_CAP_SOFT_DETECT;

  /* ------------------------------ */

/************** "Advanced" group: **************/
  scanner->opt[OPT_ADVANCED_GROUP].title = "Advanced";
  scanner->opt[OPT_ADVANCED_GROUP].desc = "";
  scanner->opt[OPT_ADVANCED_GROUP].type = SANE_TYPE_GROUP;
  scanner->opt[OPT_ADVANCED_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* preview */
  scanner->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  scanner->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  scanner->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  scanner->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;

  DBG (10, "init_options:ok\n");
  return SANE_STATUS_GOOD;
}                               /* init_options */

static int
sp15c_check_values (struct sp15c *s)
{
  if (s->use_adf == SANE_TRUE && s->autofeeder == 0)
    {
      DBG (1, "sp15c_check_values: %s\n",
           "ERROR: ADF-MODE NOT SUPPORTED BY SCANNER, ABORTING");
      return (1);
    }
  return (0);
}                               /* sp15c_check_values */

/* sp15c_free_scanner should go through the following sequence:
 * OBJECT POSITION DISCHARGE
 *     GOOD
 * RELEASE UNIT
 *     GOOD
 */
static int
sp15c_free_scanner (struct sp15c *s)
{
  int ret;
  DBG (10, "sp15c_free_scanner\n");
#if 0
  /* hmg: reports from several people show that this code ejects two pages
     instead of one. So I've commented it out for now. */
  ret = sp15c_object_discharge (s);
  if (ret)
    return ret;
#endif
  
  wait_scanner (s);

  ret = do_scsi_cmd (s->sfd, release_unitB.cmd, release_unitB.size, NULL, 0);
  if (ret)
    return ret;

  DBG (10, "sp15c_free_scanner: ok\n");
  return ret;
}                               /* sp15c_free_scanner */

/* sp15c_grab_scanner should go through the following command sequence:
 * TEST UNIT READY
 *     CHECK CONDITION  \
 * REQUEST SENSE         > These should be handled automagically by
 *     UNIT ATTENTION   /  the kernel if they happen (powerup/reset)
 * TEST UNIT READY
 *     GOOD
 * RESERVE UNIT
 *     GOOD
 * 
 * It is then responsible for installing appropriate signal handlers
 * to call emergency_give_scanner() if user aborts.
 */

static int
sp15c_grab_scanner (struct sp15c *s)
{
  int ret;

  DBG (10, "sp15c_grab_scanner\n");
  wait_scanner (s);

  ret = do_scsi_cmd (s->sfd, reserve_unitB.cmd, reserve_unitB.size, NULL, 0);
  if (ret)
    return ret;

  DBG (10, "sp15c_grab_scanner: ok\n");
  return 0;
}                               /* sp15c_grab_scanner */

/* 
 *  wait_scanner spins until TEST_UNIT_READY returns 0 (GOOD)
 *  returns 0 on success,
 *  returns -1 on error or timeout
 */
static int
wait_scanner (struct sp15c *s)
{
  int ret = -1;
  int cnt = 0;

  DBG (10, "wait_scanner\n");

  while (ret != 0)
    {
      ret = do_scsi_cmd (s->sfd, test_unit_readyB.cmd,
                         test_unit_readyB.size, 0, 0);
      if (ret == SANE_STATUS_DEVICE_BUSY)
        {
          usleep (50000);       /* wait 0.05 seconds */
          /* 20 sec. max (prescan takes up to 15 sec.) */
          if (cnt++ > 400)
            {
              DBG (1, "wait_scanner: scanner does NOT get ready\n");
              return -1;
            }
        }
      else if (ret == SANE_STATUS_GOOD)
        {
          DBG (10, "wait_scanner: ok\n");
          return ret;
        }
      else
        {
          DBG (1, "wait_scanner: unit ready failed (%s)\n",
               sane_strstatus (ret));
        }
    }
  DBG (10, "wait_scanner: ok\n");
  return 0;
}                               /* wait_scanner */

/* As noted above, this command has been supplanted by the
   MEDIA CHECK command.  Nevertheless, it's still available
   here in case some future need is discovered. */
static int
sp15c_object_position (struct sp15c *s)
{
  int ret;
  DBG (10, "sp15c_object_position\n");
  if (s->use_adf != SANE_TRUE)
    {
      return SANE_STATUS_GOOD;
    }
  if (s->autofeeder == 0)
    {
      DBG (10, "sp15c_object_position: Autofeeder not present.\n");
      return SANE_STATUS_UNSUPPORTED;
    }
  memcpy (s->buffer, object_positionB.cmd, object_positionB.size);
  set_OP_autofeed (s->buffer, OP_Feed);
  ret = do_scsi_cmd (s->sfd, s->buffer,
                     object_positionB.size, NULL, 0);
  if (ret != SANE_STATUS_GOOD)
    {
      return ret;
    }
  wait_scanner (s);
  DBG (10, "sp15c_object_position: ok\n");
  return ret;
}                               /* sp15c_object_position */

static int
sp15c_media_check (struct sp15c *s)
{
  static SANE_Status ret;
  DBG (10, "sp15c_media_check\n");
  if (s->use_adf != SANE_TRUE)
    {
      return SANE_STATUS_GOOD;
    }
  if (s->autofeeder == 0)
    {
      DBG (10, "sp15c_media_check: Autofeeder not present.\n");
      return SANE_STATUS_UNSUPPORTED;
    }

  memset (s->buffer, '\0', 256);        /* clear buffer */
  set_MC_return_size (media_checkB.cmd, 1);

  ret = do_scsi_cmd (s->sfd, media_checkB.cmd, media_checkB.size, s->buffer, 1);

  if (ret != SANE_STATUS_GOOD)
    {
      return ret;
    }
  wait_scanner (s);

  if (get_MC_adf_status (s->buffer) == MC_ADF_OK)
    {
      DBG (10, "sp15c_media_check: ok\n");
      return SANE_STATUS_GOOD;
    }
  return SANE_STATUS_NO_DOCS;

}                               /* sp15c_media_check */

static SANE_Status
do_cancel (struct sp15c *scanner)
{
  DBG (10, "do_cancel\n");
  swap_res (scanner);

  do_eof (scanner);             /* close pipe and reposition scanner */

  if (scanner->reader_pid != -1)
    {
      int exit_status;
      DBG (10, "do_cancel: kill reader_process\n");
      /* ensure child knows it's time to stop: */
      sanei_thread_kill (scanner->reader_pid);
      DBG (50, "wait for scanner to stop\n");
      sanei_thread_waitpid (scanner->reader_pid, &exit_status);
      scanner->reader_pid = -1;
    }

  if (scanner->sfd >= 0)
    {
      sp15c_free_scanner (scanner);
      DBG (10, "do_cancel: close filedescriptor\n");
      sanei_scsi_close (scanner->sfd);
      scanner->sfd = -1;
    }

  return SANE_STATUS_CANCELLED;
}                               /* do_cancel */

static void
swap_res (struct sp15c *s)
{
  s = s; /* silence compilation warnings */
  
  /* for the time being, do nothing */
}                               /* swap_res */

static int
sp15c_object_discharge (struct sp15c *s)
{
  int ret;

  DBG (10, "sp15c_object_discharge\n");
  if (s->use_adf != SANE_TRUE)
    {
      return SANE_STATUS_GOOD;
    }

  memcpy (s->buffer, object_positionB.cmd, object_positionB.size);
  set_OP_autofeed (s->buffer, OP_Discharge);
  ret = do_scsi_cmd (s->sfd, s->buffer,
                     object_positionB.size, NULL, 0);
  wait_scanner (s);
  DBG (10, "sp15c_object_discharge: ok\n");
  return ret;
}                               /* sp15c_object_discharge */

static int
sp15c_set_window_param (struct sp15c *s, int prescan)
{
  unsigned char buffer_r[WDB_size_max];
  int ret;
  int active_buffer_size;

  prescan = prescan;   /* silence compilation warnings */
  
  wait_scanner (s);
  DBG (10, "set_window_param\n");
  memset (buffer_r, '\0', WDB_size_max);        /* clear buffer */
  memcpy (buffer_r, window_descriptor_blockB.cmd,
          window_descriptor_blockB.size);       /* copy preset data */

  set_WD_wid (buffer_r, WD_wid_all);    /* window identifier */

  set_WD_Xres (buffer_r, s->x_res);     /* x resolution in dpi */
  set_WD_Yres (buffer_r, s->y_res);     /* y resolution in dpi */

  set_WD_ULX (buffer_r, s->tl_x);       /* top left x */
  set_WD_ULY (buffer_r, s->tl_y);       /* top left y */

  set_WD_width (buffer_r, s->br_x - s->tl_x);
  set_WD_length (buffer_r, s->br_y - s->tl_y);

  set_WD_brightness (buffer_r, s->brightness);
  set_WD_threshold (buffer_r, s->threshold);
  set_WD_contrast (buffer_r, s->contrast);
  set_WD_bitsperpixel (buffer_r, s->bitsperpixel);
  set_WD_rif (buffer_r, s->rif);
  set_WD_pad (buffer_r, 0x3);
  set_WD_halftone (buffer_r, s->halftone);
  set_WD_bitorder (buffer_r, s->bitorder);
  set_WD_compress_type (buffer_r, s->compress_type);
  set_WD_compress_arg (buffer_r, s->compress_arg);
  set_WD_composition (buffer_r, s->composition);        /* may be overridden below */
  set_WD_vendor_id_code (buffer_r, 0xff);
  set_WD_adf (buffer_r, s->use_adf == SANE_TRUE ? 1 : 0);
  set_WD_source (buffer_r, 1);
  set_WD_highlight_color (buffer_r, 0xff);
  set_WD_shadow_value (buffer_r, 0x00);

  switch (s->composition)
    {
    case WD_comp_LA:
    case WD_comp_HT:
      set_WD_line_width (buffer_r, ((s->br_x - s->tl_x) * s->x_res / 1200) / 8);
      set_WD_line_count (buffer_r, (s->br_y - s->tl_y) * s->y_res / 1200);
      goto b_and_w;
    case WD_comp_G4:
      set_WD_line_width (buffer_r, ((s->br_x - s->tl_x) * s->x_res / 1200) / 2);
      set_WD_line_count (buffer_r, (s->br_y - s->tl_y) * s->y_res / 1200);
      goto grayscale;
    case WD_comp_G8:
      set_WD_line_width (buffer_r, (s->br_x - s->tl_x) * s->x_res / 1200);
      set_WD_line_count (buffer_r, (s->br_y - s->tl_y) * s->y_res / 1200);
    grayscale:
      set_WD_composition (buffer_r, WD_comp_GS);
    b_and_w:
      set_WD_color (buffer_r, WD_color_greenx);
      break;
    case WD_comp_MC:
      set_WD_color (buffer_r, WD_color_rgb);
      set_WD_line_width (buffer_r, 3 * (s->br_x - s->tl_x) * s->x_res / 1200);
      set_WD_line_count (buffer_r, (s->br_y - s->tl_y) * s->y_res / 1200);
      break;
    case WD_comp_BC:
    case WD_comp_DC:
    default:
      return SANE_STATUS_INVAL;
    }

  /* prepare SCSI-BUFFER */
  memcpy (s->buffer, set_windowB.cmd, set_windowB.size);
  memcpy ((s->buffer + set_windowB.size),
          window_parameter_data_blockB.cmd,
          window_parameter_data_blockB.size);
  memcpy ((s->buffer + set_windowB.size + window_parameter_data_blockB.size),
          buffer_r, window_descriptor_blockB.size);

  set_SW_xferlen (s->buffer,
                  (window_parameter_data_blockB.size
                   + WDB_size_Color));
  set_WDPB_wdblen ((s->buffer
                    + set_windowB.size),
                   WDB_size_Color);
  set_WD_parm_length ((s->buffer
                       + set_windowB.size
                       + window_parameter_data_blockB.size),
                      9);

  DBG (10, "\tx_res=%d, y_res=%d\n",
       s->x_res, s->y_res);
  DBG (10, "\tupper left-x=%d, upper left-y=%d\n",
       s->tl_x, s->tl_y);
  DBG (10, "\twindow width=%d, length=%d\n",
       s->br_x - s->tl_x,
       s->br_y - s->tl_y);

  active_buffer_size = set_windowB.size + get_SW_xferlen (s->buffer);

  hexdump (15, "Window set", s->buffer, active_buffer_size);

  ret = do_scsi_cmd (s->sfd, s->buffer, active_buffer_size, NULL, 0);
  if (ret)
    return ret;

  DBG (10, "set_window_param: ok\n");
  return ret;
}                               /* sp15c_set_window_param */

static size_t
max_string_size (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  int i;

  for (i = 0; strings[i]; ++i)
    {
      size = strlen (strings[i]) + 1;
      if (size > max_size)
        max_size = size;
    }
  return max_size;
}                               /* max_string_size */

static int
sp15c_start_scan (struct sp15c *s)
{
  int ret;
  DBG (10, "sp15c_start_scan\n");
  ret = do_scsi_cmd (s->sfd, scanB.cmd, scanB.size, NULL, 0);
  if (ret)
    return ret;
  DBG (10, "sp15c_start_scan:ok\n");
  return ret;
}                               /* sp15c_start_scan */

static void
sigterm_handler (int signal)
{
  signal = signal; /* silence compilation warnings */

  sanei_scsi_req_flush_all ();  /* flush SCSI queue */
  _exit (SANE_STATUS_GOOD);
}                               /* sigterm_handler */

/* This function is executed as a child process. */
static int
reader_process (void *data)
{
  struct sp15c *scanner = (struct sp15c *) data;
  int pipe_fd = scanner->reader_pipe;

  int status;
  unsigned int data_left;
  unsigned int data_to_read;
  FILE *fp;
  sigset_t ignore_set;
  sigset_t sigterm_set;
  struct SIGACTION act;
  unsigned int i;
  unsigned char *src, *dst;

  DBG (10, "reader_process started\n");

  if (sanei_thread_is_forked ()) 
    close (scanner->pipe);

  sigfillset (&ignore_set);
  sigdelset (&ignore_set, SIGTERM);
#if defined (__APPLE__) && defined (__MACH__)
  sigdelset (&ignore_set, SIGUSR2);
#endif
  sigprocmask (SIG_SETMASK, &ignore_set, 0);

  memset (&act, 0, sizeof (act));
  sigaction (SIGTERM, &act, 0);

  sigemptyset (&sigterm_set);
  sigaddset (&sigterm_set, SIGTERM);

  fp = fdopen (pipe_fd, "w");
  if (!fp)
    {
      DBG (1, "reader_process: couldn't open pipe!\n");
      return 1;
    }

  DBG (10, "reader_process: starting to READ data\n");

  data_left = bytes_per_line (scanner) *
    lines_per_scan (scanner);

  sp15c_trim_rowbufsize (scanner);      /* trim bufsize */

  DBG (10, "reader_process: reading %u bytes in blocks of %u bytes\n",
       data_left, scanner->row_bufsize);

  memset (&act, 0, sizeof (act));

  act.sa_handler = sigterm_handler;
  sigaction (SIGTERM, &act, 0);
  /* wait_scanner(scanner); */
  do
    {
      data_to_read = (data_left < scanner->row_bufsize)
        ? data_left
        : scanner->row_bufsize;

      if (scanner->composition == WD_comp_G4)
        {
          data_to_read /= 2;    /* 'cause each byte holds two pixels */
        }
      status = sp15c_read_data_block (scanner, data_to_read);
      if (status == 0)
        {
          DBG (1, "reader_process: no data yet\n");
          fflush (stdout);
          fflush (stderr);
          usleep (50000);
          continue;
        }
      if (status == -1)
        {
          DBG (1, "reader_process: unable to get image data from scanner!\n");
          fflush (stdout);
          fflush (stderr);
          fclose (fp);
          return (-1);
        }

      /* expand 4-bit pixels to 8-bit bytes */
      if (scanner->composition == WD_comp_G4)
        {
          src = &scanner->buffer[data_to_read - 1];
          dst = &scanner->buffer[data_to_read * 2 - 1];
          for (i = 0; i < data_to_read; i++)
            {
              *dst-- = ((*src << 4) & 0xf0) + ((*src)        & 0x0f);
              *dst-- = ((*src)      & 0xf0) + ((*src >> 4) & 0x0f);
	      --src;
            }
          data_to_read *= 2;
        }

      fwrite (scanner->buffer, 1, data_to_read, fp);
      fflush (fp);

      data_left -= data_to_read;
      DBG (10, "reader_process: buffer of %d bytes read; %d bytes to go\n",
           data_to_read, data_left);
      fflush (stdout);
      fflush (stderr);
    }
  while (data_left);

  fclose (fp);

  DBG (10, "reader_process: finished\n");

  return 0;
}                               /* reader_process */

static SANE_Status
do_eof (struct sp15c *scanner)
{
  DBG (10, "do_eof\n");

  scanner->scanning = SANE_FALSE;
  if (scanner->pipe >= 0)
    {
      close (scanner->pipe);
      scanner->pipe = -1;
    }
  return SANE_STATUS_EOF;
}                               /* do_eof */

static int
pixels_per_line (struct sp15c *s)
{
  int pixels;
  pixels = s->x_res * (s->br_x - s->tl_x) / 1200;
  return pixels;
}                               /* pixels_per_line */

static int
lines_per_scan (struct sp15c *s)
{
  int lines;
  lines = s->y_res * (s->br_y - s->tl_y) / 1200;
  return lines;
}                               /* lines_per_scan */

static int
bytes_per_line (struct sp15c *s)
{
  int bytes;
  /* SEE PAGE 29 OF SANE SPEC!!! */
  if (s->bitsperpixel == 1)
    {
      bytes = (pixels_per_line (s) + 7) / 8;
    }
  else
    {
      bytes = pixels_per_line (s);
    }
  if (s->composition == WD_comp_MC)
    {
      bytes *= 3;
    }
  return bytes;
}                               /* bytes_per_line */

static void
sp15c_trim_rowbufsize (struct sp15c *s)
{
  unsigned int row_len;
  row_len = bytes_per_line (s);
  if (s->row_bufsize >= row_len)
    {
      s->row_bufsize = s->row_bufsize - (s->row_bufsize % row_len);
      DBG (10, "trim_rowbufsize to %d (%d lines)\n",
           s->row_bufsize, s->row_bufsize / row_len);
    }
}                               /* sp15c_trim_rowbufsize */

static int
sp15c_read_data_block (struct sp15c *s, unsigned int length)
{
  int r;

  DBG (10, "sp15c_read_data_block (length = %d)\n", length);
  /*wait_scanner(s); */

  set_R_datatype_code (readB.cmd, R_datatype_imagedata);
  set_R_xfer_length (readB.cmd, length);

  r = do_scsi_cmd (s->sfd, readB.cmd, readB.size, s->buffer, length);
#if 0
  return ((r != 0) ? -1 : length);
#else
  if (r)
    {
      return -1;
    }
  else
    {
      return length;
    }
#endif
}                               /* sp15c_read_data_block */

/*  Increase initial width by increasing bottom right X
 *  until we have a HAPPY number of bytes in line.
 *  Should be called whenever s->composition, s->x_res,
 *  s->br_x, or s->tl_x are changed */
static void
adjust_width (struct sp15c *s, SANE_Int * info)
{
  int pixels;
  int changed = 0;
  if (s->composition == WD_comp_MC)
    {
      while (pixels = s->x_res * (s->br_x - s->tl_x) / 1200,
             (s->bitsperpixel * pixels) % (8 * 4))
        {
          s->br_x--;
          changed++;
        }
    }
  else
    {
      while (pixels = s->x_res * (s->br_x - s->tl_x) / 1200,
             (s->bitsperpixel * pixels) % 8)
        {
          s->br_x--;
          changed++;
        }
    }
  if (changed && info)
    *info |= SANE_INFO_INEXACT;

  return;
}                               /* adjust_width */

static SANE_Status
apply_constraints (struct sp15c *scanner, SANE_Int opt,
                   SANE_Int * target, SANE_Word * info)
{
  SANE_Status status;
  status = sanei_constrain_value (scanner->opt + opt, target, info);
  if (status != SANE_STATUS_GOOD)
    {
      if (SANE_CONSTRAINT_RANGE ==
          scanner->opt[opt].constraint_type)
        {
          if (*target < scanner->opt[opt].constraint.range->min)
            {
              *target = scanner->opt[opt].constraint.range->min;
              return SANE_STATUS_GOOD;
            }
          else if (scanner->opt[opt].constraint.range->max < *target)
            {
              *target = scanner->opt[opt].constraint.range->max;
              return SANE_STATUS_GOOD;
            }
        }
      return status;
    }
  else
    {
      return SANE_STATUS_GOOD;
    }
}                               /* apply_constraints */

/******************************************************************************
}#############################################################################
******************************************************************************/
