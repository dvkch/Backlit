/* sane - Scanner Access Now Easy.
   Copyright (C) 2007 Jeremy Johnson
   This file is part of a SANE backend for Ricoh IS450 
   and IS420 family of HS2P Scanners using the SCSI controller.

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
/* SANE-FLOW-DIAGRAM

   - sane_init() : initialize backend, attach scanners
   . - sane_get_devices() : query list of scanner-devices
   . - sane_open() : open a particular scanner-device
   . . - attach : to the device
   . . . init_options : initialize SANE_OPTIONS array
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
#define BUILD 1

/* Begin includes */
#include "../include/sane/config.h"

#include <limits.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#include "../include/sane/sane.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_scsi.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_thread.h"

#define BACKEND_NAME hs2p
#include "../include/sane/sanei_backend.h"

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

#include "hs2p-scsi.c"

/* Begin macros */
#define MIN(x,y) ((x)<(y) ? (x) : (y))
#define MAX(x,y) ((x)>(y) ? (x) : (y))

/* Begin static constants */
static int num_devices = 0;
static HS2P_Device *first_dev = NULL;
static HS2P_Scanner *first_handle = NULL;

static SANE_Char inquiry_data[255] = "HS2P scanner";
/*
static SANE_Int disable_optional_frames = 0;
static SANE_Int fake_inquiry = 0;
*/

static HS2P_HWEntry HS2P_Device_List[] = {
  {"RICOH", "IS450"},
  {"RICOH", "IS430"},		/*untested */
  {"RICOH", "IS420"},		/*untested */
  {"RICOH", "IS01"},		/*untested */
  {"RICOH", "IS02"},		/*untested */
  {NULL, NULL}			/*sentinel */
};

#if 0
static int
allblank (const char *s)
{
  while (s && *s)
    if (!isspace (*s++))
      return 0;

  return 1;
}
#endif

static size_t
max_string_size (SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  int i;
  DBG (DBG_proc, ">> max_string_size\n");

  for (i = 0; strings[i]; ++i)
    {
      size = strlen (strings[i]) + 1;
      if (size > max_size)
	max_size = size;
    }

  DBG (DBG_proc, "<< max_string_size\n");
  return max_size;
}

static void
trim_spaces (char *s, size_t n)
{
  for (s += (n - 1); n > 0; n--, s--)
    {
      if (*s && !isspace (*s))
	break;
      *s = '\0';
    }
}
static SANE_Bool
is_device_supported (char *device)
{
  HS2P_HWEntry *hw;

  for (hw = &HS2P_Device_List[0]; hw->mfg != NULL; hw++)
    if (strncmp (device, hw->model, strlen (hw->model)) == 0)
      break;			/* found a match */

  return (hw == NULL) ? SANE_FALSE : SANE_TRUE;
}

static SANE_Int
get_list_index (const char *list[], char *s)	/* sequential search */
{
  SANE_Int i;

  for (i = 0; list[i]; i++)
    if (strcmp (s, list[i]) == 0)
      return i;			/* FOUND */

  /* unknown paper_list strings are treated as 'custom'     */
  /* unknown compression_list strings are treated as 'none' */
  /* unknown scan_source_list strings are treated as 'ADF'  */
  return 0;
}

static SANE_Int
get_val_id_strndx (struct val_id *vi, int len, SANE_Int val)
{
  int i;
  for (i = 0; i < len; i++)
    if (vi[i].val == val)
      return vi[i].id;		/* FOUND */
  return vi[0].id;		/* NOT FOUND so let's default to first */
}

