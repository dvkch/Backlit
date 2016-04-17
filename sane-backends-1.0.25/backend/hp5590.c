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

#include "../include/sane/config.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../include/sane/sane.h"
#define BACKEND_NAME hp5590
#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/saneopts.h"
#include "hp5590_cmds.c"
#include "hp5590_low.c"

/* Debug levels */
#define	DBG_err		0
#define	DBG_proc	10
#define	DBG_verbose	20

#define hp5590_assert(exp) if(!(exp)) { \
	DBG (DBG_err, "Assertion '%s' failed at %s:%u\n", #exp, __FILE__, __LINE__);\
	return SANE_STATUS_INVAL; \
}

#define hp5590_assert_void_return(exp) if(!(exp)) { \
	DBG (DBG_err, "Assertion '%s' failed at %s:%u\n", #exp, __FILE__, __LINE__);\
	return; \
}

/* #define HAS_WORKING_COLOR_48 */
#define BUILD 		7
#define USB_TIMEOUT	30 * 1000

static SANE_Word
res_list[] = { 6, 100, 200, 300, 600, 1200, 2400 };

#define SANE_VALUE_SCAN_SOURCE_FLATBED  	SANE_I18N("Flatbed")
#define SANE_VALUE_SCAN_SOURCE_ADF 		SANE_I18N("ADF")
#define SANE_VALUE_SCAN_SOURCE_ADF_DUPLEX 	SANE_I18N("ADF Duplex")
#define SANE_VALUE_SCAN_SOURCE_TMA_SLIDES 	SANE_I18N("TMA Slides")
#define SANE_VALUE_SCAN_SOURCE_TMA_NEGATIVES 	SANE_I18N("TMA Negatives")

#define SANE_VALUE_SCAN_MODE_COLOR_24 		SANE_VALUE_SCAN_MODE_COLOR
#define SANE_VALUE_SCAN_MODE_COLOR_48 		SANE_I18N("Color (48 bits)")

#define SANE_NAME_LAMP_TIMEOUT 			"extend-lamp-timeout"
#define SANE_TITLE_LAMP_TIMEOUT 		SANE_I18N("Extend lamp timeout")
#define SANE_DESC_LAMP_TIMEOUT 			SANE_I18N("Extends lamp timeout (from 15 minutes to 1 hour)")
#define SANE_NAME_WAIT_FOR_BUTTON 		"wait-for-button"
#define SANE_TITLE_WAIT_FOR_BUTTON 		SANE_I18N("Wait for button")
#define SANE_DESC_WAIT_FOR_BUTTON 		SANE_I18N("Waits for button before scanning")

#define MAX_SCAN_SOURCE_VALUE_LEN 	24
#define MAX_SCAN_MODE_VALUE_LEN		24

static SANE_Range
range_x, range_y, range_qual;

static SANE_String_Const
mode_list[] = {
  SANE_VALUE_SCAN_MODE_COLOR_24,
#ifdef HAS_WORKING_COLOR_48
  SANE_VALUE_SCAN_MODE_COLOR_48,
#endif /* HAS_WORKING_COLOR_48 */
  SANE_VALUE_SCAN_MODE_GRAY,
  SANE_VALUE_SCAN_MODE_LINEART,
  NULL
};

enum hp5590_opt_idx {
  HP5590_OPT_NUM = 0,
  HP5590_OPT_TL_X,
  HP5590_OPT_TL_Y,
  HP5590_OPT_BR_X,
  HP5590_OPT_BR_Y,
  HP5590_OPT_MODE,
  HP5590_OPT_SOURCE,
  HP5590_OPT_RESOLUTION,
  HP5590_OPT_LAMP_TIMEOUT,
  HP5590_OPT_WAIT_FOR_BUTTON,
  HP5590_OPT_PREVIEW,
  HP5590_OPT_LAST
};

struct hp5590_scanner {
  struct scanner_info 		*info;
  enum proto_flags		proto_flags;
  SANE_Device 			sane;
  SANE_Int 			dn;
  float 			br_x, br_y, tl_x, tl_y;
  unsigned int 			dpi;
  enum color_depths 		depth;
  enum scan_sources	 	source;
  SANE_Bool 			extend_lamp_timeout;
  SANE_Bool 			wait_for_button;
  SANE_Bool 			preview;
  unsigned int 			quality;
  SANE_Option_Descriptor 	*opts;
  struct hp5590_scanner 	*next;
  unsigned int 			image_size;
  SANE_Int 			transferred_image_size;
  void			 	*bulk_read_state;
  SANE_Bool 			scanning;
};

static
struct hp5590_scanner *scanners_list;

/******************************************************************************/
static SANE_Status
calc_image_params (struct hp5590_scanner *scanner,
		   unsigned int *pixel_bits,
		   unsigned int *pixels_per_line,
		   unsigned int *bytes_per_line,
		   unsigned int *lines,
		   unsigned int *image_size)
{
  unsigned int 	_pixel_bits;
  SANE_Status	ret;
  unsigned int	_pixels_per_line;
  unsigned int	_bytes_per_line;
  unsigned int	_lines;
  unsigned int	_image_size;
  float		var;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  if (!scanner)
    return SANE_STATUS_INVAL;

  ret = hp5590_calc_pixel_bits (scanner->dpi, scanner->depth, &_pixel_bits);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  var = (float) (1.0 * (scanner->br_x - scanner->tl_x) * scanner->dpi);
  _pixels_per_line = var;
  if (var > _pixels_per_line)
    _pixels_per_line++;

  var  = (float) (1.0 * (scanner->br_y - scanner->tl_y) * scanner->dpi);
  _lines = var;
  if (var > _lines)
    _lines++;

  var  = (float) (1.0 * _pixels_per_line / 8 * _pixel_bits);
  _bytes_per_line  = var;
  if (var > _bytes_per_line)
    _bytes_per_line++;

  _image_size 	   = _lines * _bytes_per_line;

  DBG (DBG_verbose, "%s: pixel_bits: %u, pixels_per_line: %u, "
       "bytes_per_line: %u, lines: %u, image_size: %u\n",
       __FUNCTION__,
       _pixel_bits, _pixels_per_line, _bytes_per_line, _lines, _image_size);

  if (pixel_bits)
    *pixel_bits = _pixel_bits;

  if (pixels_per_line)
    *pixels_per_line = _pixels_per_line;

  if (bytes_per_line)
    *bytes_per_line = _bytes_per_line;

  if (lines)
    *lines = _lines;

  if (image_size)
    *image_size = _image_size;

  return SANE_STATUS_GOOD;
}
    