static SANE_Status
init_options (HS2P_Scanner * s)
{
  SANE_Int i;
  DBG (DBG_proc, ">> init_options\n");

  memset (s->opt, 0, sizeof (s->opt));
  memset (s->val, 0, sizeof (s->val));

  for (i = 0; i < NUM_OPTIONS; ++i)
    {
      s->opt[i].size = sizeof (SANE_Word);
      s->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }

  s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  s->val[OPT_NUM_OPTS].w = NUM_OPTIONS;


  /* 
   * "Scan Mode" GROUP: 
   */
  s->opt[OPT_MODE_GROUP].name = "";
  s->opt[OPT_MODE_GROUP].title = SANE_TITLE_SCAN_MODE_GROUP;
  s->opt[OPT_MODE_GROUP].desc = "";
  s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MODE_GROUP].cap = 0;
  s->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* Preview: */
  s->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  s->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  s->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  s->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;
  s->opt[OPT_PREVIEW].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_PREVIEW].w = SANE_FALSE;

  /* Inquiry */
  s->opt[OPT_INQUIRY].name = SANE_NAME_INQUIRY;
  s->opt[OPT_INQUIRY].title = SANE_TITLE_INQUIRY;
  s->opt[OPT_INQUIRY].desc = SANE_DESC_INQUIRY;
  s->opt[OPT_INQUIRY].type = SANE_TYPE_STRING;
  s->opt[OPT_INQUIRY].size = sizeof (inquiry_data);
  s->opt[OPT_INQUIRY].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_INQUIRY].s = strdup (inquiry_data);
  s->opt[OPT_INQUIRY].cap = SANE_CAP_SOFT_DETECT;	/* Display Only */

  /* Scan mode */
  s->opt[OPT_SCAN_MODE].name = SANE_NAME_SCAN_MODE;
  s->opt[OPT_SCAN_MODE].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_SCAN_MODE].desc = SANE_DESC_SCAN_MODE;
  s->opt[OPT_SCAN_MODE].type = SANE_TYPE_STRING;
  s->opt[OPT_SCAN_MODE].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_SCAN_MODE].size =
    max_string_size ((SANE_String_Const *) scan_mode_list);
  s->opt[OPT_SCAN_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_SCAN_MODE].constraint.string_list =
    (SANE_String_Const *) & scan_mode_list[0];
  s->val[OPT_SCAN_MODE].s = strdup (scan_mode_list[0]);
  s->image_composition = LINEART;

  /* Standard resolutions */
  s->opt[OPT_RESOLUTION].name = "std-" SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].title = "Std-" SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].desc = "Std " SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->opt[OPT_RESOLUTION].constraint.word_list = s->hw->info.resStdList;
  s->val[OPT_RESOLUTION].w = s->hw->info.default_res;

  /* X Resolution */
  s->opt[OPT_X_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_X_RESOLUTION].title = SANE_TITLE_SCAN_X_RESOLUTION;
  s->opt[OPT_X_RESOLUTION].desc = "X " SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_X_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_X_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_X_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_X_RESOLUTION].constraint.range = &(s->hw->info.xres_range);
  s->val[OPT_X_RESOLUTION].w = s->hw->info.resBasicX;

  /* Y Resolution */
  s->opt[OPT_Y_RESOLUTION].name = SANE_NAME_SCAN_Y_RESOLUTION;
  s->opt[OPT_Y_RESOLUTION].title = SANE_TITLE_SCAN_Y_RESOLUTION;
  s->opt[OPT_Y_RESOLUTION].desc = "Y " SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_Y_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_Y_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_Y_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_Y_RESOLUTION].constraint.range = &(s->hw->info.yres_range);
  s->val[OPT_Y_RESOLUTION].w = s->hw->info.resBasicY;

  /* Compression */
  s->opt[OPT_COMPRESSION].name = SANE_NAME_COMPRESSION;
  s->opt[OPT_COMPRESSION].title = SANE_TITLE_COMPRESSION;
  s->opt[OPT_COMPRESSION].desc = SANE_DESC_COMPRESSION;
  s->opt[OPT_COMPRESSION].type = SANE_TYPE_STRING;
  s->opt[OPT_COMPRESSION].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_COMPRESSION].size =
    max_string_size ((SANE_String_Const *) compression_list);
  s->opt[OPT_COMPRESSION].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_COMPRESSION].constraint.string_list =
    (SANE_String_Const *) & compression_list[0];
  s->val[OPT_COMPRESSION].s = strdup (compression_list[0]);
  if (s->hw->info.supports_MH == SANE_FALSE ||	/* MH  G3 1-D       */
      s->hw->info.supports_MR == SANE_FALSE ||	/* MR  G3 2-D       */
      s->hw->info.supports_MMR == SANE_FALSE ||	/* MMR G4 2-D       */
      s->hw->info.supports_MHB == SANE_FALSE)	/* MH byte boundary */
    {
      s->opt[OPT_COMPRESSION].cap |= SANE_CAP_INACTIVE;
    }



  /* 
   * "Geometry" GROUP: 
   */
  s->opt[OPT_GEOMETRY_GROUP].name = "";
  s->opt[OPT_GEOMETRY_GROUP].title = SANE_TITLE_GEOMETRY_GROUP;
  s->opt[OPT_GEOMETRY_GROUP].desc = "";
  s->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_GEOMETRY_GROUP].cap = 0;
  s->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* Auto Size Recognition available if IPU installed */
  s->opt[OPT_AUTO_SIZE].name = SANE_NAME_AUTO_SIZE;
  s->opt[OPT_AUTO_SIZE].title = SANE_TITLE_AUTO_SIZE;
  s->opt[OPT_AUTO_SIZE].desc = SANE_DESC_AUTO_SIZE;
  s->opt[OPT_AUTO_SIZE].type = SANE_TYPE_BOOL;
  s->opt[OPT_AUTO_SIZE].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_AUTO_SIZE].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_AUTO_SIZE].w = SANE_FALSE;
  if (!s->hw->info.supports_sizerecognition)
    s->opt[OPT_AUTO_SIZE].cap |= SANE_CAP_INACTIVE;

  /* Pad short documents to requested length with white space */
  s->opt[OPT_PADDING].name = SANE_NAME_PADDING;
  s->opt[OPT_PADDING].title = SANE_TITLE_PADDING;
  s->opt[OPT_PADDING].desc = SANE_DESC_PADDING;
  s->opt[OPT_PADDING].type = SANE_TYPE_BOOL;
  s->opt[OPT_PADDING].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_PADDING].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_PADDING].w = SANE_TRUE;
  /*if (!s->hw->info.hasADF)
     s->opt[OPT_PADDING].cap |= SANE_CAP_INACTIVE;
     FIXME: compare to user setting, not the existence of FB?
     if (!strcmp (scan_source_list, "FB"))
     s->opt[OPT_PADDING].cap |= SANE_CAP_INACTIVE; */
  /* Permanently disable OPT_PADDING */
  s->opt[OPT_PADDING].cap |= SANE_CAP_INACTIVE;

  /* Paper Orientation */
  s->opt[OPT_PAGE_ORIENTATION].name = SANE_NAME_ORIENTATION;
  s->opt[OPT_PAGE_ORIENTATION].title = SANE_TITLE_ORIENTATION;
  s->opt[OPT_PAGE_ORIENTATION].desc = SANE_DESC_ORIENTATION;
  s->opt[OPT_PAGE_ORIENTATION].type = SANE_TYPE_STRING;
  s->opt[OPT_PAGE_ORIENTATION].size = max_string_size (orientation_list);
  s->opt[OPT_PAGE_ORIENTATION].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_PAGE_ORIENTATION].constraint.string_list = &orientation_list[0];
  s->val[OPT_PAGE_ORIENTATION].s = strdup (orientation_list[0]);

  /* Paper Size */
  s->opt[OPT_PAPER_SIZE].name = SANE_NAME_PAPER_SIZE;
  s->opt[OPT_PAPER_SIZE].title = SANE_TITLE_PAPER_SIZE;
  s->opt[OPT_PAPER_SIZE].desc = SANE_DESC_PAPER_SIZE;
  s->opt[OPT_PAPER_SIZE].type = SANE_TYPE_STRING;
  s->opt[OPT_PAPER_SIZE].size = max_string_size (paper_list);
  s->opt[OPT_PAPER_SIZE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_PAPER_SIZE].constraint.string_list = &paper_list[0];
  s->val[OPT_PAPER_SIZE].s = strdup (paper_list[0]);

  /* top-left x */
  s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  s->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_X].constraint.range = &(s->hw->info.x_range);
  s->val[OPT_TL_X].w = SANE_FIX (0.0);

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &(s->hw->info.y_range);
  s->val[OPT_TL_Y].w = SANE_FIX (0.0);

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &(s->hw->info.x_range);
  s->val[OPT_BR_X].w = s->hw->info.x_range.max;

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &(s->hw->info.y_range);
  s->val[OPT_BR_Y].w = s->hw->info.y_range.max;

  DBG (DBG_info, "INIT_OPTIONS: ul(x,y) = (%d,%d)    br(x,y) = (%d,%d)\n",
       (unsigned) SANE_UNFIX (s->val[OPT_TL_X].w),
       (unsigned) SANE_UNFIX (s->val[OPT_TL_Y].w),
       (unsigned) SANE_UNFIX (s->val[OPT_BR_X].w),
       (unsigned) SANE_UNFIX (s->val[OPT_BR_Y].w));
  /* Autoborder */
  /* Rotation   */
  /* Deskew     */



  /* 
   * "Feeder" GROUP: 
   */
  s->opt[OPT_FEEDER_GROUP].name = "";
  s->opt[OPT_FEEDER_GROUP].title = SANE_TITLE_FEEDER_GROUP;
  s->opt[OPT_FEEDER_GROUP].desc = "";
  s->opt[OPT_FEEDER_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_FEEDER_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_FEEDER_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* Scan Source */
  s->opt[OPT_SCAN_SOURCE].name = SANE_NAME_SCAN_SOURCE;
  s->opt[OPT_SCAN_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
  s->opt[OPT_SCAN_SOURCE].desc = SANE_DESC_SCAN_SOURCE;
  s->opt[OPT_SCAN_SOURCE].type = SANE_TYPE_STRING;
  s->opt[OPT_SCAN_SOURCE].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_SCAN_SOURCE].size = max_string_size (scan_source_list);
  s->opt[OPT_SCAN_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_SCAN_SOURCE].constraint.string_list =
    (SANE_String_Const *) & scan_source_list[0];
  s->val[OPT_SCAN_SOURCE].s = strdup (scan_source_list[0]);
  if (!s->hw->info.hasADF)
    s->opt[OPT_SCAN_SOURCE].cap |= SANE_CAP_INACTIVE;

  /* Duplex: */
  s->opt[OPT_DUPLEX].name = SANE_NAME_DUPLEX;
  s->opt[OPT_DUPLEX].title = SANE_TITLE_DUPLEX;
  s->opt[OPT_DUPLEX].desc = SANE_DESC_DUPLEX;
  s->opt[OPT_DUPLEX].type = SANE_TYPE_BOOL;
  s->opt[OPT_DUPLEX].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_DUPLEX].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_DUPLEX].w = s->hw->info.default_duplex;
  if (!s->hw->info.hasDuplex)
    s->opt[OPT_DUPLEX].cap |= SANE_CAP_INACTIVE;

  /* Prefeed: */
  s->opt[OPT_PREFEED].name = SANE_NAME_PREFEED;
  s->opt[OPT_PREFEED].title = SANE_TITLE_PREFEED;
  s->opt[OPT_PREFEED].desc = SANE_DESC_PREFEED;
  s->opt[OPT_PREFEED].type = SANE_TYPE_BOOL;
  s->opt[OPT_PREFEED].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_PREFEED].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_PREFEED].w = SANE_FALSE;
  s->opt[OPT_PREFEED].cap |= SANE_CAP_INACTIVE;

  /* Endorser: */
  s->opt[OPT_ENDORSER].name = SANE_NAME_ENDORSER;
  s->opt[OPT_ENDORSER].title = SANE_TITLE_ENDORSER;
  s->opt[OPT_ENDORSER].desc = SANE_DESC_ENDORSER;
  s->opt[OPT_ENDORSER].type = SANE_TYPE_BOOL;
  s->opt[OPT_ENDORSER].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_ENDORSER].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_ENDORSER].w = s->hw->info.endorser_control;
  if (!s->hw->info.hasEndorser)
    s->opt[OPT_ENDORSER].cap |= SANE_CAP_INACTIVE;

  /* Endorser String: */
  s->opt[OPT_ENDORSER_STRING].name = SANE_NAME_ENDORSER_STRING;
  s->opt[OPT_ENDORSER_STRING].title = SANE_TITLE_ENDORSER_STRING;
  s->opt[OPT_ENDORSER_STRING].desc = SANE_DESC_ENDORSER_STRING;
  s->opt[OPT_ENDORSER_STRING].type = SANE_TYPE_STRING;
  s->opt[OPT_ENDORSER_STRING].cap =
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_ENDORSER_STRING].size = sizeof (s->hw->info.endorser_string);
  s->opt[OPT_ENDORSER_STRING].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_ENDORSER_STRING].s = strdup (s->hw->info.endorser_string);
  if (!s->hw->info.hasEndorser)
    s->opt[OPT_ENDORSER_STRING].cap |= SANE_CAP_INACTIVE;

  /* Batch     */
  /* Check ADF */
  /* timeout ADF */
  /* timeout Manual */

  /* 
   * "Enhancement" GROUP: 
   */
  s->opt[OPT_ENHANCEMENT_GROUP].name = "";
  s->opt[OPT_ENHANCEMENT_GROUP].title = SANE_TITLE_ENHANCEMENT_GROUP;
  s->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  s->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ENHANCEMENT_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* Halftone Type */
  s->opt[OPT_HALFTONE_CODE].name = SANE_NAME_HALFTONE_CODE;
  s->opt[OPT_HALFTONE_CODE].title = SANE_TITLE_HALFTONE_CODE;
  s->opt[OPT_HALFTONE_CODE].desc = SANE_DESC_HALFTONE_CODE;
  s->opt[OPT_HALFTONE_CODE].type = SANE_TYPE_STRING;
  s->opt[OPT_HALFTONE_CODE].size = max_string_size (halftone_code);
  s->opt[OPT_HALFTONE_CODE].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_HALFTONE_CODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_HALFTONE_CODE].constraint.string_list =
    (SANE_String_Const *) & halftone_code[0];
  s->val[OPT_HALFTONE_CODE].s = strdup (halftone_code[0]);
  if (s->image_composition == LINEART)
    s->opt[OPT_HALFTONE_CODE].cap |= SANE_CAP_INACTIVE;

  /* Halftone patterns */
  s->opt[OPT_HALFTONE_PATTERN].name = SANE_NAME_HALFTONE_PATTERN;
  s->opt[OPT_HALFTONE_PATTERN].title = SANE_TITLE_HALFTONE_PATTERN;
  s->opt[OPT_HALFTONE_PATTERN].desc = SANE_DESC_HALFTONE_PATTERN;
  s->opt[OPT_HALFTONE_PATTERN].type = SANE_TYPE_STRING;
  s->opt[OPT_HALFTONE_PATTERN].size =
    max_string_size ((SANE_String_Const *) halftone_pattern_list);
  s->opt[OPT_HALFTONE_PATTERN].cap =
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_HALFTONE_PATTERN].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_HALFTONE_PATTERN].constraint.string_list =
    (SANE_String_Const *) & halftone_pattern_list[0];
  s->val[OPT_HALFTONE_PATTERN].s = strdup (halftone_pattern_list[0]);
  if (s->image_composition == LINEART)
    s->opt[OPT_HALFTONE_CODE].cap |= SANE_CAP_INACTIVE;

  /* Gray Filter */
  s->opt[OPT_GRAYFILTER].name = SANE_NAME_GRAYFILTER;
  s->opt[OPT_GRAYFILTER].title = SANE_TITLE_GRAYFILTER;
  s->opt[OPT_GRAYFILTER].desc = SANE_DESC_GRAYFILTER;
  s->opt[OPT_GRAYFILTER].type = SANE_TYPE_STRING;
  s->opt[OPT_GRAYFILTER].size = max_string_size (grayfilter_list);
  s->opt[OPT_GRAYFILTER].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_GRAYFILTER].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_GRAYFILTER].constraint.string_list =
    (SANE_String_Const *) & grayfilter_list[0];
  s->val[OPT_GRAYFILTER].s = strdup (grayfilter_list[0]);

  /* Scan Wait Mode */
  s->opt[OPT_SCAN_WAIT_MODE].name = SANE_NAME_SCAN_WAIT_MODE;
  s->opt[OPT_SCAN_WAIT_MODE].title = SANE_TITLE_SCAN_WAIT_MODE;
  s->opt[OPT_SCAN_WAIT_MODE].desc = SANE_DESC_SCAN_WAIT_MODE;
  s->opt[OPT_SCAN_WAIT_MODE].type = SANE_TYPE_BOOL;
  s->opt[OPT_SCAN_WAIT_MODE].unit = SANE_UNIT_NONE;
  s->val[OPT_SCAN_WAIT_MODE].w =
    (s->hw->info.scan_wait_mode) ? SANE_TRUE : SANE_FALSE;

  /* Brightness */
  s->opt[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].type = SANE_TYPE_INT;
  s->opt[OPT_BRIGHTNESS].unit = SANE_UNIT_NONE;
  s->opt[OPT_BRIGHTNESS].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BRIGHTNESS].constraint.range = &s->hw->info.brightness_range;
  s->val[OPT_BRIGHTNESS].w = 128;

  /* Threshold */
  s->opt[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
  s->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
  s->opt[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
  s->opt[OPT_THRESHOLD].type = SANE_TYPE_INT;
  s->opt[OPT_THRESHOLD].unit = SANE_UNIT_NONE;
  s->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_THRESHOLD].constraint.range = &s->hw->info.threshold_range;
  s->val[OPT_THRESHOLD].w = 128;

  /* Contrast */
  s->opt[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  s->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  s->opt[OPT_CONTRAST].desc = SANE_DESC_CONTRAST;
  s->opt[OPT_CONTRAST].type = SANE_TYPE_INT;
  s->opt[OPT_CONTRAST].unit = SANE_UNIT_NONE;
  s->opt[OPT_CONTRAST].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CONTRAST].constraint.range = &s->hw->info.contrast_range;
  s->val[OPT_CONTRAST].w = 128;

  /* Gamma */
  s->opt[OPT_GAMMA].name = SANE_NAME_GAMMA;
  s->opt[OPT_GAMMA].title = SANE_TITLE_GAMMA;
  s->opt[OPT_GAMMA].desc = SANE_DESC_GAMMA;
  s->opt[OPT_GAMMA].type = SANE_TYPE_STRING;
  s->opt[OPT_GAMMA].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_GAMMA].size = max_string_size ((SANE_String_Const *) gamma_list);
  /*
     s->opt[OPT_GAMMA].type = SANE_TYPE_INT;
     s->opt[OPT_GAMMA].unit = SANE_UNIT_NONE;
     s->opt[OPT_GAMMA].constraint_type = SANE_CONSTRAINT_RANGE;
     s->opt[OPT_GAMMA].constraint.range = &u8_range;
     s->val[OPT_GAMMA].w = 0;
   */
  s->opt[OPT_GAMMA].type = SANE_TYPE_STRING;
  s->opt[OPT_GAMMA].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_GAMMA].constraint.string_list =
    (SANE_String_Const *) & gamma_list[0];
  s->val[OPT_GAMMA].s = strdup (gamma_list[0]);

  /* custom-gamma table */
  s->opt[OPT_CUSTOM_GAMMA].name = SANE_NAME_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].title = SANE_TITLE_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].desc = SANE_DESC_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].type = SANE_TYPE_BOOL;
  s->opt[OPT_CUSTOM_GAMMA].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->val[OPT_CUSTOM_GAMMA].w = SANE_FALSE;
  s->opt[OPT_CUSTOM_GAMMA].cap |= SANE_CAP_INACTIVE;

  /* grayscale gamma vector */
  s->opt[OPT_GAMMA_VECTOR_GRAY].name = SANE_NAME_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR_GRAY].title = SANE_TITLE_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR_GRAY].desc = SANE_DESC_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR_GRAY].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR_GRAY].cap |=
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_GAMMA_VECTOR_GRAY].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_GRAY].size = GAMMA_LENGTH * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR_GRAY].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_GRAY].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR_GRAY].wa = s->gamma_table;
  s->opt[OPT_GAMMA_VECTOR_GRAY].cap |= SANE_CAP_INACTIVE;


  /* Control Panel */
  /* ACE Function  */
  /* ACE Sensitivity */

  /* Binary Smoothing Filter */
  s->opt[OPT_SMOOTHING].name = SANE_NAME_SMOOTHING;
  s->opt[OPT_SMOOTHING].title = SANE_TITLE_SMOOTHING;
  s->opt[OPT_SMOOTHING].desc = SANE_DESC_SMOOTHING;
  s->opt[OPT_SMOOTHING].type = SANE_TYPE_BOOL;
  s->opt[OPT_SMOOTHING].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_SMOOTHING].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_SMOOTHING].w = SANE_FALSE;
  if (!s->hw->info.hasIPU)
    s->opt[OPT_SMOOTHING].cap |= SANE_CAP_INACTIVE;

  /* Binary Noise Removal Filter */
  s->opt[OPT_NOISEREMOVAL].name = SANE_NAME_NOISEREMOVAL;
  s->opt[OPT_NOISEREMOVAL].title = SANE_TITLE_NOISEREMOVAL;
  s->opt[OPT_NOISEREMOVAL].desc = SANE_DESC_NOISEREMOVAL;
  s->opt[OPT_NOISEREMOVAL].type = SANE_TYPE_STRING;
  s->opt[OPT_NOISEREMOVAL].size = max_string_size (noisematrix_list);
  s->opt[OPT_NOISEREMOVAL].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_NOISEREMOVAL].constraint.string_list =
    (SANE_String_Const *) & noisematrix_list[0];
  s->val[OPT_NOISEREMOVAL].s = strdup (noisematrix_list[0]);
  if (!s->hw->info.hasIPU)
    s->opt[OPT_NOISEREMOVAL].cap |= SANE_CAP_INACTIVE;

  /* Automatic Separation */
  s->opt[OPT_AUTOSEP].name = SANE_NAME_AUTOSEP;
  s->opt[OPT_AUTOSEP].title = SANE_TITLE_AUTOSEP;
  s->opt[OPT_AUTOSEP].desc = SANE_DESC_AUTOSEP;
  s->opt[OPT_AUTOSEP].type = SANE_TYPE_STRING;
  s->opt[OPT_AUTOSEP].size = max_string_size (auto_separation_list);
  s->opt[OPT_AUTOSEP].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_AUTOSEP].constraint.string_list =
    (SANE_String_Const *) & auto_separation_list[0];
  s->val[OPT_AUTOSEP].s = strdup (auto_separation_list[0]);
  if (!s->hw->info.hasIPU)
    s->opt[OPT_AUTOSEP].cap |= SANE_CAP_INACTIVE;

  /* Automatic Binarization */
  s->opt[OPT_AUTOBIN].name = SANE_NAME_AUTOBIN;
  s->opt[OPT_AUTOBIN].title = SANE_TITLE_AUTOBIN;
  s->opt[OPT_AUTOBIN].desc = SANE_DESC_AUTOBIN;
  s->opt[OPT_AUTOBIN].type = SANE_TYPE_STRING;
  s->opt[OPT_AUTOBIN].size = max_string_size (auto_binarization_list);
  s->opt[OPT_AUTOBIN].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_AUTOBIN].constraint.string_list =
    (SANE_String_Const *) & auto_binarization_list[0];
  s->val[OPT_AUTOBIN].s = strdup (auto_binarization_list[0]);
  if (!s->hw->info.hasIPU)
    s->opt[OPT_AUTOBIN].cap |= SANE_CAP_INACTIVE;

  /* SECTION 
   * The IS450 supports up to 4 Section; The IS420 supports up to 6 Sections
   * For each struct window_section[i] we need to fill in ulx,uly,width,height,etc
   * NOT YET IMPLEMENTED
   */

  /* Negative */
  s->opt[OPT_NEGATIVE].name = SANE_NAME_NEGATIVE;
  s->opt[OPT_NEGATIVE].title = SANE_TITLE_NEGATIVE;
  s->opt[OPT_NEGATIVE].desc = SANE_DESC_NEGATIVE;
  s->opt[OPT_NEGATIVE].type = SANE_TYPE_BOOL;
  s->opt[OPT_NEGATIVE].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_NEGATIVE].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_NEGATIVE].w = SANE_FALSE;

  /* White Balance */
  s->opt[OPT_WHITE_BALANCE].name = SANE_NAME_WHITE_BALANCE;
  s->opt[OPT_WHITE_BALANCE].title = SANE_TITLE_WHITE_BALANCE;
  s->opt[OPT_WHITE_BALANCE].desc = SANE_DESC_WHITE_BALANCE;
  s->opt[OPT_WHITE_BALANCE].type = SANE_TYPE_BOOL;
  s->opt[OPT_WHITE_BALANCE].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_WHITE_BALANCE].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_WHITE_BALANCE].w = SANE_FALSE;	/* F/T = Relative/Absolute White */

  /* 
   * "Miscellaneous" GROUP: 
   */
  s->opt[OPT_MISCELLANEOUS_GROUP].name = "";
  s->opt[OPT_MISCELLANEOUS_GROUP].title = SANE_TITLE_MISCELLANEOUS_GROUP;
  s->opt[OPT_MISCELLANEOUS_GROUP].desc = "";
  s->opt[OPT_MISCELLANEOUS_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MISCELLANEOUS_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_MISCELLANEOUS_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* Padding Type: */
  s->opt[OPT_PADDING_TYPE].name = SANE_NAME_PADDING_TYPE;
  s->opt[OPT_PADDING_TYPE].title = SANE_TITLE_PADDING_TYPE;
  s->opt[OPT_PADDING_TYPE].desc = SANE_DESC_PADDING_TYPE;
  s->opt[OPT_PADDING_TYPE].type = SANE_TYPE_STRING;
  s->opt[OPT_PADDING_TYPE].cap = SANE_CAP_SOFT_DETECT;	/* Display only */
  s->opt[OPT_PADDING_TYPE].size = max_string_size (paddingtype_list);
  /*
     s->opt[OPT_PADDING_TYPE].size = sizeof((paddingtype_list[ get_paddingtype_strndx(TRUNCATE) ]));
     s->opt[OPT_PADDING_TYPE].constraint_type = SANE_CONSTRAINT_NONE;
   */
  s->opt[OPT_PADDING_TYPE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_PADDING_TYPE].constraint.string_list =
    (SANE_String_Const *) & paddingtype_list[0];
  s->val[OPT_PADDING_TYPE].s =
    strdup (paddingtype_list[get_paddingtype_strndx (TRUNCATE)]);
  DBG (DBG_info, "PADDINGTYPE =%s size=%d\n", s->val[OPT_PADDING_TYPE].s,
       s->opt[OPT_PADDING_TYPE].size);

  /* Bit Order
     s->opt[OPT_BITORDER].name = SANE_NAME_BITORDER;
     s->opt[OPT_BITORDER].title = SANE_TITLE_BITORDER;
     s->opt[OPT_BITORDER].desc = SANE_DESC_BITORDER;
     s->opt[OPT_BITORDER].type = SANE_TYPE_WORD;
     s->opt[OPT_BITORDER].cap = SANE_CAP_SOFT_DETECT;
     s->opt[OPT_BITORDER].constraint_type = SANE_CONSTRAINT_NONE
     s->val[OPT_BITORDER].w =  0x7;
   */

  /* Self Diagnostics */
  s->opt[OPT_SELF_DIAGNOSTICS].name = SANE_NAME_SELF_DIAGNOSTICS;
  s->opt[OPT_SELF_DIAGNOSTICS].title = SANE_TITLE_SELF_DIAGNOSTICS;
  s->opt[OPT_SELF_DIAGNOSTICS].desc = SANE_DESC_SELF_DIAGNOSTICS;
  s->opt[OPT_SELF_DIAGNOSTICS].type = SANE_TYPE_BUTTON;

  /* Optical Diagnostics */
  s->opt[OPT_OPTICAL_ADJUSTMENT].name = SANE_NAME_OPTICAL_ADJUSTMENT;
  s->opt[OPT_OPTICAL_ADJUSTMENT].title = SANE_TITLE_OPTICAL_ADJUSTMENT;
  s->opt[OPT_OPTICAL_ADJUSTMENT].desc = SANE_DESC_OPTICAL_ADJUSTMENT;
  s->opt[OPT_OPTICAL_ADJUSTMENT].type = SANE_TYPE_BUTTON;

  /* MAINTENANCE DATA */
  s->opt[OPT_DATA_GROUP].name = "";
  s->opt[OPT_DATA_GROUP].title = "Maintenance Data";
  s->opt[OPT_DATA_GROUP].desc = "";
  s->opt[OPT_DATA_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_DATA_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_DATA_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_UPDATE].name = "Update";
  s->opt[OPT_UPDATE].title = "Update";
  s->opt[OPT_UPDATE].desc = "Update scanner data";
  s->opt[OPT_UPDATE].type = SANE_TYPE_BUTTON;
  s->opt[OPT_NREGX_ADF].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_NREGX_ADF].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_NREGX_ADF].name = "# registers in main-scanning in ADF mode";
  s->opt[OPT_NREGX_ADF].title = "# registers in main-scanning in ADF mode";
  s->opt[OPT_NREGX_ADF].desc = "# registers in main-scanning in ADF mode";
  s->opt[OPT_NREGX_ADF].type = SANE_TYPE_INT;
  s->opt[OPT_NREGX_ADF].unit = SANE_UNIT_NONE;
  s->opt[OPT_NREGX_ADF].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_NREGX_ADF].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_NREGY_ADF].name = "# registers in sub-scanning in ADF mode";
  s->opt[OPT_NREGY_ADF].title = "# registers in sub-scanning in ADF mode";
  s->opt[OPT_NREGY_ADF].desc = "# registers in sub-scanning in ADF mode";
  s->opt[OPT_NREGY_ADF].type = SANE_TYPE_INT;
  s->opt[OPT_NREGY_ADF].unit = SANE_UNIT_NONE;
  s->opt[OPT_NREGY_ADF].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_NREGY_ADF].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_NREGX_BOOK].name = "# registers in main-scanning in book mode";
  s->opt[OPT_NREGX_BOOK].title = "# registers in main-scanning in book mode";
  s->opt[OPT_NREGX_BOOK].desc = "# registers in main-scanning in book mode";
  s->opt[OPT_NREGX_BOOK].type = SANE_TYPE_INT;
  s->opt[OPT_NREGX_BOOK].unit = SANE_UNIT_NONE;
  s->opt[OPT_NREGX_BOOK].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_NREGX_BOOK].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_NREGY_BOOK].name = "# registers in sub-scanning in book mode";
  s->opt[OPT_NREGY_BOOK].title = "# registers in sub-scanning in book mode";
  s->opt[OPT_NREGY_BOOK].desc = "# registers in sub-scanning in book mode";
  s->opt[OPT_NREGY_BOOK].type = SANE_TYPE_INT;
  s->opt[OPT_NREGY_BOOK].unit = SANE_UNIT_NONE;
  s->opt[OPT_NREGY_BOOK].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_NREGY_BOOK].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_NSCANS_ADF].name = "# ADF Scans";
  s->opt[OPT_NSCANS_ADF].title = "# ADF Scans";
  s->opt[OPT_NSCANS_ADF].desc = "# ADF Scans";
  s->opt[OPT_NSCANS_ADF].type = SANE_TYPE_INT;
  s->opt[OPT_NSCANS_ADF].unit = SANE_UNIT_NONE;
  s->opt[OPT_NSCANS_ADF].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_NSCANS_ADF].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_NSCANS_BOOK].name = "# BOOK Scans";
  s->opt[OPT_NSCANS_BOOK].title = "# BOOK Scans";
  s->opt[OPT_NSCANS_BOOK].desc = "# BOOK Scans";
  s->opt[OPT_NSCANS_BOOK].type = SANE_TYPE_INT;
  s->opt[OPT_NSCANS_BOOK].unit = SANE_UNIT_NONE;
  s->opt[OPT_NSCANS_BOOK].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_NSCANS_BOOK].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_LAMP_TIME].name = "LAMP TIME";
  s->opt[OPT_LAMP_TIME].title = "LAMP TIME";
  s->opt[OPT_LAMP_TIME].desc = "LAMP TIME";
  s->opt[OPT_LAMP_TIME].type = SANE_TYPE_INT;
  s->opt[OPT_LAMP_TIME].unit = SANE_UNIT_NONE;
  s->opt[OPT_LAMP_TIME].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_LAMP_TIME].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_EO_ODD].name = "E/O Balance ODD";
  s->opt[OPT_EO_ODD].title = "E/O Balance ODD";
  s->opt[OPT_EO_ODD].desc = "Adj. of E/O Balance in black level ODD";
  s->opt[OPT_EO_ODD].type = SANE_TYPE_INT;
  s->opt[OPT_EO_ODD].unit = SANE_UNIT_NONE;
  s->opt[OPT_EO_ODD].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_EO_ODD].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_EO_EVEN].name = "E/O Balance EVEN";
  s->opt[OPT_EO_EVEN].title = "E/O Balance EVEN";
  s->opt[OPT_EO_EVEN].desc = "Adj. of E/O Balance in black level EVEN";
  s->opt[OPT_EO_EVEN].type = SANE_TYPE_INT;
  s->opt[OPT_EO_EVEN].unit = SANE_UNIT_NONE;
  s->opt[OPT_EO_EVEN].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_EO_EVEN].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_BLACK_LEVEL_ODD].name = "Black Level ODD";
  s->opt[OPT_BLACK_LEVEL_ODD].title = "Black Level ODD";
  s->opt[OPT_BLACK_LEVEL_ODD].desc = "Adj. data in black level (ODD)";
  s->opt[OPT_BLACK_LEVEL_ODD].type = SANE_TYPE_INT;
  s->opt[OPT_BLACK_LEVEL_ODD].unit = SANE_UNIT_NONE;
  s->opt[OPT_BLACK_LEVEL_ODD].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_BLACK_LEVEL_ODD].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_BLACK_LEVEL_EVEN].name = "Black Level EVEN";
  s->opt[OPT_BLACK_LEVEL_EVEN].title = "Black Level EVEN";
  s->opt[OPT_BLACK_LEVEL_EVEN].desc = "Adj. data in black level (EVEN)";
  s->opt[OPT_BLACK_LEVEL_EVEN].type = SANE_TYPE_INT;
  s->opt[OPT_BLACK_LEVEL_EVEN].unit = SANE_UNIT_NONE;
  s->opt[OPT_BLACK_LEVEL_EVEN].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_BLACK_LEVEL_EVEN].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_WHITE_LEVEL_ODD].name = "White Level ODD";
  s->opt[OPT_WHITE_LEVEL_ODD].title = "White Level ODD";
  s->opt[OPT_WHITE_LEVEL_ODD].desc = "Adj. data in White level (ODD)";
  s->opt[OPT_WHITE_LEVEL_ODD].type = SANE_TYPE_INT;
  s->opt[OPT_WHITE_LEVEL_ODD].unit = SANE_UNIT_NONE;
  s->opt[OPT_WHITE_LEVEL_ODD].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_WHITE_LEVEL_ODD].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_WHITE_LEVEL_EVEN].name = "White Level EVEN";
  s->opt[OPT_WHITE_LEVEL_EVEN].title = "White Level EVEN";
  s->opt[OPT_WHITE_LEVEL_EVEN].desc = "Adj. data in White level (EVEN)";
  s->opt[OPT_WHITE_LEVEL_EVEN].type = SANE_TYPE_INT;
  s->opt[OPT_WHITE_LEVEL_EVEN].unit = SANE_UNIT_NONE;
  s->opt[OPT_WHITE_LEVEL_EVEN].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_WHITE_LEVEL_EVEN].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_WHITE_LEVEL_EVEN].name = "White Level EVEN";
  s->opt[OPT_WHITE_LEVEL_EVEN].title = "White Level EVEN";
  s->opt[OPT_WHITE_LEVEL_EVEN].desc = "Adj. data in White level (EVEN)";
  s->opt[OPT_WHITE_LEVEL_EVEN].type = SANE_TYPE_INT;
  s->opt[OPT_WHITE_LEVEL_EVEN].unit = SANE_UNIT_NONE;
  s->opt[OPT_WHITE_LEVEL_EVEN].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_WHITE_LEVEL_EVEN].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_DENSITY].name = "Density Adjustment";
  s->opt[OPT_DENSITY].title = "Density Adjustment";
  s->opt[OPT_DENSITY].desc = "Density adjustment of std. white board";
  s->opt[OPT_DENSITY].type = SANE_TYPE_INT;
  s->opt[OPT_DENSITY].unit = SANE_UNIT_NONE;
  s->opt[OPT_DENSITY].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_DENSITY].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_FIRST_ADJ_WHITE_ODD].name = "1st adj. in white level (ODD)";
  s->opt[OPT_FIRST_ADJ_WHITE_ODD].title = "1st adj. in white level (ODD)";
  s->opt[OPT_FIRST_ADJ_WHITE_ODD].desc = "1st adj. in white level (ODD)";
  s->opt[OPT_FIRST_ADJ_WHITE_ODD].type = SANE_TYPE_INT;
  s->opt[OPT_FIRST_ADJ_WHITE_ODD].unit = SANE_UNIT_NONE;
  s->opt[OPT_FIRST_ADJ_WHITE_ODD].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_FIRST_ADJ_WHITE_ODD].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_FIRST_ADJ_WHITE_EVEN].name = "1st adj. in white level (EVEN)";
  s->opt[OPT_FIRST_ADJ_WHITE_EVEN].title = "1st adj. in white level (EVEN)";
  s->opt[OPT_FIRST_ADJ_WHITE_EVEN].desc = "1st adj. in white level (EVEN)";
  s->opt[OPT_FIRST_ADJ_WHITE_EVEN].type = SANE_TYPE_INT;
  s->opt[OPT_FIRST_ADJ_WHITE_EVEN].unit = SANE_UNIT_NONE;
  s->opt[OPT_FIRST_ADJ_WHITE_EVEN].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_FIRST_ADJ_WHITE_EVEN].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_NREGX_REVERSE].name = "# registers of main-scanning of backside";
  s->opt[OPT_NREGX_REVERSE].title =
    "# registers of main-scanning of backside";
  s->opt[OPT_NREGX_REVERSE].desc =
    "# registers of main-scanning of ADF backside";
  s->opt[OPT_NREGX_REVERSE].type = SANE_TYPE_INT;
  s->opt[OPT_NREGX_REVERSE].unit = SANE_UNIT_NONE;
  s->opt[OPT_NREGX_REVERSE].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_NREGX_REVERSE].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_NREGY_REVERSE].name = "# registers of sub-scanning of backside";
  s->opt[OPT_NREGY_REVERSE].title = "# registers of sub-scanning of backside";
  s->opt[OPT_NREGY_REVERSE].desc =
    "# registers of sub-scanning of ADF backside";
  s->opt[OPT_NREGY_REVERSE].type = SANE_TYPE_INT;
  s->opt[OPT_NREGY_REVERSE].unit = SANE_UNIT_NONE;
  s->opt[OPT_NREGY_REVERSE].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_NREGY_REVERSE].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_NSCANS_REVERSE_ADF].name = "# of scans of reverse side in ADF";
  s->opt[OPT_NSCANS_REVERSE_ADF].title = "# of scans of reverse side in ADF";
  s->opt[OPT_NSCANS_REVERSE_ADF].desc = "# of scans of reverse side in ADF";
  s->opt[OPT_NSCANS_REVERSE_ADF].type = SANE_TYPE_INT;
  s->opt[OPT_NSCANS_REVERSE_ADF].unit = SANE_UNIT_NONE;
  s->opt[OPT_NSCANS_REVERSE_ADF].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_NSCANS_REVERSE_ADF].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_REVERSE_TIME].name = "LAMP TIME (reverse)";
  s->opt[OPT_REVERSE_TIME].title = "LAMP TIME (reverse)";
  s->opt[OPT_REVERSE_TIME].desc = "LAMP TIME (reverse)";
  s->opt[OPT_REVERSE_TIME].type = SANE_TYPE_INT;
  s->opt[OPT_REVERSE_TIME].unit = SANE_UNIT_NONE;
  s->opt[OPT_REVERSE_TIME].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_REVERSE_TIME].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_NCHARS].name = "# of endorser characters";
  s->opt[OPT_NCHARS].title = "# of endorser characters";
  s->opt[OPT_NCHARS].desc = "# of endorser characters";
  s->opt[OPT_NCHARS].type = SANE_TYPE_INT;
  s->opt[OPT_NCHARS].unit = SANE_UNIT_NONE;
  s->opt[OPT_NCHARS].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_NCHARS].constraint_type = SANE_CONSTRAINT_NONE;

  DBG (DBG_proc, "<< init_options\n");
  return SANE_STATUS_GOOD;
}

static SANE_Status
attach (SANE_String_Const devname, int __sane_unused__ connType,
	HS2P_Device ** devp)
{
  SANE_Status status;
  HS2P_Device *dev;
  struct inquiry_standard_data ibuf;
  struct inquiry_vpd_data vbuf;
  struct inquiry_jis_data jbuf;
  size_t buf_size;
  int fd = -1;
  double mm;

  char device_string[60];

  unsigned int i;
  SANE_String *str;


  DBG (DBG_sane_proc, ">>> attach:\n");

  for (dev = first_dev; dev; dev = dev->next)
    {
      if (strcmp (dev->sane.name, devname) == 0)
	{
	  if (devp)
	    *devp = dev;
	  return SANE_STATUS_GOOD;
	}
    }
  DBG (DBG_sane_proc, ">>> attach: opening \"%s\"\n", devname);

  /* sanei_scsi_open takes an option bufsize argument */
  status = sanei_scsi_open (devname, &fd, &sense_handler, &(dev->sense_data));
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, ">>> attach: open failed: %s\n",
	   sane_strstatus (status));
      return (status);
    }

  DBG (DBG_sane_proc, ">>> attach: opened %s fd=%d\n", devname, fd);

  DBG (DBG_sane_proc, ">>> attach: sending INQUIRY (standard data)\n");
  memset (&ibuf, 0, sizeof (ibuf));
  buf_size = sizeof (ibuf);
  status = inquiry (fd, &ibuf, &buf_size, 0, HS2P_INQUIRY_STANDARD_PAGE_CODE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, ">>> attach: inquiry failed: %s\n",
	   sane_strstatus (status));
      sanei_scsi_close (fd);
      return (status);
    }

  DBG (DBG_info,
       ">>> attach: reported devtype='%d', vendor='%.8s', product='%.16s', revision='%.4s'\n",
       ibuf.devtype, ibuf.vendor, ibuf.product, ibuf.revision);
  DBG (DBG_info,
       ">>> attach: reported RMB=%#x Ver=%#x ResponseDataFormat=%#x Length=%#x Byte7=%#x\n",
       ibuf.rmb_evpd, ibuf.version, ibuf.response_data_format, ibuf.length,
       ibuf.byte7);

  if (ibuf.devtype != 6 || strncmp ((char *) ibuf.vendor, "RICOH   ", 8) != 0)
    {
      DBG (DBG_warning, ">>> attach: device is not a RICOH scanner\n");
      sanei_scsi_close (fd);
      return SANE_STATUS_INVAL;
    }
  else if (!is_device_supported ((char *) ibuf.product))
    {
      DBG (DBG_warning,
	   ">>> attach: device %s is not yet a supported RICOH scanner\n",
	   ibuf.product);
      sanei_scsi_close (fd);
      return SANE_STATUS_INVAL;
    }

  /* We should now have an open file descriptor to a supported hs2p scanner */
  DBG (DBG_sane_proc, ">>> attach: sending TEST_UNIT_READY\n");
  do
    {
      status = test_unit_ready (fd);
    }
  while (status == HS2P_SK_UNIT_ATTENTION);
  if (status != HS2P_SCSI_STATUS_GOOD)
    {
      DBG (DBG_error, ">>> attach: test unit ready failed (%s)\n",
	   sane_strstatus (status));
      sanei_scsi_close (fd);
      return (status);
    }

  DBG (DBG_sane_proc, ">>> attach: sending INQUIRY (vpd data)\n");
  memset (&vbuf, 0, sizeof (vbuf));
  buf_size = sizeof (vbuf);
  status = inquiry (fd, &vbuf, &buf_size, 1, HS2P_INQUIRY_VPD_PAGE_CODE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, ">>> attach: inquiry (vpd data) failed: %s\n",
	   sane_strstatus (status));
      sanei_scsi_close (fd);
      return status;
    }
  print_vpd_info (&vbuf);

  DBG (DBG_sane_proc, ">>> attach: sending INQUIRY (jis data)\n");
  memset (&jbuf, 0, sizeof (jbuf));
  buf_size = sizeof (jbuf);
  status = inquiry (fd, &jbuf, &buf_size, 1, HS2P_INQUIRY_JIS_PAGE_CODE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, ">>> attach: inquiry (jis data) failed: %s\n",
	   sane_strstatus (status));
      sanei_scsi_close (fd);
      return status;
    }
  print_jis_info (&jbuf);


  /* Fill in HS2P_Device {sane;info} */
  dev = malloc (sizeof (*dev));
  if (!dev)
    return SANE_STATUS_NO_MEM;
  memset (dev, 0, sizeof (*dev));

  /* Maximum Number of Sub-Sections of Main Scanning Window */
  if (strncmp ((char *) ibuf.product, "IS450", 5) == 0)
    {
      dev->info.max_win_sections = 4;
    }
  else if (strncmp ((char *) ibuf.product, "IS420", 5) == 0)
    {
      dev->info.max_win_sections = 6;
    }

  /* Some MODE SELECT scanner options */
  DBG (DBG_proc, ">>> attach: get_basic_measurement_unit\n");
  status =
    get_basic_measurement_unit (fd, &(dev->info.bmu), &(dev->info.mud));
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, ">>> attach: get_basic_measurement_unit failed (%s)\n",
	   sane_strstatus (status));
      DBG (DBG_error, ">>> attach: setting to defaults\n");
      status = set_basic_measurement_unit (fd, MILLIMETERS);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       ">>> attach: set_basic_measurement_unit failed (%s)\n",
	       sane_strstatus (status));
	}
    }
  if ((status =
       get_connection_parameters (fd, &(dev->info.cxn))) != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, ">>> attach: get_connection_parameters failed\n");
    }
  status = get_endorser_control (fd, &dev->info.endorser_control);
  if (status != SANE_STATUS_GOOD || dev->info.endorser_control != 0x01)
    {
      DBG (DBG_error,
	   ">>> attach: get_endorser_control failed: return value=%#02x\n",
	   dev->info.endorser_control);
      dev->info.endorser_control = 0x00;
    }
  if ((dev->info.service_mode = get_service_mode (fd)) != 0x00
      && dev->info.service_mode != 0x01)
    {
      DBG (DBG_error, ">>> attach: get_service_mode failed %#02x\n",
	   dev->info.service_mode);
      dev->info.service_mode = 0x00;
    }
  if ((dev->info.scan_wait_mode = get_scan_wait_mode (fd)) != 0x00
      && dev->info.scan_wait_mode != 0x01)
    {
      DBG (DBG_error,
	   ">>> attach: get_scan_wait_mode failed: return value=%#02x\n",
	   dev->info.scan_wait_mode);
      dev->info.scan_wait_mode = 0x00;
    }
  status = get_white_balance (fd, &dev->info.white_balance);
  if (status != SANE_STATUS_GOOD && dev->info.white_balance != 0x01)
    {
      DBG (DBG_error,
	   ">>> attach: get_white_balance failed: return value=%#02x\n",
	   dev->info.white_balance);
      dev->info.white_balance = RELATIVE_WHITE;
    }

  DBG (DBG_info, ">>> attach: flushing and closing fd=%d\n", fd);
  sanei_scsi_req_flush_all ();
  sanei_scsi_close (fd);

  dev->info.devtype = ibuf.devtype;
  snprintf (dev->info.vendor, 9, "%-.5s", ibuf.vendor);	/* RICOH */
  trim_spaces (dev->info.vendor, sizeof (dev->info.vendor));
  snprintf (dev->info.product, 16, "%-.16s", ibuf.product);	/* IS450 */
  trim_spaces (dev->info.product, sizeof (dev->info.product));
  snprintf (dev->info.revision, 5, "%-.4s", ibuf.revision);	/* 1R04 */
  trim_spaces (dev->info.revision, sizeof (dev->info.revision));

  /* SANE_Device sane information */
  dev->sane.name = strdup (devname);
  dev->sane.vendor =
    (strcmp (dev->info.vendor, "RICOH") ==
     0) ? strdup ("Ricoh") : strdup (dev->info.vendor);
  dev->sane.model = strdup (dev->info.product);
  dev->sane.type = strdup ("sheetfed scanner");
  /*
     dev->sane.email_backend_author = strdup("<Jeremy Johnson> jeremy@acjlaw.net");
     dev->sane.backend_website = strdup("http://www.acjlaw.net:8080/~jeremy/Ricoh");
   */
  /* read these values from backend configuration file using parse_configuration
     dev->sane.location = strdup();
     dev->sane.comment = strdup();
     dev->sane.backend_version_code = strdup();
   */
  /* NOT YET USED */
  /* dev->sane.backend_capablity_flags = 0x00; */

  /* set capabilities from vpd */
  /* adf_id: 0=none,1=simplex,2=duplex,3=ARDF,4=reserved;  should be 1 or 2 for IS450 family */
  dev->info.hasADF = vbuf.adf_id == 0 ? SANE_FALSE : SANE_TRUE;
  dev->info.hasSimplex = vbuf.adf_id == 1 ? SANE_TRUE : SANE_FALSE;
  dev->info.hasDuplex = vbuf.adf_id == 2 ? SANE_TRUE : SANE_FALSE;
  dev->info.hasARDF = vbuf.adf_id == 3 ? SANE_TRUE : SANE_FALSE;

  /* end_id 0=none,1=Yes,2=reserved;  should always be 0 or 1 */
  dev->info.hasEndorser = vbuf.end_id == 1 ? SANE_TRUE : SANE_FALSE;
  for (i = 0; i < 20; i++)
    dev->info.endorser_string[i] = '\0';

  /* ipu_id: Bit0: '0'-no IPU, '1'-has IPU
   *         Bit1: '0'-no extended board, '1'-has extended board;
   * should always be 0 
   */
  dev->info.hasIPU = (vbuf.ipu_id & 0x01) == 0x01 ? SANE_TRUE : SANE_FALSE;
  dev->info.hasXBD = (vbuf.ipu_id & 0x02) == 0x02 ? SANE_TRUE : SANE_FALSE;


  /* Image Composition Byte is set to 0x37 (0011 0111) */
  dev->info.supports_lineart = (vbuf.imagecomposition & 0x01) == 0x01 ? SANE_TRUE : SANE_FALSE;	/* TRUE */
  dev->info.supports_dithering = (vbuf.imagecomposition & 0x02) == 0x02 ? SANE_TRUE : SANE_FALSE;	/* TRUE */
  dev->info.supports_errordiffusion = (vbuf.imagecomposition & 0x04) == 0x04 ? SANE_TRUE : SANE_FALSE;	/* TRUE */
  dev->info.supports_color = (vbuf.imagecomposition & 0x08) == 0x08 ? SANE_TRUE : SANE_FALSE;	/* FALSE */
  dev->info.supports_4bitgray = (vbuf.imagecomposition & 0x10) == 0x10 ? SANE_TRUE : SANE_FALSE;	/* TRUE */
  dev->info.supports_8bitgray = (vbuf.imagecomposition & 0x20) == 0x20 ? SANE_TRUE : SANE_FALSE;	/* TRUE */
  /* vbuf.imagecomposition & 0x40; FALSE */
  /* vbuf.imagecomposition & 0x80  FALSE reserved */
  str = &scan_mode_list[0];	/* array of string pointers */
  if (dev->info.supports_lineart)
    *str++ = strdup (SM_LINEART);
  if (dev->info.supports_dithering || dev->info.supports_errordiffusion)
    *str++ = strdup (SM_HALFTONE);
  if (dev->info.supports_color)
    *str++ = strdup (SM_COLOR);
  if (dev->info.supports_4bitgray)
    *str++ = strdup (SM_4BITGRAY);
  if (dev->info.supports_8bitgray)
    *str++ = strdup (SM_8BITGRAY);
  *str = NULL;

  snprintf (device_string, 60, "Flatbed%s%s%s%s%s%s",
	    dev->info.hasADF ? "/ADF" : "",
	    dev->info.hasDuplex ? "/Duplex" : "",
	    dev->info.hasEndorser ? "/Endorser" : "",
	    dev->info.hasIPU ? "/IPU" : "",
	    dev->info.supports_color ? " Color" : " B&W", " Scanner");
  dev->sane.type = strdup (device_string);

  /* ACE Image Data Processing  Binary Filters
   * For IS450 this is set to 0x18 (0001 1000) if IPU installed, else 0x00 
   * For IS420 this is set to 0x3C (0011 1100) if IPU installed, else 0x00 
   */
  dev->info.supports_whiteframing =
    ((vbuf.imagedataprocessing[0] & 0x01) == 0x01) ? SANE_TRUE : SANE_FALSE;
  dev->info.supports_blackframing =
    ((vbuf.imagedataprocessing[0] & 0x02) == 0x02) ? SANE_TRUE : SANE_FALSE;
  dev->info.supports_edgeextraction =
    ((vbuf.imagedataprocessing[0] & 0x04) == 0x04) ? SANE_TRUE : SANE_FALSE;
  dev->info.supports_noiseremoval =
    ((vbuf.imagedataprocessing[0] & 0x08) == 0x08) ? SANE_TRUE : SANE_FALSE;
  dev->info.supports_smoothing =
    ((vbuf.imagedataprocessing[0] & 0x10) == 0x10) ? SANE_TRUE : SANE_FALSE;
  dev->info.supports_linebolding =
    ((vbuf.imagedataprocessing[0] & 0x20) == 0x20) ? SANE_TRUE : SANE_FALSE;

  /* Compression Method is not supported for IS450
   *                    is     supported for IS420  */
  dev->info.supports_MH =
    ((vbuf.compression & 0x01) == 0x01) ? SANE_TRUE : SANE_FALSE;
  dev->info.supports_MR =
    ((vbuf.compression & 0x02) == 0x02) ? SANE_TRUE : SANE_FALSE;
  dev->info.supports_MMR =
    ((vbuf.compression & 0x04) == 0x04) ? SANE_TRUE : SANE_FALSE;
  dev->info.supports_MHB = ((vbuf.compression & 0x08) == 0x08) ? SANE_TRUE : SANE_FALSE;	/* MH Byte Boundary */

  /* compression_list[] will have variable number of elements, but the order will be fixed as follows: */
  str = &compression_list[0];
  *str++ = strdup ("none");
  if (dev->info.supports_MH)
    *str++ = strdup ("G3 1-D MH");
  if (dev->info.supports_MR)
    *str++ = strdup ("G3 2-D MR");
  if (dev->info.supports_MMR)
    *str++ = strdup ("G4 2-D MMR");
  if (dev->info.supports_MHB)
    *str++ = strdup ("MH Byte Boundary");
  *str = NULL;

  /* Marker Recognition is set to 0x00 */
  dev->info.supports_markerrecognition =
    ((vbuf.markerrecognition & 0x01) == 0x01) ? SANE_TRUE : SANE_FALSE;

  /* Size Recognition 
   * For IS450 this is set to 0x01 when IPU installed; else 0x00
   * For IS420 this is set to 0x01 
   */
  dev->info.supports_sizerecognition =
    ((vbuf.sizerecognition & 0x01) == 0x01) ? SANE_TRUE : SANE_FALSE;

  /* X Maximum Output Pixel in main scanning direction 
   * For IS450 this is set to 0x1360 (4960) 
   * For IS420 this is set to        (4880) 
   * [MostSignificantByte LeastSignificantByte]
   */
  dev->info.xmaxoutputpixels =
    (vbuf.xmaxoutputpixels[0] << 8) | vbuf.xmaxoutputpixels[1];

  /* Set capabilities from jis VPD IDENTIFIER Page Code F0H */
  dev->info.resBasicX = _2btol (&jbuf.BasicRes.x[0]);	/* set to 400 */
  dev->info.resBasicY = _2btol (&jbuf.BasicRes.y[0]);	/* set to 400 */

  dev->info.resXstep = (jbuf.resolutionstep >> 4) & 0x0F;	/* set to 1   */
  dev->info.resYstep = jbuf.resolutionstep & 0x0F;	/* set to 1   */
  dev->info.resMaxX = _2btol (&jbuf.MaxRes.x[0]);	/* set to 800 */
  dev->info.resMaxY = _2btol (&jbuf.MaxRes.y[0]);	/* set to 800 */
  dev->info.resMinX = _2btol (&jbuf.MinRes.x[0]);	/* set to 100 for IS450 and 60 for IS420 */
  dev->info.resMinY = _2btol (&jbuf.MinRes.y[0]);	/* set to 100 for IS450 and 60 for IS420 */

  dev->info.xres_range.min = _2btol (&jbuf.MinRes.x[0]);	/* set to 100 for IS450 and 60 for IS420 */
  dev->info.xres_range.max = _2btol (&jbuf.MaxRes.x[0]);	/* set to 800 */
  dev->info.resXstep = (jbuf.resolutionstep >> 4) & 0x0F;	/* set to 1   */
  dev->info.xres_range.quant = dev->info.resXstep;

  dev->info.yres_range.min = _2btol (&jbuf.MinRes.y[0]);	/* set to 100 for IS450 and 60 for IS420 */
  dev->info.yres_range.max = _2btol (&jbuf.MaxRes.y[0]);	/* set to 800 */
  dev->info.resYstep = jbuf.resolutionstep & 0x0F;	/* set to 1   */
  dev->info.yres_range.quant = dev->info.resYstep;

  /* set the length of the list to zero first, then append standard resolutions */
  i = 0;
  if ((jbuf.standardres[0] & 0x80) == 0x80)
    dev->info.resStdList[++i] = 60;
  if ((jbuf.standardres[0] & 0x40) == 0x40)
    dev->info.resStdList[++i] = 75;
  if ((jbuf.standardres[0] & 0x20) == 0x20)
    dev->info.resStdList[++i] = 100;
  if ((jbuf.standardres[0] & 0x10) == 0x10)
    dev->info.resStdList[++i] = 120;
  if ((jbuf.standardres[0] & 0x08) == 0x08)
    dev->info.resStdList[++i] = 150;
  if ((jbuf.standardres[0] & 0x04) == 0x04)
    dev->info.resStdList[++i] = 160;
  if ((jbuf.standardres[0] & 0x02) == 0x02)
    dev->info.resStdList[++i] = 180;
  if ((jbuf.standardres[0] & 0x01) == 0x01)
    dev->info.resStdList[++i] = 200;
  if ((jbuf.standardres[1] & 0x80) == 0x80)
    dev->info.resStdList[++i] = 240;
  if ((jbuf.standardres[1] & 0x40) == 0x40)
    dev->info.resStdList[++i] = 300;
  if ((jbuf.standardres[1] & 0x20) == 0x20)
    dev->info.resStdList[++i] = 320;
  if ((jbuf.standardres[1] & 0x10) == 0x10)
    dev->info.resStdList[++i] = 400;
  if ((jbuf.standardres[1] & 0x08) == 0x08)
    dev->info.resStdList[++i] = 480;
  if ((jbuf.standardres[1] & 0x04) == 0x04)
    dev->info.resStdList[++i] = 600;
  if ((jbuf.standardres[1] & 0x02) == 0x02)
    dev->info.resStdList[++i] = 800;
  if ((jbuf.standardres[1] & 0x01) == 0x01)
    dev->info.resStdList[++i] = 1200;
  dev->info.resStdList[0] = i;	/* number of resolutions */
  if (dev->info.resStdList[0] == 0)
    {				/* make a default standard resolutions for 200 and 300dpi */
      DBG (DBG_warning, "attach: no standard resolutions reported\n");
      dev->info.resStdList[0] = 2;
      dev->info.resStdList[1] = 200;
      dev->info.resStdList[2] = 300;
      dev->info.resBasicX = dev->info.resBasicY = 300;
    }
  DBG (DBG_info, "attach: Window(W/L) = (%lu/%lu)\n",
       _4btol (&jbuf.Window.width[0]), _4btol (&jbuf.Window.length[0]));
  dev->info.winWidth = _4btol (&jbuf.Window.width[0]);
  dev->info.winHeight = _4btol (&jbuf.Window.length[0]);
  if (dev->info.winWidth <= 0)
    {
      dev->info.winWidth = (SANE_Int) (dev->info.resBasicX * 8.5);
      DBG (DBG_warning, "attach: invalid window width reported, using %d\n",
	   dev->info.winWidth);
    }
  if (dev->info.winHeight <= 0)
    {
      dev->info.winHeight = dev->info.resBasicY * 14;
      DBG (DBG_warning, "attach: invalid window height reported, using %d\n",
	   dev->info.winHeight);
    }
  /* 4692 / 400 * 25.4 = 297 */
  mm = (dev->info.resBasicX > 0) ?
    ((double) dev->info.winWidth / (double) dev->info.resBasicX *
     MM_PER_INCH) : 0.0;
  dev->info.x_range.min = SANE_FIX (0.0);
  dev->info.x_range.max = SANE_FIX (mm);
  dev->info.x_range.quant = SANE_FIX (0.0);
  DBG (DBG_info, "attach: winWidth=%d resBasicX=%d mm/in=%f mm=%f\n",
       dev->info.winWidth, dev->info.resBasicX, MM_PER_INCH, mm);

  mm = (dev->info.resBasicY > 0) ?
    ((double) dev->info.winHeight / (double) dev->info.resBasicY *
     MM_PER_INCH) : 0.0;
  dev->info.y_range.min = SANE_FIX (0.0);
  dev->info.y_range.max = SANE_FIX (mm);
  dev->info.y_range.quant = SANE_FIX (0.0);

  DBG (DBG_info, "attach: RANGE x_range.max=%f, y_range.max=%f\n",
       SANE_UNFIX (dev->info.x_range.max),
       SANE_UNFIX (dev->info.y_range.max));

  /* min, max, quantization  light-dark  1-255, 0 means default 128 */
  dev->info.brightness_range.min = 1;
  dev->info.brightness_range.max = 255;
  dev->info.brightness_range.quant = 1;
  /* min, max, quantization  white-black 1-255, 0 means default 128 */
  dev->info.contrast_range.min = 1;
  dev->info.contrast_range.max = 255;
  dev->info.contrast_range.quant = 1;
  /* min, max, quantization  low-high    1-255, 0 means default 128 */
  dev->info.threshold_range.min = 1;
  dev->info.threshold_range.max = 255;
  dev->info.threshold_range.quant = 1;

  /* jbuf.functions */
  dev->info.overflow_support =
    ((jbuf.functions & 0x01) == 0x01) ? SANE_TRUE : SANE_FALSE;
  dev->info.lineart_support =
    ((jbuf.functions & 0x02) == 0x02) ? SANE_TRUE : SANE_FALSE;
  dev->info.dither_support =
    ((jbuf.functions & 0x04) == 0x04) ? SANE_TRUE : SANE_FALSE;
  dev->info.grayscale_support =
    ((jbuf.functions & 0x08) == 0x08) ? SANE_TRUE : SANE_FALSE;

  /* set option defaults  */
  dev->info.default_res = dev->info.resBasicX;
  dev->info.default_xres = dev->info.resBasicX;
  dev->info.default_yres = dev->info.resBasicY;
  dev->info.default_imagecomposition = LINEART;
  dev->info.default_media = FLATBED;
  dev->info.default_duplex = SANE_FALSE;

  /* dev->info.autoborder_default = dev->info.canBorderRecog; */
  /*
     dev->info.batch_default = SANE_FALSE;
     dev->info.deskew_default = SANE_FALSE;
     dev->info.check_adf_default = SANE_FALSE;
     dev->info.timeout_adf_default = 0;
     dev->info.timeout_manual_default = 0;
   */
  /* dev->info.control_panel_default = dev->info.canACE; Image Data Processing */

  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    *devp = dev;

  DBG (DBG_sane_proc, "<<< attach:\n");
  return SANE_STATUS_GOOD;
}