/******************************************************************************/
static SANE_Status
attach_usb_device (SANE_String_Const devname,
		   enum hp_scanner_types hp_scanner_type)
{
  struct scanner_info		*info;
  struct hp5590_scanner		*scanner, *ptr;
  unsigned int 			max_count, count;
  SANE_Int 			dn;
  SANE_Status			ret;
  const struct hp5590_model	*hp5590_model;

  DBG (DBG_proc, "%s: Opening USB device\n", __FUNCTION__);
  if (sanei_usb_open (devname, &dn) != SANE_STATUS_GOOD)
    return SANE_STATUS_IO_ERROR;
  DBG (DBG_proc, "%s: USB device opened\n", __FUNCTION__);

  ret = hp5590_model_def (hp_scanner_type, &hp5590_model);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  if (hp5590_init_scanner (dn, hp5590_model->proto_flags,
    			   &info, hp_scanner_type) != 0)
    return SANE_STATUS_IO_ERROR;

  DBG (1, "%s: found HP%s scanner at '%s'\n",
       __FUNCTION__, info->model, devname);

  DBG (DBG_verbose, "%s: Reading max scan count\n", __FUNCTION__);
  if (hp5590_read_max_scan_count (dn, hp5590_model->proto_flags,
    				  &max_count) != 0)
    return SANE_STATUS_IO_ERROR;
  DBG (DBG_verbose, "%s: Max Scanning count %u\n", __FUNCTION__, max_count);

  DBG (DBG_verbose, "%s: Reading scan count\n", __FUNCTION__);
  if (hp5590_read_scan_count (dn, hp5590_model->proto_flags,
    			      &count) != 0)
    return SANE_STATUS_IO_ERROR;
  DBG (DBG_verbose, "%s: Scanning count %u\n", __FUNCTION__, count);

  ret = hp5590_read_part_number (dn, hp5590_model->proto_flags);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  ret = hp5590_stop_scan (dn, hp5590_model->proto_flags);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  scanner = malloc (sizeof(struct hp5590_scanner));
  if (!scanner)
    return SANE_STATUS_NO_MEM;
  memset (scanner, 0, sizeof(struct hp5590_scanner));

  scanner->sane.model = info->model;
  scanner->sane.vendor = "HP";
  scanner->sane.type = info->kind;
  scanner->sane.name = devname;
  scanner->dn = dn;
  scanner->proto_flags = hp5590_model->proto_flags;
  scanner->info = info;
  scanner->bulk_read_state = NULL;
  scanner->opts = NULL;