/* SANE callback to attach a SCSI device */
static SANE_Status
attach_one_scsi (const char *devname)
{
  return attach (devname, CONNECTION_SCSI, NULL);
  /* return SANE_STATUS_GOOD; */
}

static void
parse_configuration_file (FILE * fp)
{
  char line[PATH_MAX], *s, *t;
  int linenumber;

  DBG (DBG_proc, ">> parse_configuration_file\n");

  if (fp == NULL)
    {
      DBG (DBG_proc,
	   ">> parse_configuration_file: No config file present!\n");
    }
  else
    {				/*parse configuration file */
      for (linenumber = 0; sanei_config_read (line, sizeof (line), fp);
	   linenumber++)
	{
	  DBG (DBG_proc,
	       ">> parse_configuration_file: parsing config line \"%s\"\n",
	       line);
	  if (line[0] == '#')
	    continue;		/* ignore line comments */
	  for (s = line; isspace (*s); ++s);	/* skip white space: */
	  for (t = s; *t != '\0'; t++);
	  for (--t; t > s && isspace (*t); t--);
	  *(++t) = '\0';	/*trim trailing space */
	  if (!strlen (s))
	    continue;		/* ignore empty lines */
	  if ((t = strstr (s, "scsi ")) != NULL)
	    {
	      /*  scsi VENDOR MODEL TYPE BUS CHANNEL ID LUN */
	      DBG (DBG_proc,
		   ">> parse_configuration_file: config file line %d: trying to attach SCSI: %s'\n",
		   linenumber, line);
	      sanei_config_attach_matching_devices (t, attach_one_scsi);
	    }
	  else if ((t = strstr (s, "/dev/")) != NULL)
	    {
	      /* /dev/scanner /dev/sg0 */
	      DBG (DBG_proc,
		   ">> parse_configuration_file: config file line %d: trying to attach SCSI: %s'\n",
		   linenumber, line);
	      sanei_config_attach_matching_devices (t, attach_one_scsi);
	    }
	  else if ((t = strstr (s, "option")) != NULL)
	    {
	      for (t += 6; isspace (*t); t++);	/* skip to flag */
	      /* if(strstr(t,"FLAG_VALUE")!=NULL) FLAG_VALUE=SANE_TRUE; */
	    }
	  else
	    {
	      DBG (DBG_proc,
		   ">> parse_configuration_file: config file line %d: OBSOLETE !! use the scsi keyword!\n",
		   linenumber);
	      DBG (DBG_proc,
		   ">> parse_configuration_file:   (see man sane-avision for details): trying to attach SCSI: %s'\n",
		   line);
	    }
	}
      fclose (fp);
    }
  DBG (DBG_proc, "<< parse_configuration_file\n");
  return;
}

static SANE_Status
do_cancel (HS2P_Scanner * s)
{
  SANE_Status status;
  DBG (DBG_sane_proc, ">> do_cancel\n");

  DBG (DBG_proc, "cancel: sending OBJECT POSITION\n");

  s->scanning = SANE_FALSE;
  s->cancelled = SANE_TRUE;
  s->EOM = SANE_FALSE;

  if (s->fd >= 0)
    {
      if ((status =
	   object_position (s->fd,
			    OBJECT_POSITION_UNLOAD)) != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "cancel: OBJECT POSTITION failed\n");
	}
      sanei_scsi_req_flush_all ();
      release_unit (s->fd);
      sanei_scsi_close (s->fd);
      s->fd = -1;
    }
  /*
     if (s->reader_pid > 0){
     int exit_status;
     sanei_thread_kill (s->reader_pid);
     sanei_thread_waitpid (s->reader_pid, &exit_status);
     s->reader_pid = 0;
     }
   */

  DBG (DBG_sane_proc, "<< do_cancel\n");
  return (SANE_STATUS_CANCELLED);
}


SANE_Status
sane_init (SANE_Int * version_code,
	   SANE_Auth_Callback __sane_unused__ authorize)
{
  FILE *fp;

  DBG_INIT ();			/* initialize SANE DEBUG */

  /*DBG (DBG_sane_init, "> sane_init (authorize = %p)\n", (void *) authorize); */
#if defined PACKAGE && defined VERSION
  DBG (DBG_sane_init, "> sane_init: hs2p backend version %d.%d-%d ("
       PACKAGE " " VERSION ")\n", SANE_CURRENT_MAJOR, V_MINOR, BUILD);
#endif
  /*
     sanei_thread_init ();
   */

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, 0);


  if ((fp = sanei_config_open (HS2P_CONFIG_FILE)) != NULL)
    {
      parse_configuration_file (fp);
    }
  else
    {
      DBG (DBG_sane_init, "> sane_init: No config file \"%s\" present!\n",
	   HS2P_CONFIG_FILE);
    }

#if 0
  /* avision.c: search for all supported scanners on all scsi busses & channels */
  for (hw = &HS2P_Device_List[0]; hw->mfg != NULL; hw++)
    {
      sanei_scsi_find_devices (hw->mfg,	/*vendor */
			       hw->model,	/*model */
			       NULL,	/*all types */
			       -1,	/*all bus */
			       -1,	/*all channel */
			       -1,	/*all id */
			       -1,	/*all lun */
			       attach_one_scsi);	/*callback */
      DBG (2, "sane_init: %s %s\n", hw->mfg, hw->model);
    }
#endif

  DBG (DBG_sane_init, "< sane_init\n");
  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  HS2P_Device *dev, *next;
  DBG (DBG_proc, ">> sane_exit\n");

  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      free ((void *) (SANE_String_Const *) dev->sane.name);
      free ((SANE_String_Const *) dev->sane.model);
      free (dev);
    }

  DBG (DBG_proc, "<< sane_exit\n");
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  static const SANE_Device **devlist = 0;
  HS2P_Device *dev;
  int i;
  DBG (DBG_proc, ">> sane_get_devices (local_only = %d)\n", local_only);

  if (devlist)
    free (devlist);
  devlist = malloc ((num_devices + 1) * sizeof (devlist[0]));
  if (!devlist)
    return (SANE_STATUS_NO_MEM);

  i = 0;
  for (dev = first_dev; dev; dev = dev->next)
    devlist[i++] = &dev->sane;
  devlist[i++] = 0;

  *device_list = devlist;

  DBG (DBG_proc, "<< sane_get_devices\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devnam, SANE_Handle * handle)
{
  SANE_Status status;
  HS2P_Device *dev;
  HS2P_Scanner *s;
  DBG (DBG_proc, "> sane_open\n");

  if (devnam[0] == '\0')
    {
      for (dev = first_dev; dev; dev = dev->next)
	{
	  if (strcmp (dev->sane.name, devnam) == 0)
	    break;
	}
      if (!dev)
	{
	  status = attach (devnam, CONNECTION_SCSI, &dev);
	  if (status != SANE_STATUS_GOOD)
	    return (status);
	}
    }
  else
    {
      dev = first_dev;
    }
  if (!dev)
    return (SANE_STATUS_INVAL);

  s = malloc (sizeof (*s));
  if (!s)
    return SANE_STATUS_NO_MEM;
  memset (s, 0, sizeof (*s));	/* initialize */

  s->fd = -1;
  s->hw = dev;
  s->hw->info.bmu = s->bmu = MILLIMETERS;	/* 01H */
  s->hw->info.mud = s->mud = 1;	/* If the scale is MM or POINT, mud is fixed to 1 */
  s->bpp = 1;			/* supports 1,4,6,8 so we set to LINEART 1bpp     */
  /*
     s->scanning  = SANE_FALSE;
     s->cancelled =  SANE_FALSE;
   */
  /*
   */

  ScannerDump (s);
  init_options (s);

  s->next = first_handle;	/* insert newly opened handle into list of open handles: */
  first_handle = s;

  /* initialize our parameters here AND in sane_start?
     get_parameters(s, 0);
   */

  *handle = s;

  DBG (DBG_proc, "< sane_open\n");
  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  HS2P_Scanner *s = (HS2P_Scanner *) handle;
  char **str;
  DBG (DBG_proc, ">> sane_close\n");

  if (s->fd != -1)
    sanei_scsi_close (s->fd);
  free (s);

  for (str = &compression_list[0]; *str; str++);
  free (*str);
  for (str = &scan_mode_list[0]; *str; str++);
  free (*str);

  DBG (DBG_proc, "<< sane_close\n");
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  HS2P_Scanner *s = handle;
  DBG (DBG_proc, ">> sane_get_option_descriptor: %d name=%s\n", option,
       s->opt[option].name);

  if ((unsigned) option >= NUM_OPTIONS)
    return (0);

  DBG (DBG_info, "<< sane_get_option_descriptor: name=%s\n",
       s->opt[option].name);
  return (s->opt + option);
}

#if 0
static SANE_Int
get_scan_mode_id (char *s)	/* sequential search */
{
  SANE_Int i;

  for (i = 0; scan_mode_list[i]; i++)
    if (strcmp (s, scan_mode_list[i]) == 0)
      break;

  /* unknown strings are treated as 'lineart' */
  return scan_mode_list[i] ? i : 0;
}
#endif
static SANE_Status
update_hs2p_data (HS2P_Scanner * s)
{

  DBG (DBG_proc, ">> update_hs2p_data\n");
  /* OPT_NREGX_ADF: */
  DBG (DBG_sane_option, "OPT_NREGX_ADF\n");
  s->val[OPT_NREGX_ADF].w = (SANE_Word) s->data.maintenance.nregx_adf;

  /* OPT_NREGY_ADF: */
  DBG (DBG_sane_option, "OPT_NREGY_ADF\n");
  s->val[OPT_NREGY_ADF].w = (SANE_Word) s->data.maintenance.nregx_book;

  /* OPT_NREGX_BOOK: */
  DBG (DBG_sane_option, "OPT_NREGX_BOOK\n");
  s->val[OPT_NREGX_BOOK].w = (SANE_Word) s->data.maintenance.nregx_book;

  /* OPT_NREGY_BOOK: */
  DBG (DBG_sane_option, "OPT_NREGY_BOOK\n");
  s->val[OPT_NREGY_BOOK].w = (SANE_Word) s->data.maintenance.nregy_book;

  /* OPT_NSCANS_ADF: */
  DBG (DBG_sane_option, "OPT_NSCANS_ADF\n");
  s->val[OPT_NSCANS_ADF].w =
    (SANE_Word) _4btol (&(s->data.maintenance.nscans_adf[0]));

  /* OPT_NSCANS_BOOK: */
  DBG (DBG_sane_option, "OPT_NSCANS_BOOK\n");
  s->val[OPT_NSCANS_BOOK].w =
    (SANE_Word) _4btol (&(s->data.maintenance.nscans_book[0]));

  /* OPT_LAMP_TIME: */
  DBG (DBG_sane_option, "OPT_LAMP_TIME\n");
  s->val[OPT_LAMP_TIME].w =
    (SANE_Word) _4btol (&(s->data.maintenance.lamp_time[0]));

  /* OPT_EO_ODD: */
  DBG (DBG_sane_option, "OPT_EO_ODD\n");
  s->val[OPT_EO_ODD].w = (SANE_Word) s->data.maintenance.eo_odd;

  /* OPT_EO_EVEN: */
  DBG (DBG_sane_option, "OPT_EO_EVEN\n");
  s->val[OPT_EO_EVEN].w = (SANE_Word) s->data.maintenance.eo_even;

  /* OPT_BLACK_LEVEL_ODD: */
  DBG (DBG_sane_option, "OPT_BLACK_LEVEL_ODD\n");
  s->val[OPT_BLACK_LEVEL_ODD].w =
    (SANE_Word) s->data.maintenance.black_level_odd;

  /* OPT_BLACK_LEVEL_EVEN: */
  DBG (DBG_sane_option, "OPT_BLACK_LEVEL_EVEN\n");
  s->val[OPT_BLACK_LEVEL_EVEN].w =
    (SANE_Word) s->data.maintenance.black_level_even;

  /* OPT_WHITE_LEVEL_ODD: */
  DBG (DBG_sane_option, "OPT_WHITE_LEVEL_ODD\n");
  s->val[OPT_WHITE_LEVEL_ODD].w =
    (SANE_Word) _2btol (&(s->data.maintenance.white_level_odd[0]));

  /* OPT_WHITE_LEVEL_EVEN: */
  DBG (DBG_sane_option, "OPT_WHITE_LEVEL_EVEN\n");
  s->val[OPT_WHITE_LEVEL_EVEN].w =
    (SANE_Word) _2btol (&(s->data.maintenance.white_level_even[0]));

  /* OPT_FIRST_ADJ_WHITE_ODD: */
  DBG (DBG_sane_option, "OPT_FIRST_ADJ_WHITE_ODD\n");
  s->val[OPT_FIRST_ADJ_WHITE_ODD].w =
    (SANE_Word) _2btol (&(s->data.maintenance.first_adj_white_odd[0]));

  /* OPT_FIRST_ADJ_WHITE_EVEN: */
  DBG (DBG_sane_option, "OPT_FIRST_ADJ_WHITE_EVEN\n");
  s->val[OPT_FIRST_ADJ_WHITE_EVEN].w =
    (SANE_Word) _2btol (&(s->data.maintenance.first_adj_white_even[0]));

  /* OPT_DENSITY: */
  DBG (DBG_sane_option, "OPT_DENSITY\n");
  s->val[OPT_DENSITY].w = (SANE_Word) s->data.maintenance.density_adj;

  /* OPT_NREGX_REVERSE: */
  DBG (DBG_sane_option, "OPT_NREGX_REVERSE\n");
  s->val[OPT_NREGX_REVERSE].w = (SANE_Word) s->data.maintenance.nregx_reverse;

  /* OPT_NREGY_REVERSE: */
  DBG (DBG_sane_option, "OPT_NREGY_REVERSE\n");
  s->val[OPT_NREGY_REVERSE].w = (SANE_Word) s->data.maintenance.nregy_reverse;

  /* OPT_NSCANS_REVERSE_ADF: */
  DBG (DBG_sane_option, "OPT_NSCANS_REVERSE_ADF\n");
  s->val[OPT_NSCANS_REVERSE_ADF].w =
    (SANE_Word) _4btol (&(s->data.maintenance.nscans_reverse_adf[0]));

  /* OPT_REVERSE_TIME: */
  DBG (DBG_sane_option, "OPT_REVERSE_TIME\n");
  s->val[OPT_REVERSE_TIME].w =
    (SANE_Word) _4btol (&(s->data.maintenance.reverse_time[0]));

  /* OPT_NCHARS: */
  DBG (DBG_sane_option, "OPT_NCHARS\n");
  s->val[OPT_NCHARS].w =
    (SANE_Word) _4btol (&(s->data.maintenance.nchars[0]));

  DBG (DBG_proc, "<< update_hs2p_data\n");
  return SANE_STATUS_GOOD;
}

static SANE_Status
hs2p_open (HS2P_Scanner * s)
{
  SANE_Status status;
  DBG (DBG_proc, ">> hs2p_open\n");
  DBG (DBG_info, ">> hs2p_open: trying to open: name=\"%s\" fd=%d\n",
       s->hw->sane.name, s->fd);
  if ((status =
       sanei_scsi_open (s->hw->sane.name, &s->fd, &sense_handler,
			&(s->hw->sense_data))) != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sane_start: open of %s failed: %d %s\n",
	   s->hw->sane.name, status, sane_strstatus (status));
      return (status);
    }
  DBG (DBG_info, ">>hs2p_open: OPENED \"%s\" fd=%d\n", s->hw->sane.name,
       s->fd);

  if ((status = test_unit_ready (s->fd)) != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "hs2p_open: test_unit_ready() failed: %s\n",
	   sane_strstatus (status));
      sanei_scsi_close (s->fd);
      s->fd = -1;
      return status;
    }
  DBG (DBG_proc, "<< hs2p_open\n");
  return SANE_STATUS_GOOD;
}

static SANE_Status
hs2p_close (HS2P_Scanner * s)
{

  DBG (DBG_proc, ">> hs2p_close\n");

  release_unit (s->fd);
  sanei_scsi_close (s->fd);
  s->fd = -1;

  DBG (DBG_proc, "<< hs2p_close\n");
  return SANE_STATUS_GOOD;
}

#include <stdarg.h>
static SANE_Status
get_hs2p_data (HS2P_Scanner * s, ...)
{
  SANE_Status status;
  SANE_Byte *buf;
  size_t *len = &(s->data.bufsize);
  int dtc, fd = s->fd;
  u_long dtq = 0;		/* two bytes */
  va_list ap;

  DBG (DBG_proc, ">> get_hs2p_data\n");
  if (fd < 0)
    {
      status = hs2p_open (s);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "get_hs2p_data: error opening scanner: %s\n",
	       sane_strstatus (status));
	  return status;
	}
    }

  for (va_start (ap, s), dtc = va_arg (ap, int); dtc != DATA_TYPE_EOL;
       dtc = va_arg (ap, int))
    {
      DBG (DBG_proc, ">> get_hs2p_data 0x%2.2x\n", (int) dtc);
      switch (dtc)
	{
	case DATA_TYPE_GAMMA:
	  buf = &(s->data.gamma[0]);
	  *len = sizeof (s->data.gamma);
	  break;
	case DATA_TYPE_ENDORSER:
	  buf = &(s->data.endorser[0]);
	  *len = sizeof (s->data.endorser);
	  break;
	case DATA_TYPE_SIZE:
	  buf = &(s->data.size);
	  *len = sizeof (s->data.size);
	  break;
	case DATA_TYPE_PAGE_LEN:
	  buf = s->data.nlines;
	  *len = sizeof (s->data.nlines);
	  break;
	case DATA_TYPE_MAINTENANCE:
	  buf = (SANE_Byte *) & (s->data.maintenance);
	  *len = sizeof (s->data.maintenance);
	  break;
	case DATA_TYPE_ADF_STATUS:
	  buf = &(s->data.adf_status);
	  *len = sizeof (s->data.adf_status);
	  break;
	case DATA_TYPE_IMAGE:
	case DATA_TYPE_HALFTONE:
	default:
	  DBG (DBG_info, "Data Type Code %2.2x not handled.\n", dtc);
	  return SANE_STATUS_INVAL;
	}
      DBG (DBG_info,
	   "get_hs2p_data calling read_data for dtc=%2.2x and bufsize=%lu\n",
	   (int) dtc, (u_long) * len);
      status = read_data (s->fd, buf, len, (SANE_Byte) dtc, dtq);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "get_scanner_data: ERROR %s\n",
	       sane_strstatus (status));
	}
    }
  va_end (ap);

  if (fd < 0)
    {				/* need to return fd to original state */
      status = hs2p_close (s);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "get_hs2p_data: error closing fd: %s\n",
	       sane_strstatus (status));
	}
    }
  DBG (DBG_proc, "<< get_hs2p_data: %d\n", status);
  return (status);
}