  if (!scanners_list)
    scanners_list = scanner;
  else
    {
      for (ptr = scanners_list; ptr->next; ptr = ptr->next);
      ptr->next = scanner;
    }

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
attach_hp4570 (SANE_String_Const devname)
{
  return attach_usb_device (devname, SCANNER_HP4570);
}

/******************************************************************************/
static SANE_Status
attach_hp5550 (SANE_String_Const devname)
{
  return attach_usb_device (devname, SCANNER_HP5550);
}

/******************************************************************************/
static SANE_Status
attach_hp5590 (SANE_String_Const devname)
{
  return attach_usb_device (devname, SCANNER_HP5590);
}

/******************************************************************************/
static SANE_Status
attach_hp7650 (SANE_String_Const devname)
{
  return attach_usb_device (devname, SCANNER_HP7650);
}

/******************************************************************************/
SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback __sane_unused__ authorize)
{
  SANE_Status 	ret;
  SANE_Word	vendor_id, product_id;
  
  DBG_INIT();
  
  DBG (1, "SANE backed for HP ScanJet 4500C/4570C/5500C/5550C/5590/7650 %u.%u.%u\n",
       SANE_CURRENT_MAJOR, V_MINOR, BUILD);
  DBG (1, "(c) Ilia Sotnikov <hostcc@gmail.com>\n");

  if (version_code)
    *version_code = SANE_VERSION_CODE(SANE_CURRENT_MAJOR, V_MINOR, BUILD);

  sanei_usb_init();

  sanei_usb_set_timeout (USB_TIMEOUT);

  scanners_list = NULL;

  ret = hp5590_vendor_product_id (SCANNER_HP4570, &vendor_id, &product_id);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  ret = sanei_usb_find_devices (vendor_id, product_id, attach_hp4570);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  ret = hp5590_vendor_product_id (SCANNER_HP5550, &vendor_id, &product_id);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  ret = sanei_usb_find_devices (vendor_id, product_id, attach_hp5550);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  ret = hp5590_vendor_product_id (SCANNER_HP5590, &vendor_id, &product_id);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  ret = sanei_usb_find_devices (vendor_id, product_id, attach_hp5590);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  ret = hp5590_vendor_product_id (SCANNER_HP7650, &vendor_id, &product_id);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  ret = sanei_usb_find_devices (vendor_id, product_id, attach_hp7650);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
void sane_exit (void)
{
  struct hp5590_scanner *ptr, *pnext;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  for (ptr = scanners_list; ptr; ptr = pnext)
    {
      if (ptr->opts != NULL)
	free (ptr->opts);
      pnext = ptr->next;
      free (ptr);
    }
}

/******************************************************************************/
SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  struct hp5590_scanner *ptr;
  unsigned int found, i;

  DBG (DBG_proc, "%s, local only: %u\n", __FUNCTION__, local_only);

  if (!device_list)
    return SANE_STATUS_INVAL;

  for (found = 0, ptr = scanners_list; ptr; found++, ptr = ptr->next);
  DBG (1, "Found %u devices\n", found);

  found++;
  *device_list = malloc (found * sizeof (SANE_Device));
  if (!*device_list)
    return SANE_STATUS_NO_MEM;
  memset (*device_list, 0, found * sizeof(SANE_Device));

  for (i = 0, ptr = scanners_list; ptr; i++, ptr = ptr->next)
    {
      (*device_list)[i] = &(ptr->sane);
    }

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  struct hp5590_scanner 	*ptr;
  SANE_Option_Descriptor 	*opts;
  unsigned int 			available_sources;
  SANE_String_Const 		*sources_list;
  unsigned int 			source_idx;

  DBG (DBG_proc, "%s: device name: %s\n", __FUNCTION__, devicename);

  if (!handle)
    return SANE_STATUS_INVAL;

  /* Allow to open the first available device by specifying zero-length name */
  if (!devicename || !devicename[0]) {
    ptr = scanners_list;
  } else {
    for (ptr = scanners_list;
	 ptr && strcmp (ptr->sane.name, devicename) != 0;
	 ptr = ptr->next);
  }

  if (!ptr)
    return SANE_STATUS_INVAL;

  ptr->tl_x = 0;
  ptr->tl_y = 0;
  ptr->br_x = ptr->info->max_size_x;
  ptr->br_y = ptr->info->max_size_y;
  ptr->dpi = res_list[1];
  ptr->depth = DEPTH_BW; 
  ptr->source = SOURCE_FLATBED;
  ptr->extend_lamp_timeout = SANE_FALSE;
  ptr->wait_for_button = SANE_FALSE;
  ptr->preview = SANE_FALSE;
  ptr->quality = 4;
  ptr->image_size = 0;
  ptr->scanning = SANE_FALSE;

  *handle = ptr;

  opts = malloc (sizeof (SANE_Option_Descriptor) * HP5590_OPT_LAST);
  if (!opts)
    return SANE_STATUS_NO_MEM;

  opts[HP5590_OPT_NUM].name = SANE_NAME_NUM_OPTIONS;
  opts[HP5590_OPT_NUM].title = SANE_TITLE_NUM_OPTIONS;
  opts[HP5590_OPT_NUM].desc = SANE_DESC_NUM_OPTIONS;
  opts[HP5590_OPT_NUM].type = SANE_TYPE_INT;
  opts[HP5590_OPT_NUM].unit = SANE_UNIT_NONE;
  opts[HP5590_OPT_NUM].size = sizeof(SANE_Word);
  opts[HP5590_OPT_NUM].cap =  SANE_CAP_INACTIVE | SANE_CAP_SOFT_DETECT;
  opts[HP5590_OPT_NUM].constraint_type = SANE_CONSTRAINT_NONE;
  opts[HP5590_OPT_NUM].constraint.string_list = NULL;

  range_x.min = SANE_FIX(0);
  range_x.max = SANE_FIX(ptr->info->max_size_x * 25.4);
  range_x.quant = SANE_FIX(0.1);
  range_y.min = SANE_FIX(0);
  range_y.max = SANE_FIX(ptr->info->max_size_y * 25.4);
  range_y.quant = SANE_FIX(0.1);

  range_qual.min = SANE_FIX(4);
  range_qual.max = SANE_FIX(16);
  range_qual.quant = SANE_FIX(1);

  opts[HP5590_OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  opts[HP5590_OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  opts[HP5590_OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  opts[HP5590_OPT_TL_X].type = SANE_TYPE_FIXED;
  opts[HP5590_OPT_TL_X].unit = SANE_UNIT_MM;
  opts[HP5590_OPT_TL_X].size = sizeof(SANE_Fixed);
  opts[HP5590_OPT_TL_X].cap =  SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  opts[HP5590_OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  opts[HP5590_OPT_TL_X].constraint.range = &range_x;

  opts[HP5590_OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  opts[HP5590_OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  opts[HP5590_OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  opts[HP5590_OPT_TL_Y].type = SANE_TYPE_FIXED;
  opts[HP5590_OPT_TL_Y].unit = SANE_UNIT_MM;
  opts[HP5590_OPT_TL_Y].size = sizeof(SANE_Fixed);
  opts[HP5590_OPT_TL_Y].cap =  SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  opts[HP5590_OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  opts[HP5590_OPT_TL_Y].constraint.range = &range_y;

  opts[HP5590_OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  opts[HP5590_OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  opts[HP5590_OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  opts[HP5590_OPT_BR_X].type = SANE_TYPE_FIXED;
  opts[HP5590_OPT_BR_X].unit = SANE_UNIT_MM;
  opts[HP5590_OPT_BR_X].size = sizeof(SANE_Fixed);
  opts[HP5590_OPT_BR_X].cap =  SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  opts[HP5590_OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  opts[HP5590_OPT_BR_X].constraint.range = &range_x;

  opts[HP5590_OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  opts[HP5590_OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  opts[HP5590_OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  opts[HP5590_OPT_BR_Y].type = SANE_TYPE_FIXED;
  opts[HP5590_OPT_BR_Y].unit = SANE_UNIT_MM;
  opts[HP5590_OPT_BR_Y].size = sizeof(SANE_Fixed);
  opts[HP5590_OPT_BR_Y].cap =  SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  opts[HP5590_OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  opts[HP5590_OPT_BR_Y].constraint.range = &range_y;

  opts[HP5590_OPT_MODE].name = SANE_NAME_SCAN_MODE;
  opts[HP5590_OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  opts[HP5590_OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  opts[HP5590_OPT_MODE].type = SANE_TYPE_STRING;
  opts[HP5590_OPT_MODE].unit = SANE_UNIT_NONE;
  opts[HP5590_OPT_MODE].size = MAX_SCAN_MODE_VALUE_LEN;
  opts[HP5590_OPT_MODE].cap =  SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  opts[HP5590_OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  opts[HP5590_OPT_MODE].constraint.string_list = mode_list;

  available_sources = 1; /* Flatbed is always available */
  if (ptr->info->features & FEATURE_ADF)
    available_sources += 2;
  if (ptr->info->features & FEATURE_TMA)
    available_sources += 2;
  available_sources++;	/* Count terminating NULL */
  sources_list = malloc (available_sources * sizeof (SANE_String_Const));
  if (!sources_list)
    return SANE_STATUS_NO_MEM;
  source_idx = 0;
  sources_list[source_idx++] = SANE_VALUE_SCAN_SOURCE_FLATBED;
  if (ptr->info->features & FEATURE_ADF)
    {
      sources_list[source_idx++] = SANE_VALUE_SCAN_SOURCE_ADF;
      sources_list[source_idx++] = SANE_VALUE_SCAN_SOURCE_ADF_DUPLEX;
    }
  if (ptr->info->features & FEATURE_TMA)
    {
      sources_list[source_idx++] = SANE_VALUE_SCAN_SOURCE_TMA_SLIDES;
      sources_list[source_idx++] = SANE_VALUE_SCAN_SOURCE_TMA_NEGATIVES;
    }
  sources_list[source_idx] = NULL;

  opts[HP5590_OPT_SOURCE].name = SANE_NAME_SCAN_SOURCE;
  opts[HP5590_OPT_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
  opts[HP5590_OPT_SOURCE].desc = SANE_DESC_SCAN_SOURCE;
  opts[HP5590_OPT_SOURCE].type = SANE_TYPE_STRING;
  opts[HP5590_OPT_SOURCE].unit = SANE_UNIT_NONE;
  opts[HP5590_OPT_SOURCE].size = MAX_SCAN_SOURCE_VALUE_LEN;
  opts[HP5590_OPT_SOURCE].cap =  SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  opts[HP5590_OPT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  opts[HP5590_OPT_SOURCE].constraint.string_list = sources_list;

  opts[HP5590_OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  opts[HP5590_OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  opts[HP5590_OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  opts[HP5590_OPT_RESOLUTION].type = SANE_TYPE_INT;
  opts[HP5590_OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  opts[HP5590_OPT_RESOLUTION].size = sizeof(SANE_Int);
  opts[HP5590_OPT_RESOLUTION].cap =  SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  opts[HP5590_OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  opts[HP5590_OPT_RESOLUTION].constraint.word_list = res_list;

  opts[HP5590_OPT_LAMP_TIMEOUT].name = SANE_NAME_LAMP_TIMEOUT;
  opts[HP5590_OPT_LAMP_TIMEOUT].title = SANE_TITLE_LAMP_TIMEOUT;
  opts[HP5590_OPT_LAMP_TIMEOUT].desc = SANE_DESC_LAMP_TIMEOUT;
  opts[HP5590_OPT_LAMP_TIMEOUT].type = SANE_TYPE_BOOL;
  opts[HP5590_OPT_LAMP_TIMEOUT].unit = SANE_UNIT_NONE;
  opts[HP5590_OPT_LAMP_TIMEOUT].size = sizeof(SANE_Bool);
  opts[HP5590_OPT_LAMP_TIMEOUT].cap =  SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
  opts[HP5590_OPT_LAMP_TIMEOUT].constraint_type = SANE_CONSTRAINT_NONE;
  opts[HP5590_OPT_LAMP_TIMEOUT].constraint.string_list = NULL;

  opts[HP5590_OPT_WAIT_FOR_BUTTON].name = SANE_NAME_WAIT_FOR_BUTTON;
  opts[HP5590_OPT_WAIT_FOR_BUTTON].title = SANE_TITLE_WAIT_FOR_BUTTON;
  opts[HP5590_OPT_WAIT_FOR_BUTTON].desc = SANE_DESC_WAIT_FOR_BUTTON;
  opts[HP5590_OPT_WAIT_FOR_BUTTON].type = SANE_TYPE_BOOL;
  opts[HP5590_OPT_WAIT_FOR_BUTTON].unit = SANE_UNIT_NONE;
  opts[HP5590_OPT_WAIT_FOR_BUTTON].size = sizeof(SANE_Bool);
  opts[HP5590_OPT_WAIT_FOR_BUTTON].cap =  SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  opts[HP5590_OPT_WAIT_FOR_BUTTON].constraint_type = SANE_CONSTRAINT_NONE;
  opts[HP5590_OPT_WAIT_FOR_BUTTON].constraint.string_list = NULL;

  opts[HP5590_OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  opts[HP5590_OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  opts[HP5590_OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  opts[HP5590_OPT_PREVIEW].type = SANE_TYPE_BOOL;
  opts[HP5590_OPT_PREVIEW].unit = SANE_UNIT_NONE;
  opts[HP5590_OPT_PREVIEW].size = sizeof(SANE_Bool);
  opts[HP5590_OPT_PREVIEW].cap =  SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  opts[HP5590_OPT_PREVIEW].constraint_type = SANE_CONSTRAINT_NONE;
  opts[HP5590_OPT_PREVIEW].constraint.string_list = NULL;

  ptr->opts = opts;

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
void
sane_close (SANE_Handle handle)
{
  struct hp5590_scanner *scanner = handle;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  sanei_usb_close (scanner->dn);
  scanner->dn = -1;
}

/******************************************************************************/
const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  struct hp5590_scanner *scanner = handle;

  DBG (DBG_proc, "%s, option: %u\n", __FUNCTION__, option);

  if (option >= HP5590_OPT_LAST)
    return NULL;

  return &scanner->opts[option];
}

/******************************************************************************/
SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *value,
                     SANE_Int * info)
{
  struct hp5590_scanner	*scanner = handle;
  
  if (!value)
    return SANE_STATUS_INVAL;

  if (!handle)
    return SANE_STATUS_INVAL;
 
  if (option >= HP5590_OPT_LAST)
    return SANE_STATUS_INVAL;

  if (action == SANE_ACTION_GET_VALUE)
    {
      if (option == HP5590_OPT_NUM)
	{
	  DBG(3, "%s: get total number of options - %u\n", __FUNCTION__, HP5590_OPT_LAST);
	  *((SANE_Int *) value) = HP5590_OPT_LAST;
	  return SANE_STATUS_GOOD;
	}
 
      if (!scanner->opts)
	return SANE_STATUS_INVAL;
      
      DBG (DBG_proc, "%s: get option '%s' value\n", __FUNCTION__, scanner->opts[option].name);

      if (option == HP5590_OPT_BR_X)
	{
	  *(SANE_Fixed *) value = SANE_FIX (scanner->br_x * 25.4);
	}

      if (option == HP5590_OPT_BR_Y)
	{
	  *(SANE_Fixed *) value = SANE_FIX (scanner->br_y * 25.4);
	}

      if (option == HP5590_OPT_TL_X)
	{
	  *(SANE_Fixed *) value = SANE_FIX ((scanner->tl_x) * 25.4);
	}

      if (option == HP5590_OPT_TL_Y)
	{
	  *(SANE_Fixed *) value = SANE_FIX (scanner->tl_y * 25.4);
	}

      if (option == HP5590_OPT_MODE)
	{
	  switch (scanner->depth) {
	    case DEPTH_BW:
	      memset (value , 0, scanner->opts[option].size);
	      memcpy (value, SANE_VALUE_SCAN_MODE_LINEART, strlen (SANE_VALUE_SCAN_MODE_LINEART));
	      break;
	    case DEPTH_GRAY:
	      memset (value , 0, scanner->opts[option].size);
	      memcpy (value, SANE_VALUE_SCAN_MODE_GRAY, strlen (SANE_VALUE_SCAN_MODE_GRAY));
	      break;
	    case DEPTH_COLOR_24:
	      memset (value , 0, scanner->opts[option].size);
	      memcpy (value, SANE_VALUE_SCAN_MODE_COLOR_24, strlen (SANE_VALUE_SCAN_MODE_COLOR_24));
	      break;
	    case DEPTH_COLOR_48:
	      memset (value , 0, scanner->opts[option].size);
	      memcpy (value, SANE_VALUE_SCAN_MODE_COLOR_48, strlen (SANE_VALUE_SCAN_MODE_COLOR_48));
	      break;
	    default:
	      return SANE_STATUS_INVAL;
	  }
	}

      if (option == HP5590_OPT_SOURCE)
	{
	  switch (scanner->source) {
	    case SOURCE_FLATBED:
	      memset (value , 0, scanner->opts[option].size);
	      memcpy (value, SANE_VALUE_SCAN_SOURCE_FLATBED, strlen (SANE_VALUE_SCAN_SOURCE_FLATBED));
	      break;
	    case SOURCE_ADF:
	      memset (value , 0, scanner->opts[option].size);
	      memcpy (value, SANE_VALUE_SCAN_SOURCE_ADF, strlen (SANE_VALUE_SCAN_SOURCE_ADF));
	      break;
	    case SOURCE_ADF_DUPLEX:
	      memset (value , 0, scanner->opts[option].size);
	      memcpy (value, SANE_VALUE_SCAN_SOURCE_ADF_DUPLEX, strlen (SANE_VALUE_SCAN_SOURCE_ADF_DUPLEX));
	      break;
	    case SOURCE_TMA_SLIDES:
	      memset (value , 0, scanner->opts[option].size);
	      memcpy (value, SANE_VALUE_SCAN_SOURCE_TMA_SLIDES, strlen (SANE_VALUE_SCAN_SOURCE_TMA_SLIDES));
	      break;
	    case SOURCE_TMA_NEGATIVES:
	      memset (value , 0, scanner->opts[option].size);
	      memcpy (value, SANE_VALUE_SCAN_SOURCE_TMA_NEGATIVES, strlen (SANE_VALUE_SCAN_SOURCE_TMA_NEGATIVES));
	      break;
	    case SOURCE_NONE:
	    default:
	      return SANE_STATUS_INVAL;
	  }
	}

      if (option == HP5590_OPT_RESOLUTION)
	{
	  *(SANE_Int *) value = scanner->dpi;
	}

      if (option == HP5590_OPT_LAMP_TIMEOUT)
	{
	  *(SANE_Bool *) value = scanner->extend_lamp_timeout;
	}

      if (option == HP5590_OPT_WAIT_FOR_BUTTON)
	{
	  *(SANE_Bool *) value = scanner->wait_for_button;
	}

      if (option == HP5590_OPT_PREVIEW)
	{
	  *(SANE_Bool *) value = scanner->preview;
	}
    }
 
  if (action == SANE_ACTION_SET_VALUE)
    {
      if (option == HP5590_OPT_NUM)
	return SANE_STATUS_INVAL;

      if (option == HP5590_OPT_BR_X)
	{
	  float val = SANE_UNFIX(*(SANE_Fixed *) value) / 25.4;
	  if (val <= scanner->tl_x)
	    return SANE_STATUS_GOOD;
	  scanner->br_x = val;
	  if (info)
	    *info = SANE_INFO_RELOAD_PARAMS;
	}

      if (option == HP5590_OPT_BR_Y)
	{
	  float val = SANE_UNFIX(*(SANE_Fixed *) value) / 25.4;
	  if (val <= scanner->tl_y)
	    return SANE_STATUS_GOOD;
	  scanner->br_y = val;
	  if (info)
 	    *info = SANE_INFO_RELOAD_PARAMS;
	}

      if (option == HP5590_OPT_TL_X)
	{
	  float val = SANE_UNFIX(*(SANE_Fixed *) value) / 25.4;
	  if (val >= scanner->br_x)
	    return SANE_STATUS_GOOD;
	  scanner->tl_x = val;
	  if (info)
 	    *info = SANE_INFO_RELOAD_PARAMS;
	}

      if (option == HP5590_OPT_TL_Y)
	{
	  float val = SANE_UNFIX(*(SANE_Fixed *) value) / 25.4;
	  if (val >= scanner->br_y)
	    return SANE_STATUS_GOOD;
	  scanner->tl_y = val;
	  if (info)
  	    *info = SANE_INFO_RELOAD_PARAMS;
	}

      if (option == HP5590_OPT_MODE)
	{
	  if (strcmp ((char *) value, (char *) SANE_VALUE_SCAN_MODE_LINEART) == 0)
	    {
	      scanner->depth = DEPTH_BW;
	    }

	  if (strcmp ((char *) value, (char *) SANE_VALUE_SCAN_MODE_GRAY) == 0)
	    {
	      scanner->depth = DEPTH_GRAY;
	    }

	  if (strcmp ((char *) value, (char *) SANE_VALUE_SCAN_MODE_COLOR_24) == 0)
	    {
	      scanner->depth = DEPTH_COLOR_24;
	    }

	  if (strcmp ((char *) value, (char *) SANE_VALUE_SCAN_MODE_COLOR_48) == 0)
	    {
	      scanner->depth = DEPTH_COLOR_48;
	    }
	  if (info)
 	    *info = SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
	}

      if (option == HP5590_OPT_SOURCE)
	{
          range_y.max = SANE_FIX(scanner->info->max_size_y * 25.4);

	  if (strcmp ((char *) value, (char *) SANE_VALUE_SCAN_SOURCE_FLATBED) == 0)
	    {
	      scanner->source = SOURCE_FLATBED;
	      range_x.max = SANE_FIX(scanner->info->max_size_x * 25.4);
	      range_y.max = SANE_FIX(scanner->info->max_size_y * 25.4);
	      scanner->br_x = scanner->info->max_size_x;
	      scanner->br_y = scanner->info->max_size_y;
	    }
	  /* In ADF modes the device can scan up to ADF_MAX_Y_INCHES, which is usually
	   * bigger than what scanner reports back during initialization
	   */
	  if (strcmp ((char *) value, (char *) SANE_VALUE_SCAN_SOURCE_ADF) == 0)
	    {
	      scanner->source = SOURCE_ADF;
	      range_x.max = SANE_FIX(scanner->info->max_size_x * 25.4);
	      range_y.max = SANE_FIX(ADF_MAX_Y_INCHES * 25.4);
	      scanner->br_x = scanner->info->max_size_x;
	      scanner->br_y = ADF_MAX_Y_INCHES * 25.4;
	    }
	  if (strcmp ((char *) value, (char *) SANE_VALUE_SCAN_SOURCE_ADF_DUPLEX) == 0)
	    {
	      scanner->source = SOURCE_ADF_DUPLEX;
	      range_x.max = SANE_FIX(scanner->info->max_size_x * 25.4);
	      range_y.max = SANE_FIX(ADF_MAX_Y_INCHES * 25.4 * 2);
	      scanner->br_y = ADF_MAX_Y_INCHES * 25.4 * 2;
	      scanner->br_x = scanner->info->max_size_x;
	    }
	  if (strcmp ((char *) value, (char *) SANE_VALUE_SCAN_SOURCE_TMA_SLIDES) == 0)
	    {
	      scanner->source = SOURCE_TMA_SLIDES;
	      range_x.max = SANE_FIX(TMA_MAX_X_INCHES * 25.4);
	      range_y.max = SANE_FIX(TMA_MAX_Y_INCHES * 25.4);
	      scanner->br_x = TMA_MAX_X_INCHES * 25.4;
	      scanner->br_y = TMA_MAX_Y_INCHES * 25.4;
	    }
	  if (strcmp ((char *) value, (char *) SANE_VALUE_SCAN_SOURCE_TMA_NEGATIVES) == 0)
	    {
	      scanner->source = SOURCE_TMA_NEGATIVES;
	      range_x.max = SANE_FIX(TMA_MAX_X_INCHES * 25.4);
	      range_y.max = SANE_FIX(TMA_MAX_Y_INCHES * 25.4);
	      scanner->br_x = TMA_MAX_X_INCHES * 25.4;
	      scanner->br_y = TMA_MAX_Y_INCHES * 25.4;
	    }
	  if (info)
	    *info = SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
	}

      if (option == HP5590_OPT_RESOLUTION)
	{
	  scanner->dpi = *(SANE_Int *) value;
	  if (info)
	    *info = SANE_INFO_RELOAD_PARAMS;
	}

      if (option == HP5590_OPT_LAMP_TIMEOUT)
	{
	  scanner->extend_lamp_timeout = *(SANE_Bool *) value;
	}

      if (option == HP5590_OPT_WAIT_FOR_BUTTON)
	{
	  scanner->wait_for_button = *(SANE_Bool *) value;
	}

      if (option == HP5590_OPT_PREVIEW)
	{
	  scanner->preview = *(SANE_Bool *) value;
	}
    }
   
  return SANE_STATUS_GOOD;
}

/******************************************************************************/
SANE_Status sane_get_parameters (SANE_Handle handle,
		                 SANE_Parameters * params)
{
  struct hp5590_scanner	*scanner = handle;
  SANE_Status		ret;
  unsigned int		pixel_bits;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  if (!params)
    return SANE_STATUS_INVAL;

  if (!handle)
    return SANE_STATUS_INVAL;

  ret = calc_image_params (scanner,
			   (unsigned int *) &pixel_bits,
			   (unsigned int *) &params->pixels_per_line,
			   (unsigned int *) &params->bytes_per_line,
			   (unsigned int *) &params->lines, NULL);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  switch (scanner->depth) {
    case DEPTH_BW:
      params->depth = pixel_bits;
      params->format = SANE_FRAME_GRAY;
      params->last_frame = SANE_TRUE;
      break;
    case DEPTH_GRAY:
      params->depth = pixel_bits;
      params->format = SANE_FRAME_GRAY;
      params->last_frame = SANE_TRUE;
      break;
    case DEPTH_COLOR_24:
      params->depth = pixel_bits / 3;
      params->last_frame = SANE_TRUE;
      params->format = SANE_FRAME_RGB;
      break;
    case DEPTH_COLOR_48:
      params->depth = pixel_bits / 3;
      params->last_frame = SANE_TRUE;
      params->format = SANE_FRAME_RGB;
      break;
    default:
      DBG(0, "%s: Unknown depth\n", __FUNCTION__);
      return SANE_STATUS_INVAL;
  }

  
  DBG (DBG_proc, "format: %u, last_frame: %u, bytes_per_line: %u, "
       "pixels_per_line: %u, lines: %u, depth: %u\n",
       params->format, params->last_frame,
       params->bytes_per_line, params->pixels_per_line,
       params->lines, params->depth);

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
SANE_Status
sane_start (SANE_Handle handle)
{
  struct hp5590_scanner	*scanner = handle;
  SANE_Status		ret;
  unsigned int		bytes_per_line;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  if (!scanner)
    return SANE_STATUS_INVAL;

  if (   scanner->scanning == SANE_TRUE
      && (  scanner->source == SOURCE_ADF
	 || scanner->source == SOURCE_ADF_DUPLEX))
    {
      DBG (DBG_verbose, "%s: Scanner is scanning, check if more data is available\n",
	   __FUNCTION__);
      ret = hp5590_is_data_available (scanner->dn, scanner->proto_flags);
      if (ret == SANE_STATUS_GOOD)
	{
	  DBG (DBG_verbose, "%s: More data is available\n", __FUNCTION__);
	  scanner->transferred_image_size = scanner->image_size;
	  return SANE_STATUS_GOOD;
	}

      if (ret != SANE_STATUS_NO_DOCS)
	return ret;
    }

  sane_cancel (handle);

  if (scanner->wait_for_button)
    {
      enum button_status status;
      for (;;)
	{
	  ret = hp5590_read_buttons (scanner->dn,
	  		     	     scanner->proto_flags,
				     &status);
	  if (ret != SANE_STATUS_GOOD)
	    return ret;

	  if (status == BUTTON_CANCEL)
	    return SANE_STATUS_CANCELLED;

	  if (status != BUTTON_NONE && status != BUTTON_POWER)
	    break;
	  sleep (1);
	}
    }

  DBG (DBG_verbose, "Init scanner\n");  
  ret = hp5590_init_scanner (scanner->dn, scanner->proto_flags,
  			     NULL, SCANNER_NONE);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  ret = hp5590_power_status (scanner->dn, scanner->proto_flags);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  DBG (DBG_verbose, "Wakeup\n");
  ret = hp5590_select_source_and_wakeup (scanner->dn, scanner->proto_flags,
  					 scanner->source,
					 scanner->extend_lamp_timeout);
  if (ret != SANE_STATUS_GOOD)
    return ret;
 
  ret = hp5590_set_scan_params (scanner->dn,
  				scanner->proto_flags,
  				scanner->info,
			      	scanner->tl_x * scanner->dpi,
			      	scanner->tl_y * scanner->dpi,
			      	(scanner->br_x - scanner->tl_x) * scanner->dpi,
			      	(scanner->br_y - scanner->tl_y) * scanner->dpi,
			      	scanner->dpi,
		              	scanner->depth, scanner->preview ? MODE_PREVIEW : MODE_NORMAL,
			      	scanner->source);
  if (ret != SANE_STATUS_GOOD)
    {
      hp5590_reset_scan_head (scanner->dn, scanner->proto_flags);
      return ret;
    }

  ret = calc_image_params (scanner, NULL, NULL,
			   &bytes_per_line, NULL,
			   &scanner->image_size);
  if (ret != SANE_STATUS_GOOD)
    {
      hp5590_reset_scan_head (scanner->dn, scanner->proto_flags);
      return ret;
    }

  scanner->transferred_image_size = scanner->image_size;

  if (   scanner->depth == DEPTH_COLOR_24
      || scanner->depth == DEPTH_COLOR_48)
    {
      DBG (1, "Color 24/48 bits: checking if image size is correctly "
	   "aligned on number of colors\n");
      if (bytes_per_line % 3)
	{
	  DBG (DBG_err, "Color 24/48 bits: image size doesn't lined up on number of colors (3) "
	       "(image size: %u, bytes per line %u)\n",
	       scanner->image_size, bytes_per_line);
          hp5590_reset_scan_head (scanner->dn, scanner->proto_flags);
	  return SANE_STATUS_INVAL;
	}
      DBG (1, "Color 24/48 bits: image size is correctly aligned on number of colors "
	   "(image size: %u, bytes per line %u)\n",
	   scanner->image_size, bytes_per_line);

      DBG (1, "Color 24/48 bits: checking if image size is correctly "
	   "aligned on bytes per line\n");
      if (scanner->image_size % bytes_per_line)
	{
	  DBG (DBG_err, "Color 24/48 bits: image size doesn't lined up on bytes per line "
	       "(image size: %u, bytes per line %u)\n",
	       scanner->image_size, bytes_per_line);
          hp5590_reset_scan_head (scanner->dn, scanner->proto_flags);
	  return SANE_STATUS_INVAL;
	}
      DBG (1, "Color 24/48 bits: image size correctly aligned on bytes per line "
	   "(images size: %u, bytes per line: %u)\n",
	   scanner->image_size, bytes_per_line);
    }
 
  DBG (DBG_verbose, "Final image size: %u\n", scanner->image_size);

  DBG (DBG_verbose, "Reverse calibration maps\n");
  ret = hp5590_send_reverse_calibration_map (scanner->dn, scanner->proto_flags);
  if (ret != SANE_STATUS_GOOD)
    {
      hp5590_reset_scan_head (scanner->dn, scanner->proto_flags);
      return ret;
    }

  DBG (DBG_verbose, "Forward calibration maps\n");
  ret = hp5590_send_forward_calibration_maps (scanner->dn, scanner->proto_flags);
  if (ret != SANE_STATUS_GOOD)
    {
      hp5590_reset_scan_head (scanner->dn, scanner->proto_flags);
      return ret;
    }

  scanner->scanning = SANE_TRUE;

  DBG (DBG_verbose, "Starting scan\n");
  ret = hp5590_start_scan (scanner->dn, scanner->proto_flags);
  /* Check for paper jam */
  if (	  ret == SANE_STATUS_DEVICE_BUSY
      && (   scanner->source == SOURCE_ADF
	  || scanner->source == SOURCE_ADF_DUPLEX))
    return SANE_STATUS_JAMMED;

  if (ret != SANE_STATUS_GOOD)
    {
      hp5590_reset_scan_head (scanner->dn, scanner->proto_flags);
      return ret;
    }

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
convert_lineart (struct hp5590_scanner *scanner, SANE_Byte *data, SANE_Int size)
{
  SANE_Int i;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  hp5590_assert (scanner != NULL);
  hp5590_assert (data != NULL);

  /* Invert lineart */
  if (scanner->depth == DEPTH_BW)
    {
      for (i = 0; i < size; i++)
	data[i] ^= 0xff;
    }

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
convert_to_rgb (struct hp5590_scanner *scanner, SANE_Byte *data, SANE_Int size)
{
  unsigned int pixels_per_line;
  unsigned int bytes_per_color;
  unsigned int bytes_per_line;
  unsigned int lines;
  unsigned int i, j;
  unsigned char *buf;
  unsigned char *ptr;
  SANE_Status	ret;

  hp5590_assert (scanner != NULL);
  hp5590_assert (data != NULL);

  if (   scanner->depth == DEPTH_BW
      || scanner->depth == DEPTH_GRAY)
    return SANE_STATUS_GOOD;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

#ifndef HAS_WORKING_COLOR_48
  if (scanner->depth == DEPTH_COLOR_48)
    return SANE_STATUS_UNSUPPORTED;
#endif

  ret = calc_image_params (scanner,
			   NULL,
			   &pixels_per_line, &bytes_per_line,
			   NULL, NULL);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  lines = size / bytes_per_line;
  bytes_per_color = bytes_per_line / 3;
  
  DBG (DBG_verbose, "Length : %u\n", size); 

  DBG (DBG_verbose, "Converting row RGB to normal RGB\n");

  DBG (DBG_verbose, "Bytes per line %u\n", bytes_per_line);
  DBG (DBG_verbose, "Bytes per color %u\n", bytes_per_color);
  DBG (DBG_verbose, "Lines %u\n", lines);

  buf = malloc (bytes_per_line);
  if (!buf)
    return SANE_STATUS_NO_MEM;

  ptr = data;
  for (j = 0; j < lines; ptr += bytes_per_line, j++)
    {
      memset (buf, 0, bytes_per_line);
      for (i = 0; i < pixels_per_line; i++)
	{
	  if (scanner->depth == DEPTH_COLOR_24)
	    {
	      /* R */
	      buf[i*3]   = ptr[i];
	      /* G */
	      buf[i*3+1] = ptr[i+bytes_per_color];
	      /* B */
	      buf[i*3+2] = ptr[i+bytes_per_color*2];
	    }
	  else
	    {
	      /* R */
	      buf[i*6]   = ptr[2*i+1];
	      buf[i*6+1] = ptr[2*i];
	      /* G */
	      buf[i*6+2] = ptr[2*i+bytes_per_color+1];
	      buf[i*6+3] = ptr[2*i+bytes_per_color];
	      /* B */
	      buf[i*6+4] = ptr[2*i+bytes_per_color*2+1];
	      buf[i*6+5] = ptr[2*i+bytes_per_color*2];
	    }
	}
      
      /* Invert pixels in case of TMA Negatives source has been selected */
      if (scanner->source == SOURCE_TMA_NEGATIVES)
        {
          for (i = 0; i < bytes_per_line; i++)
            buf[i] ^= 0xff;
        }

      memcpy (ptr, buf, bytes_per_line);
    }
  free (buf);

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * data,
	   SANE_Int max_length, SANE_Int * length)
{
  struct hp5590_scanner	*scanner = handle;
  SANE_Status 		ret;

  DBG (DBG_proc, "%s, length %u, left %u\n",
       __FUNCTION__,
       max_length,
       scanner->transferred_image_size);

  if (!length)
    {
      scanner->scanning = SANE_FALSE;
      return SANE_STATUS_INVAL;
    }

  if (scanner->transferred_image_size == 0)
    {
      *length = 0;
      DBG (DBG_verbose, "Setting scan count\n");

      ret = hp5590_inc_scan_count (scanner->dn, scanner->proto_flags);
      if (ret != SANE_STATUS_GOOD)
	return ret;

      /* Dont free bulk read state, some bytes could be left
       * for the next images from ADF
       */
      return SANE_STATUS_EOF;
    }

  if (!scanner->bulk_read_state)
    {
      ret = hp5590_low_init_bulk_read_state (&scanner->bulk_read_state);
      if (ret != SANE_STATUS_GOOD)
	{
	  scanner->scanning = SANE_FALSE;
	  return ret;
	}
    }

  *length = max_length;
  if (*length > scanner->transferred_image_size)
    *length = scanner->transferred_image_size;

  if (   scanner->depth == DEPTH_COLOR_24
      || scanner->depth == DEPTH_COLOR_48)
   {
      unsigned int bytes_per_line;
      ret = calc_image_params (scanner,
			       NULL, NULL,
			       &bytes_per_line,
			       NULL, NULL);
      if (ret != SANE_STATUS_GOOD)
	return ret;

     *length -= *length % bytes_per_line;
     DBG (2, "Aligning requested size to bytes per line "
	  "(requested: %u, aligned: %u)\n",
	  max_length, *length);
   }

  ret = hp5590_read (scanner->dn, scanner->proto_flags,
  		     data, *length, scanner->bulk_read_state);
  if (ret != SANE_STATUS_GOOD)
    {
      scanner->scanning = SANE_FALSE;
      return ret;
    }

  scanner->transferred_image_size -= *length;

  ret = convert_to_rgb (scanner, data, *length);
  if (ret != SANE_STATUS_GOOD)
    {
      scanner->scanning = SANE_FALSE;
      return ret;
    }

  ret = convert_lineart (scanner, data, *length);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
void
sane_cancel (SANE_Handle handle)
{
  struct hp5590_scanner	*scanner = handle;
  SANE_Status		ret;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  scanner->scanning = SANE_FALSE;

  if (scanner->dn < 0)
   return; 

  hp5590_low_free_bulk_read_state (&scanner->bulk_read_state);

  ret = hp5590_stop_scan (scanner->dn, scanner->proto_flags);
  if (ret != SANE_STATUS_GOOD)
    return;
}

/******************************************************************************/

SANE_Status
sane_set_io_mode (SANE_Handle __sane_unused__ handle,
		  SANE_Bool __sane_unused__ non_blocking)
{
  DBG (DBG_proc, "%s\n", __FUNCTION__);  

  return SANE_STATUS_UNSUPPORTED;
}

/******************************************************************************/
SANE_Status
sane_get_select_fd (SANE_Handle __sane_unused__ handle,
		    SANE_Int __sane_unused__ * fd)
{
  DBG (DBG_proc, "%s\n", __FUNCTION__);  

  return SANE_STATUS_UNSUPPORTED;
}

/* vim: sw=2 ts=8
 */