static SANE_Status
print_maintenance_data (MAINTENANCE_DATA * d)
{
  DBG (DBG_proc, ">> print_maintenance_data: \n");

  DBG (DBG_LEVEL, "nregx_adf = %d\n", d->nregx_adf);
  DBG (DBG_LEVEL, "nregy_adf = %d\n", d->nregy_adf);

  DBG (DBG_LEVEL, "nregx_book = %d\n", d->nregx_book);
  DBG (DBG_LEVEL, "nregy_book = %d\n", d->nregy_book);

  DBG (DBG_LEVEL, "nscans_adf = %lu\n", _4btol (&(d->nscans_adf[0])));
  DBG (DBG_LEVEL, "nscans_adf = %lu\n", _4btol (&(d->nscans_adf[0])));

  DBG (DBG_LEVEL, "lamp time = %lu\n", _4btol (&(d->lamp_time[0])));

  DBG (DBG_LEVEL, "eo_odd = %d\n", d->eo_odd);
  DBG (DBG_LEVEL, "eo_even = %d\n", d->eo_even);

  DBG (DBG_LEVEL, "black_level_odd = %d\n", d->black_level_odd);
  DBG (DBG_LEVEL, "black_level_even = %d\n", d->black_level_even);

  DBG (DBG_LEVEL, "white_level_odd = %lu\n",
       _2btol (&(d->white_level_odd[0])));
  DBG (DBG_LEVEL, "white_level_even = %lu\n",
       _2btol (&(d->white_level_even[0])));

  DBG (DBG_LEVEL, "first_adj_white_odd = %lu\n",
       _2btol (&(d->first_adj_white_odd[0])));
  DBG (DBG_LEVEL, "first_adj_white_even = %lu\n",
       _2btol (&(d->first_adj_white_even[0])));

  DBG (DBG_LEVEL, "density_adj = %d\n", d->density_adj);

  DBG (DBG_LEVEL, "nregx_reverse = %d\n", d->nregx_reverse);
  DBG (DBG_LEVEL, "nregy_reverse = %d\n", d->nregy_reverse);

  DBG (DBG_LEVEL, "nscans_reverse_adf = %lu\n",
       _4btol (&(d->nscans_reverse_adf[0])));

  DBG (DBG_LEVEL, "reverse_time = %lu\n", _4btol (&(d->reverse_time[0])));

  DBG (DBG_LEVEL, "nchars = %lu\n", _4btol (&(d->nchars[0])));

  DBG (DBG_proc, "<< print_maintenance_data: \n");
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  HS2P_Scanner *s = handle;
  SANE_Status status;
  SANE_Word cap;
  SANE_String_Const name;
  SANE_Int paper_id;



  name = s->opt[option].name ? s->opt[option].name : "(nil)";
  if (info)
    *info = 0;
  DBG (DBG_proc, ">> sane_control_option: %s option=%d name=%s\n",
       action == SANE_ACTION_GET_VALUE ? "SET" : "GET", option, name);

  if (s->scanning)
    return (SANE_STATUS_DEVICE_BUSY);
  if (option >= NUM_OPTIONS)
    return (SANE_STATUS_INVAL);

  cap = s->opt[option].cap;
  if (!SANE_OPTION_IS_ACTIVE (cap))
    return (SANE_STATUS_INVAL);

  if (action == SANE_ACTION_GET_VALUE)
    {
      DBG (DBG_proc, "sane_control_option get_value option=%d\n", option);
      switch (option)
	{
	  /* word options: */
	case OPT_RESOLUTION:
	case OPT_X_RESOLUTION:
	case OPT_Y_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_BRIGHTNESS:
	case OPT_THRESHOLD:
	case OPT_CONTRAST:
	case OPT_NUM_OPTS:
	  *(SANE_Word *) val = s->val[option].w;
	  return (SANE_STATUS_GOOD);

	  /* bool options: */
	  /*case OPT_AUTOBORDER: case OPT_DESKEW: case OPT_CHECK_ADF: case OPT_BATCH: */
	case OPT_PREVIEW:
	case OPT_SCAN_WAIT_MODE:
	case OPT_DUPLEX:
	case OPT_AUTO_SIZE:
	case OPT_NEGATIVE:
	case OPT_ENDORSER:
	case OPT_SMOOTHING:
	case OPT_WHITE_BALANCE:
	case OPT_PREFEED:
	case OPT_CUSTOM_GAMMA:
	case OPT_PADDING:
	  *(SANE_Bool *) val = s->val[option].w;
	  return (SANE_STATUS_GOOD);

	  /* string options: */
	  /* case OPT_ADF:      */
	  /* case OPT_BITORDER: */
	  /* case OPT_ROTATION  */
	  /* case OPT_SECTION:  */
	case OPT_INQUIRY:
	case OPT_SCAN_SOURCE:
	case OPT_PAGE_ORIENTATION:
	case OPT_PAPER_SIZE:
	case OPT_SCAN_MODE:
	case OPT_ENDORSER_STRING:
	case OPT_COMPRESSION:
	case OPT_NOISEREMOVAL:
	case OPT_GRAYFILTER:
	case OPT_HALFTONE_CODE:
	case OPT_HALFTONE_PATTERN:
	case OPT_GAMMA:
	case OPT_AUTOSEP:
	case OPT_AUTOBIN:
	case OPT_PADDING_TYPE:
	  DBG (DBG_proc, "STRING=%s\n", s->val[option].s);
	  strcpy (val, s->val[option].s);
	  return (SANE_STATUS_GOOD);
	  DBG (DBG_proc, "sizeof(val)=%lu sizeof(s)=%lu\n",
	       (u_long) sizeof (val), (u_long) sizeof (s->val[option].s));
	  return (SANE_STATUS_GOOD);

	  /* gamma */
	case OPT_GAMMA_VECTOR_GRAY:
	  memcpy (val, s->val[option].wa, s->opt[option].size);
	  return SANE_STATUS_GOOD;

	  /* MAINTENANCE DATA */
	case OPT_DATA_GROUP:
	case OPT_UPDATE:
	  return SANE_STATUS_GOOD;
	case OPT_NREGX_ADF:
	  DBG (DBG_sane_option, "OPT_NREGX_ADF\n");
	  *(SANE_Word *) val = (SANE_Word) s->data.maintenance.nregx_adf;
	  return SANE_STATUS_GOOD;
	case OPT_NREGY_ADF:
	  DBG (DBG_sane_option, "OPT_NREGY_ADF\n");
	  *(SANE_Word *) val = (SANE_Word) s->data.maintenance.nregx_book;
	  return SANE_STATUS_GOOD;
	case OPT_NREGX_BOOK:
	  DBG (DBG_sane_option, "OPT_NREGX_BOOK\n");
	  *(SANE_Word *) val = (SANE_Word) s->data.maintenance.nregx_book;
	  return SANE_STATUS_GOOD;
	case OPT_NREGY_BOOK:
	  DBG (DBG_sane_option, "OPT_NREGY_BOOK\n");
	  *(SANE_Word *) val = (SANE_Word) s->data.maintenance.nregy_book;
	  return SANE_STATUS_GOOD;
	case OPT_NSCANS_ADF:
	  DBG (DBG_sane_option, "OPT_NSCANS_ADF\n");
	  *(SANE_Word *) val =
	    (SANE_Word) _4btol (&(s->data.maintenance.nscans_adf[0]));
	  return SANE_STATUS_GOOD;
	case OPT_NSCANS_BOOK:
	  DBG (DBG_sane_option, "OPT_NSCANS_BOOK\n");
	  *(SANE_Word *) val =
	    (SANE_Word) _4btol (&(s->data.maintenance.nscans_book[0]));
	  return SANE_STATUS_GOOD;
	case OPT_LAMP_TIME:
	  DBG (DBG_sane_option, "OPT_LAMP_TIME\n");
	  *(SANE_Word *) val =
	    (SANE_Word) _4btol (&(s->data.maintenance.lamp_time[0]));
	  return SANE_STATUS_GOOD;
	case OPT_EO_ODD:
	  DBG (DBG_sane_option, "OPT_EO_ODD\n");
	  *(SANE_Word *) val = (SANE_Word) s->data.maintenance.eo_odd;
	  return SANE_STATUS_GOOD;
	case OPT_EO_EVEN:
	  DBG (DBG_sane_option, "OPT_EO_EVEN\n");
	  *(SANE_Word *) val = (SANE_Word) s->data.maintenance.eo_even;
	  return SANE_STATUS_GOOD;
	case OPT_BLACK_LEVEL_ODD:
	  DBG (DBG_sane_option, "OPT_BLACK_LEVEL_ODD\n");
	  *(SANE_Word *) val =
	    (SANE_Word) s->data.maintenance.black_level_odd;
	  return SANE_STATUS_GOOD;
	case OPT_BLACK_LEVEL_EVEN:
	  DBG (DBG_sane_option, "OPT_BLACK_LEVEL_EVEN\n");
	  *(SANE_Word *) val =
	    (SANE_Word) s->data.maintenance.black_level_even;
	  return SANE_STATUS_GOOD;
	case OPT_WHITE_LEVEL_ODD:
	  DBG (DBG_sane_option, "OPT_WHITE_LEVEL_ODD\n");
	  *(SANE_Word *) val =
	    (SANE_Word) _2btol (&(s->data.maintenance.white_level_odd[0]));
	  return SANE_STATUS_GOOD;
	case OPT_WHITE_LEVEL_EVEN:
	  DBG (DBG_sane_option, "OPT_WHITE_LEVEL_EVEN\n");
	  *(SANE_Word *) val =
	    (SANE_Word) _2btol (&(s->data.maintenance.white_level_even[0]));
	  return SANE_STATUS_GOOD;
	case OPT_FIRST_ADJ_WHITE_ODD:
	  DBG (DBG_sane_option, "OPT_FIRST_ADJ_WHITE_ODD\n");
	  *(SANE_Word *) val =
	    (SANE_Word)
	    _2btol (&(s->data.maintenance.first_adj_white_odd[0]));
	  return SANE_STATUS_GOOD;
	case OPT_FIRST_ADJ_WHITE_EVEN:
	  DBG (DBG_sane_option, "OPT_FIRST_ADJ_WHITE_EVEN\n");
	  *(SANE_Word *) val =
	    (SANE_Word)
	    _2btol (&(s->data.maintenance.first_adj_white_even[0]));
	  return SANE_STATUS_GOOD;
	case OPT_DENSITY:
	  DBG (DBG_sane_option, "OPT_DENSITY\n");
	  *(SANE_Word *) val = (SANE_Word) s->data.maintenance.density_adj;
	  return SANE_STATUS_GOOD;
	case OPT_NREGX_REVERSE:
	  DBG (DBG_sane_option, "OPT_NREGX_REVERSE\n");
	  *(SANE_Word *) val = (SANE_Word) s->data.maintenance.nregx_reverse;
	  return SANE_STATUS_GOOD;
	case OPT_NREGY_REVERSE:
	  DBG (DBG_sane_option, "OPT_NREGY_REVERSE\n");
	  *(SANE_Word *) val = (SANE_Word) s->data.maintenance.nregy_reverse;
	  return SANE_STATUS_GOOD;
	case OPT_NSCANS_REVERSE_ADF:
	  DBG (DBG_sane_option, "OPT_NSCANS_REVERSE_ADF\n");
	  *(SANE_Word *) val =
	    (SANE_Word) _4btol (&(s->data.maintenance.nscans_reverse_adf[0]));
	  return SANE_STATUS_GOOD;
	case OPT_REVERSE_TIME:
	  DBG (DBG_sane_option, "OPT_REVERSE_TIME\n");
	  *(SANE_Word *) val =
	    (SANE_Word) _4btol (&(s->data.maintenance.reverse_time[0]));
	  return SANE_STATUS_GOOD;
	case OPT_NCHARS:
	  DBG (DBG_sane_option, "OPT_NCHARS\n");
	  *(SANE_Word *) val =
	    (SANE_Word) _4btol (&(s->data.maintenance.nchars[0]));
	  return (SANE_STATUS_GOOD);

	default:
	  DBG (DBG_proc, "sane_control_option:invalid option number %d\n",
	       option);
	  return SANE_STATUS_INVAL;
	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      DBG (DBG_proc, "sane_control_option set_value\n");
      switch (s->opt[option].type)
	{
	case SANE_TYPE_BOOL:
	case SANE_TYPE_INT:
	  DBG (DBG_proc, "sane_control_option: set_value %s [#%d] to %d\n",
	       name, option, *(SANE_Word *) val);
	  break;
	case SANE_TYPE_FIXED:
	  DBG (DBG_proc, "sane_control_option: set_value %s [#%d] to %f\n",
	       name, option, SANE_UNFIX (*(SANE_Word *) val));
	  break;
	case SANE_TYPE_STRING:
	  DBG (DBG_proc, "sane_control_option: set_value %s [#%d] to %s\n",
	       name, option, (char *) val);
	  break;
	case SANE_TYPE_BUTTON:
	  DBG (DBG_proc, "sane_control_option: set_value %s [#%d]\n",
	       name, option);
	  update_hs2p_data (s);
	  break;
	default:
	  DBG (DBG_proc, "sane_control_option: set_value %s [#%d]\n", name,
	       option);
	}

      if (!SANE_OPTION_IS_SETTABLE (cap))
	return (SANE_STATUS_INVAL);
      if ((status =
	   sanei_constrain_value (s->opt + option, val,
				  info)) != SANE_STATUS_GOOD)
	return status;

      switch (option)
	{
	  /* (mostly) side-effect-free word options: */
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	  s->opt[OPT_AUTO_SIZE].cap |= SANE_CAP_INACTIVE;	/* disable auto size */
	  /* make sure that paper-size is set to custom */
	  if (info && s->val[option].w != *(SANE_Word *) val)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  s->val[option].w = *(SANE_Word *) val;
	  /* resets the paper format to user defined */
	  if (strcmp (s->val[OPT_PAPER_SIZE].s, paper_list[0]) != 0)	/* CUSTOM PAPER SIZE */
	    {
	      if (info)
		*info |= SANE_INFO_RELOAD_OPTIONS;
	      if (s->val[OPT_PAPER_SIZE].s)
		free (s->val[OPT_PAPER_SIZE].s);
	      s->val[OPT_PAPER_SIZE].s = strdup (paper_list[0]);	/* CUSTOM PAPER SIZE */
	    }
	  /* fall through */
	case OPT_X_RESOLUTION:
	case OPT_Y_RESOLUTION:
	  if (info && s->val[option].w != *(SANE_Word *) val)
	    *info |= SANE_INFO_RELOAD_PARAMS;

	  /* fall through */
	  /*case OPT_ACE_FUNCTION: case OPT_ACE_SENSITIVITY: */
	case OPT_BRIGHTNESS:
	case OPT_THRESHOLD:
	case OPT_CONTRAST:
	case OPT_NUM_OPTS:
	  s->val[option].w = *(SANE_Word *) val;
	  return (SANE_STATUS_GOOD);

	  /* string options */
	case OPT_NOISEREMOVAL:
	case OPT_AUTOSEP:
	case OPT_AUTOBIN:
	case OPT_COMPRESSION:
	case OPT_PADDING_TYPE:
	case OPT_GRAYFILTER:
	case OPT_HALFTONE_CODE:
	case OPT_HALFTONE_PATTERN:
	case OPT_ENDORSER_STRING:
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);
	  return SANE_STATUS_GOOD;

	  /* boolean options: */
	case OPT_PREVIEW:
	case OPT_DUPLEX:
	case OPT_NEGATIVE:
	case OPT_SCAN_WAIT_MODE:
	case OPT_ENDORSER:
	case OPT_SMOOTHING:
	case OPT_WHITE_BALANCE:
	case OPT_PREFEED:
	case OPT_PADDING:
	  s->val[option].w = *(SANE_Word *) val;
	  return SANE_STATUS_GOOD;

	case OPT_GAMMA_VECTOR_GRAY:
	  memcpy (s->val[option].wa, val, s->opt[option].size);
	  return SANE_STATUS_GOOD;

	  /* options with side effect */
	case OPT_GAMMA:
	  if (strcmp (s->val[option].s, (SANE_String) val))
	    {
	      if (!strcmp ((SANE_String) val, "User"))
		{
		  s->val[OPT_CUSTOM_GAMMA].b = SANE_TRUE;
		  s->opt[OPT_CUSTOM_GAMMA].cap &= ~SANE_CAP_INACTIVE;
		  /* Brightness and Contrast do not work when downloading Gamma Table */
		  s->opt[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
		  s->opt[OPT_CONTRAST].cap |= SANE_CAP_INACTIVE;
		}
	      else
		{
		  s->val[OPT_CUSTOM_GAMMA].b = SANE_FALSE;
		  s->opt[OPT_CUSTOM_GAMMA].cap |= SANE_CAP_INACTIVE;
		  s->opt[OPT_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_CONTRAST].cap &= ~SANE_CAP_INACTIVE;
		}
	      if (info)
		*info |= SANE_INFO_RELOAD_OPTIONS;
	    }
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);
	  return SANE_STATUS_GOOD;

	case OPT_CUSTOM_GAMMA:
	  s->val[OPT_CUSTOM_GAMMA].w = *(SANE_Word *) val;
	  if (s->val[OPT_CUSTOM_GAMMA].w)
	    {
	      s->opt[OPT_GAMMA_VECTOR_GRAY].cap &= ~SANE_CAP_INACTIVE;
	    }
	  else
	    {
	      s->opt[OPT_GAMMA_VECTOR_GRAY].cap |= SANE_CAP_INACTIVE;
	    }
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;
	  return SANE_STATUS_GOOD;

	case OPT_RESOLUTION:
	  if (info && s->val[option].w != *(SANE_Word *) val)
	    *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
	  s->val[option].w = *(SANE_Word *) val;
	  s->val[OPT_X_RESOLUTION].w = *(SANE_Word *) val;
	  s->val[OPT_Y_RESOLUTION].w = *(SANE_Word *) val;
	  return SANE_STATUS_GOOD;
	case OPT_SCAN_SOURCE:
	  /* a string option */
	  /*    Since the scanner ejects the sheet in ADF mode 
	   * it is impossible to scan multiple sections in one document
	   *    In ADF mode, because of mechanical limitations:
	   * the minimum document size is (x,y)=(69mm x 120mm)
	   */
	  if (info && strcmp ((char *) s->val[option].s, (char *) val))
	    *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);
	  if (!strcmp ("ADF", (SANE_String) val))
	    {
	      s->opt[OPT_ENDORSER].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[OPT_ENDORSER_STRING].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[OPT_PREFEED].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[OPT_DUPLEX].cap &= ~SANE_CAP_INACTIVE;
	      /*s->opt[OPT_PADDING].cap &= ~SANE_CAP_INACTIVE; */
	    }
	  else
	    {			/* Flatbed */
	      s->opt[OPT_ENDORSER].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_ENDORSER_STRING].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_PREFEED].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_DUPLEX].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_PADDING].cap |= SANE_CAP_INACTIVE;
	    }
	  return SANE_STATUS_GOOD;
	case OPT_SCAN_MODE:
	  /* a string option */
	  /* scan mode != lineart disables compression, setting it to  'none' */
	  if (strcmp (s->val[option].s, (SANE_String) val))
	    {
	      if (info)
		*info |= SANE_INFO_RELOAD_OPTIONS;
	      if (!strcmp (SM_LINEART, (SANE_String) val))
		{
		  s->image_composition = LINEART;
		  s->opt[OPT_COMPRESSION].cap &= ~SANE_CAP_INACTIVE;	/* enable compression control */
		  s->opt[OPT_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;	/* enable threshold control   */
		  s->opt[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;	/* disable brightness control */
		  s->opt[OPT_CONTRAST].cap |= SANE_CAP_INACTIVE;	/* disable contrast control   */
		  s->opt[OPT_GAMMA].cap |= SANE_CAP_INACTIVE;	/* disable gamma              */
		  s->opt[OPT_CUSTOM_GAMMA].cap |= SANE_CAP_INACTIVE;	/* disable gamma              */
		  s->opt[OPT_GAMMA_VECTOR_GRAY].cap |= SANE_CAP_INACTIVE;	/* disable gamma              */
		  s->opt[OPT_HALFTONE_CODE].cap |= SANE_CAP_INACTIVE;	/* disable halftone code      */
		  s->opt[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;	/* disable halftone pattern   */
		}
	      else
		{
		  if (!strcmp (SM_HALFTONE, (SANE_String) val))
		    {
		      s->image_composition = HALFTONE;
		      s->opt[OPT_HALFTONE_CODE].cap &= ~SANE_CAP_INACTIVE;	/* enable halftone code    */
		      s->opt[OPT_HALFTONE_PATTERN].cap &= ~SANE_CAP_INACTIVE;	/* enable halftone pattern */
		    }
		  else if (!strcmp (SM_4BITGRAY, (SANE_String) val) ||
			   !strcmp (SM_6BITGRAY, (SANE_String) val) ||
			   !strcmp (SM_8BITGRAY, (SANE_String) val))
		    {
		      s->image_composition = GRAYSCALE;
		      s->opt[OPT_GAMMA].cap &= ~SANE_CAP_INACTIVE;	/* enable  gamma            */
		      s->opt[OPT_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;	/* enable  brightness       */
		      s->opt[OPT_CONTRAST].cap &= ~SANE_CAP_INACTIVE;	/* enable  contrast         */
		      s->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;	/* disable threshold        */
		      s->opt[OPT_COMPRESSION].cap |= SANE_CAP_INACTIVE;	/* disable compression      */
		      s->opt[OPT_HALFTONE_CODE].cap |= SANE_CAP_INACTIVE;	/* disable halftone code    */
		      s->opt[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;	/* disable halftone pattern */
		      if (s->val[OPT_COMPRESSION].s
			  && get_compression_id (s->val[OPT_COMPRESSION].s) !=
			  0)
			{
			  free (s->val[OPT_COMPRESSION].s);
			  s->val[OPT_COMPRESSION].s =
			    strdup (compression_list[0]);
			}
		    }
		}
	      free (s->val[option].s);
	      s->val[option].s = strdup (val);
	    }
	  return SANE_STATUS_GOOD;

	case OPT_PAGE_ORIENTATION:
	  if (strcmp (s->val[option].s, (SANE_String) val))
	    {
	      free (s->val[option].s);
	      s->val[option].s = strdup (val);
	      if (info)
		*info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
	    }
	  /* set val to current selected paper size */
	  paper_id = get_paper_id ((SANE_String) s->val[OPT_PAPER_SIZE].s);
	  goto paper_id;
	case OPT_PAPER_SIZE:
	  /* a string option */
	  /* changes geometry options, therefore _RELOAD_PARAMS and _RELOAD_OPTIONS */
	  s->opt[OPT_AUTO_SIZE].cap |= SANE_CAP_INACTIVE;	/* disable auto size */
	  if (strcmp (s->val[option].s, (SANE_String) val))
	    {
	      paper_id = get_paper_id ((SANE_String) val);

	      /* paper_id 0 is a special case (custom) that
	       * disables the paper size control of geometry
	       */
	    paper_id:
	      if (paper_id != 0)
		{
		  double x_max, y_max, x, y, temp;

		  x_max = SANE_UNFIX (s->hw->info.x_range.max);
		  y_max = SANE_UNFIX (s->hw->info.y_range.max);

		  /* a dimension of 0.0 (or less) is replaced with the max value */
		  x = (paper_sizes[paper_id].width <= 0.0) ? x_max :
		    paper_sizes[paper_id].width;
		  y = (paper_sizes[paper_id].length <= 0.0) ? y_max :
		    paper_sizes[paper_id].length;

		  if (info)
		    *info |=
		      SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;

		  if (!strcmp (s->val[OPT_PAGE_ORIENTATION].s, LANDSCAPE))	/* swap */
		    {
		      temp = y_max;
		      y_max = x_max;
		      x_max = temp;
		      temp = y;
		      y = x;
		      x = temp;
		    }

		  s->val[OPT_TL_X].w = SANE_FIX (0.0);
		  s->val[OPT_TL_Y].w = SANE_FIX (0.0);
		  s->val[OPT_BR_X].w = SANE_FIX (MIN (x, x_max));
		  s->val[OPT_BR_Y].w = SANE_FIX (MIN (y, y_max));
		}
	      free (s->val[option].s);
	      s->val[option].s = strdup (val);
	    }
	  return SANE_STATUS_GOOD;
	case OPT_UPDATE:	/* SANE_TYPE_BUTTON */
	  DBG (DBG_info,
	       "OPT_UPDATE: ready to call get_hs2p_data: fd=%d\n", s->fd);
	  get_hs2p_data (s,
			 /* DATA_TYPE_GAMMA, */
			 /* DATA_TYPE_ENDORSER, */
			 /* DATA_TYPE_SIZE, */
			 /* DATA_TYPE_PAGE_LEN, */
			 DATA_TYPE_MAINTENANCE,
			 /* DATA_TYPE_ADF_STATUS, */
			 /* DATA_TYPE_IMAGE, */
			 /* DATA_TYPE_HALFTONE, */
			 DATA_TYPE_EOL);	/* va_list end */
	  update_hs2p_data (s);
	  if (DBG_LEVEL >= DBG_info)
	    print_maintenance_data (&(s->data.maintenance));
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
	  return SANE_STATUS_GOOD;
	}
      return (SANE_STATUS_GOOD);
    }

  DBG (DBG_proc, "<< sane_control_option\n");
  return (SANE_STATUS_INVAL);

}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  HS2P_Scanner *s = handle;
  DBG (DBG_proc, ">> sane_get_parameters\n");

  if (!s->scanning)
    {
      int width, length, xres, yres;
      const char *mode;

      memset (&s->params, 0, sizeof (s->params));	/* CLEAR SANE_Parameters */

      width =
	(int) (SANE_UNFIX (s->val[OPT_BR_X].w) -
	       SANE_UNFIX (s->val[OPT_TL_X].w));
      length =
	(int) (SANE_UNFIX (s->val[OPT_BR_Y].w) -
	       SANE_UNFIX (s->val[OPT_TL_Y].w));
      xres = s->val[OPT_X_RESOLUTION].w;
      yres = s->val[OPT_Y_RESOLUTION].w;
      DBG (DBG_proc,
	   ">>sane_get_parameters: (W/L)=(%d/%d) (xres/yres)=(%d/%d) mud=%d\n",
	   width, length, xres, yres, s->hw->info.mud);

      /* make best-effort guess at what parameters will look like once scanning starts.  */
      if (xres > 0 && yres > 0 && width > 0 && length > 0)
	{			/* convert from mm to pixels */
	  s->params.pixels_per_line =
	    width * xres / s->hw->info.mud / MM_PER_INCH;
	  s->params.lines = length * yres / s->hw->info.mud / MM_PER_INCH;
	}

      mode = s->val[OPT_SCAN_MODE].s;
      if ((strcmp (mode, SM_LINEART) == 0) ||
	  (strcmp (mode, SM_HALFTONE)) == 0)
	{
	  s->params.format = SANE_FRAME_GRAY;
	  s->params.bytes_per_line = s->params.pixels_per_line / 8;
	  /* if the scanner truncates to the byte boundary, so: chop! */
	  s->params.pixels_per_line = s->params.bytes_per_line * 8;
	  s->params.depth = 1;
	}
      else			/* if (strcmp (mode, SANE_VALUE_SCAN_MODE_GRAY) == 0) */
	{
	  s->params.format = SANE_FRAME_GRAY;
	  s->params.bytes_per_line = s->params.pixels_per_line;
	  s->params.depth = 8;
	}
      s->params.last_frame = SANE_TRUE;
    }
  else
    DBG (DBG_proc, "sane_get_parameters: scanning, so can't get params\n");

  if (params)
    *params = s->params;

  DBG (DBG_proc,
       "%d pixels per line, %d bytes per line, %d lines high, total %lu bytes, "
       "dpi=%ld\n", s->params.pixels_per_line, s->params.bytes_per_line,
       s->params.lines, (u_long) s->bytes_to_read,
       (long) SANE_UNFIX (s->val[OPT_Y_RESOLUTION].w));

  DBG (DBG_proc, "<< sane_get_parameters\n");
  return (SANE_STATUS_GOOD);
}

static SANE_Status
set_window_data (HS2P_Scanner * s, SWD * wbuf)
{
  struct hs2p_window_data *data;
  int i, nwin, id, xres, yres, xmax, ymax;
  long ulx, uly, width, length, number, bytes;
  double offset;

  DBG (DBG_proc, ">> set_window_data: sizeof(*wbuf)=%lu; window len=%lu\n",
       (u_long) sizeof (*wbuf), (u_long) sizeof (wbuf->data));

  /* initialize our window buffer with zeros */
  DBG (DBG_proc, ">> set_window_data: CLEARING wbuf\n");
  memset (wbuf, 0, sizeof (*wbuf));

  /* Header */
  DBG (DBG_proc,
       ">> set_window_data: writing Window Descriptor Length =%lu\n",
       (u_long) sizeof (wbuf->data));
  _lto2b (sizeof (wbuf->data), &wbuf->hdr.len[0]);

  /* X-Axis Resolution 100-800dpi in 1 dpi steps */
  xres = s->val[OPT_X_RESOLUTION].w;
  if (xres < s->hw->info.resMinX || xres > s->hw->info.resMaxX)
    {
      DBG (DBG_error, "XRESOLUTION %d IS NOT WITHIN [%d, %d]\n", xres,
	   s->hw->info.resMinX, s->hw->info.resMaxX);
      return (SANE_STATUS_INVAL);
    }

  /* Y-Axis Resolution 100-800dpi in 1 dpi steps */
  yres = s->val[OPT_Y_RESOLUTION].w;
  if (yres < s->hw->info.resMinY || yres > s->hw->info.resMaxY)
    {
      DBG (DBG_error, "YRESOLUTION %d IS NOT WITHIN [%d, %d]\n", yres,
	   s->hw->info.resMinY, s->hw->info.resMaxY);
      return (SANE_STATUS_INVAL);
    }

  ulx = (long) SANE_UNFIX (s->val[OPT_TL_X].w);
  uly = (long) SANE_UNFIX (s->val[OPT_TL_Y].w);
  DBG (DBG_info, "set_window_data: upperleft=(%ld,%ld)\n", ulx, uly);

  width = (long) SANE_UNFIX (s->val[OPT_BR_X].w - s->val[OPT_TL_X].w);	/* Window Width */
  length = (long) SANE_UNFIX (s->val[OPT_BR_Y].w - s->val[OPT_TL_Y].w);	/* Window Length */
  DBG (DBG_info, "set_window_data: WxL= %ld x %ld\n", width, length);

  /* NOTE: the width in inches converted to byte unit must be the following values or less
   * Binary:       620 bytes
   * 4-bits gray: 2480 bytes
   * 8-bits gray: 4960 bytes
   */
  if (!strcmp (s->val[OPT_SCAN_MODE].s, SM_LINEART))
    {
      bytes = (width / MM_PER_INCH) * (s->val[OPT_X_RESOLUTION].w / 8.0);
      if (bytes > 620)
	{
	  DBG (DBG_error,
	       "width in pixels too large: width=%ld x-resolution=%d bytes=%ld\n",
	       width, s->val[OPT_X_RESOLUTION].w, bytes);
	  return (SANE_STATUS_INVAL);
	}
    }
  else if (!strcmp (s->val[OPT_SCAN_MODE].s, SM_4BITGRAY))
    {
      bytes = (width / MM_PER_INCH) * (s->val[OPT_X_RESOLUTION].w / 2.0);
      if (bytes > 2480)
	{
	  DBG (DBG_error,
	       "width in pixels too large: width=%ld x-resolution=%d bytes=%ld\n",
	       width, s->val[OPT_X_RESOLUTION].w, bytes);
	  return (SANE_STATUS_INVAL);
	}
    }
  else if (!strcmp (s->val[OPT_SCAN_MODE].s, SM_8BITGRAY))
    {
      bytes = (width / MM_PER_INCH) * (s->val[OPT_X_RESOLUTION].w);
      if (bytes > 4960)
	{
	  DBG (DBG_error,
	       "width in pixels too large: width=%ld x-resolution=%d bytes=%ld\n",
	       width, s->val[OPT_X_RESOLUTION].w, bytes);
	  return (SANE_STATUS_INVAL);
	}
    }


  if (strcmp (s->val[OPT_SCAN_SOURCE].s, scan_source_list[ADF]) == 0)
    {
      offset = (SANE_UNFIX (s->hw->info.x_range.max) - width) / 2.0;
      DBG (DBG_info, "set_window_data: ADF origin offset=%f\n", offset);

      ulx += (long) offset;
    }


  if (strcmp (s->val[OPT_SCAN_SOURCE].s, scan_source_list[FB]) == 0)
    {				/* FB */
      xmax = 298;		/*mm */
      ymax = 432;
    }
  else
    {				/* ADF */
      xmax = 298;
      ymax = 2000;
    }

  /* Boundary Conditions when BMU = MM */
  number = ulx + width;
  if (number <= 0 || number > xmax)
    {
      DBG (DBG_error, "NOT WITHIN BOUNDS: ulx=%ld width=%ld sum=%ld\n",
	   ulx, width, number);
      return (SANE_STATUS_INVAL);
    }
  number = uly + length;
  if (number <= 0 || number > ymax)
    {
      DBG (DBG_error, "NOT WITHIN BOUNDS: uly=%ld length=%ld sum=%ld\n",
	   uly, length, number);
      return (SANE_STATUS_INVAL);
    }



  /* For each window (up to 2 if we're duplexing) */
  nwin = (s->val[OPT_DUPLEX].w == SANE_TRUE) ? 2 : 1;
  for (i = 0; i < nwin; i++)
    {
      data = &(wbuf->data[i]);
      data->window_id = i;
      data->auto_bit &= 0xFE;	/* Auto bit set to 0 since auto function isn't supported */

      _lto2b (xres, &data->xres[0]);	/* Set X resolution */
      _lto2b (yres, &data->yres[0]);	/* Set Y resolution */

      _lto4b (ulx, &data->ulx[0]);	/* X-Axis Upper Left */
      _lto4b (uly, &data->uly[0]);	/* Y-Axis Upper Left */

      _lto4b (width, &data->width[0]);	/* Window Width */
      _lto4b (length, &data->length[0]);	/* Window Length */






      data->brightness = s->val[OPT_BRIGHTNESS].w;	/* black-white: 1-255; 0 is default 128 */
      data->threshold = s->val[OPT_THRESHOLD].w;	/* light-dark:  1-255; 0 is default 128 */
      data->contrast = s->val[OPT_CONTRAST].w;	/* low-high:    1-255: 0 is default 128 */
      if (data->brightness == 128)
	data->brightness = 0;
      if (data->threshold == 128)
	data->threshold = 0;
      if (data->contrast == 128)
	data->contrast = 0;

      data->image_composition = s->image_composition;
      data->bpp = s->bpp = s->params.depth;

      /* Byte 27, 347 Halftone Code: if HALFTONE, then either DITHER or ERROR_DIFFUSION */
      if (s->image_composition == HALFTONE)
	{			/* Then let's use pattern selected by user */
	  data->halftone_code =
	    (get_halftone_code_id (s->val[OPT_HALFTONE_CODE].s) ==
	     0) ? DITHER : ERROR_DIFFUSION;
	  data->halftone_id =
	    get_halftone_pattern_val (s->val[OPT_HALFTONE_PATTERN].s);
	}
      else
	{
	  data->halftone_code = DITHER;	/* 00H reserved */
	  data->halftone_id = 0x01;	/* 00H reserved */
	}



      /* Byte 29, 349: RIF:reserved:padding type */
      if (data->image_composition == LINEART
	  || data->image_composition == HALFTONE)
	{
	  if (s->val[OPT_NEGATIVE].w)
	    data->byte29 |= (1 << 7);	/* set bit 7 */
	  else
	    data->byte29 &= ~(1 << 7);	/* unset bit 7 */
	}
      /* Padding Type */
      data->byte29 |=
	(paddingtype[get_paddingtype_id (s->val[OPT_PADDING_TYPE].s)].
	 val & 0x07);

      /* Bit Ordering: 
       *     Manual Says DEFAULT: [1111 1111][1111 1000]
       * Bits15-8 reserved;
       * Bit7: '0'-Normal '1'-Mirroring
       * Bit6-4: Reserved
       * Bit3: '0'-arrangement from MSB in grayscale mode
       *       '1'-arrangement from LSB in grayscale mode
       *    2: '0'-unpacked 4-bits grayscale [DEFAULT]
       *       '1'-packed 4-bits grayscale
       *    1: '0'-output from LSB of each word [DEFAULT]
       *       '1'-output from MSB of each word
       *    0: '0'-output from bit 0 of each byte [DEFAULT]
       *       '1'-output from bit 7 of each byte
       */
      _lto2b (0x007, &data->bit_ordering[0]);	/* Set to Packed4bitGray, MSB, MSbit */

      /* Compression Type and Argument NOT SUPPORTED in this scanner */
      data->compression_type = 0x00;
      data->compression_arg = 0x02;

      /* Byte42:  MRIF:Filtering:GammaID */
      if (data->image_composition == GRAYSCALE)
	{
	  if (s->val[OPT_NEGATIVE].w)
	    data->byte42 &= ~(1 << 7);	/* unset bit 7 */
	  else
	    data->byte42 |= (1 << 7);	/* set bit 7 */
	  data->byte42 |= (get_grayfilter_val (s->val[OPT_GRAYFILTER].s) & (7 << 4));	/* set bits 6-4 to GRAYFILTER */
	}
      else
	{
	  data->byte42 &= ~(1 << 7);	/* unset bit 7 */
	  data->byte42 &= ~(7 << 4);	/* unset bits 6-4 */
	}
      /* Bytes 45, 365 Binary Filtering for lineart and halftone can be set when option IPU is installed */
      if ((id = get_noisematrix_id (s->val[OPT_NOISEREMOVAL].s)) != 0)
	{
	  data->binary_filtering |= (1 << 7);	/* set bit 7 */
	  data->binary_filtering |= noisematrix[id].val;	/* 00H, 01H, 02H; 03H:Reserved */
	}
      if (s->val[OPT_SMOOTHING].w == SANE_TRUE)
	data->binary_filtering |= (1 << 6);	/* set bit 6 */

      /* Automatic separation, automatic binarization, and SECTION is available if Image Processing Unit is installed */
      if (s->hw->info.hasIPU)
	{
	  /* Byte 48: Automatic Separation */
	  data->automatic_separation =
	    get_auto_separation_val (s->val[OPT_AUTOSEP].s);
	  /* Byte 50: Automatic Binarization */
	  data->automatic_binarization =
	    get_auto_binarization_val (s->val[OPT_AUTOBIN].s);
	  /* fill in values for each section
	     for(j=0; j<NumSec; j++){ 
	     wbuf[i].winsec[j].ulx
	     wbuf[i].winsec[j].uly
	     wbuf[i].winsec[j].width
	     wbuf[i].winsec[j].length
	     wbuf[i].winsec[j].binary_filtering
	     wbuf[i].winsec[j].threshold
	     wbuf[i].winsec[j].image_composition
	     wbuf[i].winsec[j].halftone_id
	     wbuf[i].winsec[j].halftone_arg
	     }
	   */
	}
    }
  DBG (DBG_proc, "<< set_window_data\n");
  return (SANE_STATUS_GOOD);
}

SANE_Status
sane_start (SANE_Handle handle)	/* begin scanning */
{
  HS2P_Scanner *s = handle;
  SANE_Status status;
  SWD wbuf;			/* Set Window Data: hdr + data */
  GWD gbuf;			/* Get Window Data: hdr + data */
  SANE_Byte mode, prefeed, mwt = 0;

  DBG (DBG_proc, ">> sane_start\n");
  s->cancelled = SANE_FALSE;

  if (s->another_side)
    {
      /* Number of bytes to read for one side of sheet */
      s->bytes_to_read = s->params.bytes_per_line * s->params.lines;
      DBG (DBG_info,
	   "SIDE#2 %d pixels per line, %d bytes, %d lines high, dpi=%d\n",
	   s->params.pixels_per_line, s->params.bytes_per_line,
	   s->params.lines, (int) s->val[OPT_Y_RESOLUTION].w);
      s->scanning = SANE_TRUE;
      s->cancelled = SANE_FALSE;
      s->another_side = SANE_FALSE;	/* This is side 2, so no more sides */
      DBG (DBG_proc, "<< sane_start\n");
      return (SANE_STATUS_GOOD);
    }

  if (s->scanning)
    {
      DBG (DBG_info, "sane_start: device busy\n");
      return SANE_STATUS_DEVICE_BUSY;
    }

  /* Let's start a new scan */

  if ((status = sane_get_parameters (s, 0)) != SANE_STATUS_GOOD)
    {				/* get preliminary parameters */
      DBG (DBG_error, "sane_start: sane_get_parameters failed: %s\n",
	   sane_strstatus (status));
      return (status);
    }

  DBG (DBG_info, ">> sane_start: trying to open: name=\"%s\" fd=%d\n",
       s->hw->sane.name, s->fd);
  if ((status =
       sanei_scsi_open (s->hw->sane.name, &s->fd, &sense_handler,
			&(s->hw->sense_data))) != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sane_start: open of %s failed: %d %s\n",
	   s->hw->sane.name, status, sane_strstatus (status));
      return (status);
    }
  DBG (DBG_info, ">>sane_start: OPENED \"%s\" fd=%d\n", s->hw->sane.name,
       s->fd);

  if ((status = test_unit_ready (s->fd)) != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sane_start: test_unit_ready() failed: %s\n",
	   sane_strstatus (status));
      sanei_scsi_close (s->fd);
      s->fd = -1;
      return status;
    }


  if ((status = reserve_unit (s->fd)) != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sane_start: reserve_unit() failed: %s\n",
	   sane_strstatus (status));
      sanei_scsi_close (s->fd);
      s->fd = -1;
      return (status);
    }

  /* NOW SET UP SCANNER ONCE PER BATCH */

  DBG (DBG_info, "sane_start: setting basic measurement unit to mm\n");
  if ((status = set_basic_measurement_unit (s->fd, s->hw->info.bmu)))
    {
      DBG (DBG_error, "set_basic_measurment_unit failed: %s\n",
	   sane_strstatus (status));
      release_unit (s->fd);
      sanei_scsi_close (s->fd);
      s->fd = -1;
      return (status);
    }

  if (get_scan_source_id (s->val[OPT_SCAN_SOURCE].s) == 0)
    {
      mode = FLATBED;
    }
  else
    {
      mode = (s->val[OPT_DUPLEX].w) ? DUPLEX : SIMPLEX;
    }

  prefeed = s->val[OPT_PREFEED].w ? 0x04 : 0x00;
  DBG (DBG_info, "sane_start: setting scan source to %d %s\n", mode,
       (SANE_String) s->val[OPT_SCAN_SOURCE].s);
  DBG (DBG_info, "sane_start: setting prefeed to %d\n", prefeed);
  if ((status =
       set_adf_control (s->fd, &mode, &prefeed, &mwt)) != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sane_start: error set_adf_control: %s\n",
	   sane_strstatus (status));
      release_unit (s->fd);
      sanei_scsi_close (s->fd);
      s->fd = -1;
      return (SANE_STATUS_INVAL);
    }


  DBG (DBG_info, "sane_start: setting endorser control to %d\n",
       s->val[OPT_ENDORSER].w);
  if ((status =
       set_endorser_control (s->fd,
			     &s->val[OPT_ENDORSER].w)) != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "set_endorser_control failed: %s\n",
	   sane_strstatus (status));
      release_unit (s->fd);
      sanei_scsi_close (s->fd);
      s->fd = -1;
      return (status);
    }
  if (s->val[OPT_ENDORSER].w)
    {
      DBG (DBG_info, "sane_start: setting endorser string to %s\n",
	   s->val[OPT_ENDORSER_STRING].s);
      if ((status =
	   set_endorser_string (s->fd,
				(SANE_String) s->val[OPT_ENDORSER_STRING].
				s)) != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "set_endorser_string failed: %s\n",
	       sane_strstatus (status));
	  release_unit (s->fd);
	  sanei_scsi_close (s->fd);
	  s->fd = -1;
	  return (status);
	}
    }

  DBG (DBG_info, "sane_start: setting scan_wait_mode to %d\n",
       s->val[OPT_SCAN_WAIT_MODE].w);
  if ((status =
       set_scan_wait_mode (s->fd,
			   s->val[OPT_SCAN_WAIT_MODE].w)) != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "set_scan_wait_mode failed: %s\n",
	   sane_strstatus (status));
      release_unit (s->fd);
      sanei_scsi_close (s->fd);
      s->fd = -1;
      return (status);
    }
  DBG (DBG_info, "sane_start: setting white_balance to %d\n",
       s->val[OPT_WHITE_BALANCE].w);
  if ((status =
       set_white_balance (s->fd,
			  &s->val[OPT_WHITE_BALANCE].w)) != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "set_white_balance failed: %s\n",
	   sane_strstatus (status));
      release_unit (s->fd);
      sanei_scsi_close (s->fd);
      s->fd = -1;
      return (status);
    }

  if (s->val[OPT_CUSTOM_GAMMA].b)
    {				/* Custom Gamma needs to be sent to scanner */
      DBG (DBG_info, "sane_start: setting custom gamma\n");
      if ((status = hs2p_send_gamma (s)))
	{
	  DBG (DBG_error, "hs2p_send_gamma failed: %s\n",
	       sane_strstatus (status));
	  release_unit (s->fd);
	  sanei_scsi_close (s->fd);
	  s->fd = -1;
	  return (status);
	}
      /* We succeeded, so we don't need to upload this vector again (unless user modifies gamma table) */
      s->val[OPT_CUSTOM_GAMMA].b = SANE_FALSE;
    }


  DBG (DBG_info, "sane_start: filling in window data buffer \n");
  if ((status = set_window_data (s, &wbuf)) != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "set_window_data failed: %s\n",
	   sane_strstatus (status));
      release_unit (s->fd);
      sanei_scsi_close (s->fd);
      s->fd = -1;
      return (status);
    }
  DBG (DBG_info, "sane_start: sending SET WINDOW DATA\n");
  if ((status = set_window (s->fd, &wbuf)) != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "SET WINDOW DATA failed: %s\n",
	   sane_strstatus (status));
      print_window_data (&wbuf);
      release_unit (s->fd);
      sanei_scsi_close (s->fd);
      s->fd = -1;
      return (status);
    }
  DBG (DBG_info, "sane_start: sending GET WINDOW\n");
  memset (&gbuf, 0, sizeof (gbuf));	/* CLEAR wbuf */
  if ((status = get_window (s->fd, &gbuf)) != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "GET WINDOW failed: %s\n", sane_strstatus (status));
      release_unit (s->fd);
      sanei_scsi_close (s->fd);
      s->fd = -1;
      return (status);
    }

  /* DONE WITH SETTING UP SCANNER ONCE PER BATCH */

  s->EOM = SANE_FALSE;
  if (mode != FLATBED)
    {
      if ((status =
	   get_hs2p_data (s, DATA_TYPE_ADF_STATUS,
			  DATA_TYPE_EOL)) != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "sane_start: error reading adf_status:  %s\n",
	       sane_strstatus (status));
	  return (status);
	}
      if ((s->data.adf_status & 0x00) == 0x01)
	{
	  DBG (DBG_warning, "sane_start: No document on ADF\n");
	  return (SANE_STATUS_NO_DOCS);
	}
      else if ((s->data.adf_status & 0x02) == 0x02)
	{
	  DBG (DBG_warning, "sane_start: ADF cover open!\n");
	  return (SANE_STATUS_COVER_OPEN);
	}
    }


  status = trigger_scan (s);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "start of scan failed: %s\n", sane_strstatus (status));
      print_window_data (&wbuf);
      /* this line introduced not to freeze xscanimage */
      /*do_cancel (s); */
      return status;
    }
  /* Wait for scanner to become ready to transmit data */
  status = hs2p_wait_ready (s);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "GET DATA STATUS failed: %s\n",
	   sane_strstatus (status));
      return (status);
    }

  s->another_side = (mode == DUPLEX) ? SANE_TRUE : SANE_FALSE;
  /* Number of bytes to read for one side of sheet */
  DBG (DBG_info, "ANOTHER SIDE = %s\n", (s->another_side) ? "TRUE" : "FALSE");
  s->bytes_to_read = s->params.bytes_per_line * s->params.lines;
  DBG (DBG_info, "%d pixels per line, %d bytes, %d lines high, dpi=%d\n",
       s->params.pixels_per_line, s->params.bytes_per_line,
       s->params.lines, (int) s->val[OPT_Y_RESOLUTION].w);
  s->scanning = SANE_TRUE;
  s->cancelled = SANE_FALSE;

  DBG (DBG_proc, "<< sane_start\n");
  return (SANE_STATUS_GOOD);
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len,
	   SANE_Int * len)
{
  HS2P_Scanner *s = handle;
  SANE_Status status;
  size_t nread, bytes_requested, i, start;
  SANE_Byte color;
  DBG (DBG_proc, ">> sane_read\n");

  *len = 0;

  DBG (DBG_info, "sane_read: bytes left to read: %ld\n",
       (u_long) s->bytes_to_read);

  if (s->bytes_to_read == 0)
    {				/* We've reached the end of one side of sheet */
      if (!s->another_side)
	{
	  do_cancel (s);
	  return (SANE_STATUS_EOF);
	}
      else
	{
	  /* let frontend call sane_start again to reset bytes_to_read */
	  DBG (DBG_proc, "<< sane_read: getting another side\n");
	  return (SANE_STATUS_EOF);
	}
    }

  if (s->cancelled)
    {
      DBG (DBG_info, "sane_read: cancelled!\n");
      return SANE_STATUS_CANCELLED;
    }
  if (!s->scanning)
    {
      DBG (DBG_info, "sane_read: scanning is false!\n");
      return (do_cancel (s));
    }

  nread = max_len;
  if (nread > s->bytes_to_read)
    nread = s->bytes_to_read;
  bytes_requested = nread;
  start = 0;

pad:
  if (s->EOM)
    {
      if (s->val[OPT_PADDING].w)
	{
	  DBG (DBG_info, "sane_read s->EOM padding from %ld to %ld\n",
	       (u_long) start, (u_long) bytes_requested);
	  color = (s->val[OPT_NEGATIVE].w) ? 0 : 255;
	  /* pad to requested length */
	  for (i = start; i < bytes_requested; i++)
	    buf[i] = color;
	  nread = bytes_requested;	/* we've padded to bytes_requested */
	  *len = nread;
	  s->bytes_to_read -= nread;
	}
      else			/* TRUNCATE: should never reach here */
	{
	  *len = nread;
	  s->bytes_to_read = 0;	/* EOM */
	}
    }
  else
    {
      DBG (DBG_info, "sane_read: trying to read %ld bytes\n", (u_long) nread);
      status = read_data (s->fd, buf, &nread, DATA_TYPE_IMAGE, DTQ);
      switch (status)
	{
	case SANE_STATUS_NO_DOCS:
	  DBG (DBG_error, "sane_read: End-Of-Medium detected\n");
	  s->EOM = SANE_TRUE;
	  /* 
	   * If status != SANE_STATUS_GOOD, then sense_handler() has already
	   * been called and the sanei_* functions have already gotten the
	   * sense data buffer (which apparently clears the error conditionn)
	   * so the following doesn't work:
	   get_sense_data (s->fd, &(s->hw->sense_data));
	   print_sense_data (&(s->hw->sense_data));
	   */
	  start = (isset_ILI (s->hw->sense_data)) ?	/* Invalid Length Indicator */
	    bytes_requested - _4btol (s->hw->sense_data.information) : nread;
	  goto pad;
	  break;
	case SANE_STATUS_GOOD:
	  *len = nread;
	  s->bytes_to_read -= nread;
	  break;
	default:
	  DBG (DBG_error, "sane_read: read error\n");
	  do_cancel (s);
	  return (SANE_STATUS_IO_ERROR);
	}
    }
  DBG (DBG_proc, "<< sane_read\n");
  return (SANE_STATUS_GOOD);
}


void
sane_cancel (SANE_Handle handle)
{
  HS2P_Scanner *s = handle;
  DBG (DBG_proc, ">> sane_cancel\n");

  if (s->scanning)
    {				/* if batchmode is enabled, then call set_window to abort the batch 
				   if (_OPT_VAL_WORD(s, OPT_BATCH) == SANE_TRUE) {
				   DBG(5, "sane_cancel: calling set_window to abort batch\n");
				   set_window(s, BH_BATCH_ABORT);
				   }   */
      do_cancel (s);
    }



  DBG (DBG_proc, "<< sane_cancel\n");
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  DBG (DBG_proc, ">> sane_set_io_mode (handle = %p, non_blocking = %d)\n",
       handle, non_blocking);
  DBG (DBG_proc, "<< sane_set_io_mode\n");

  return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
#ifdef NONBLOCKSUPPORTED
  HS2P_Scanner *s = handle;
#endif
  DBG (DBG_proc, ">> sane_get_select_fd (handle = %p, fd = %p)\n", handle,
       (void *) fd);

#ifdef NONBLOCKSUPPORTED
  if (s->fd < 0)
    {
      DBG (DBG_proc, "<< sane_get_select_fd\n");
      return SANE_STATUS_INVAL;
    }
  *fd = s->fd;
  return SANE_STATUS_GOOD;
#else
  handle = handle;
  fd = fd;			/* get rid of compiler warning */
  DBG (DBG_proc, "<< sane_get_select_fd\n");
  return SANE_STATUS_UNSUPPORTED;
#endif
}
