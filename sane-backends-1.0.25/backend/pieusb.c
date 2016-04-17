/* sane - Scanner Access Now Easy.

   pieusb.c

   Copyright (C) 2012-2015 Jan Vleeshouwers, Michael Rickmann, Klaus Kaempf

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
   If you do not wish that, delete this exception notice.  */

/* =========================================================================
 *
 * SANE interface to three Reflecta USB scanners with USB-id 0x05e3/0x0145:
 * - Reflecta CrystalScan 7200 (model id 0x30)
 * - Reflecta ProScan 7200 (model id 0x36)
 * - Reflecta 6000 Multiple Slide Scanner (model id 0x3A)
 *
 * ========================================================================= */

#define DEBUG_NOT_STATIC
/* --------------------------------------------------------------------------
 *
 * INCLUDES
 *
 * --------------------------------------------------------------------------*/

#include "../include/sane/config.h"
/* Standard includes for various utiliy functions */
#include <stdio.h> /* for FILE */
#include <string.h> /* for strlen */
#include <stdlib.h> /* for NULL */
#include <stdint.h>
#include <math.h>

/* Configuration defines */
#include "../include/sane/config.h"

/* SANE includes */
#include "../include/sane/sane.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/sanei_config.h"

/* Backend includes */
#define BACKEND_NAME pieusb
#include "../include/sane/sanei_backend.h"
#include "pieusb.h"
#include "pieusb_specific.h"

#define CAN_DO_4_CHANNEL_TIFF

#ifdef CAN_DO_4_CHANNEL_TIFF
extern void write_tiff_rgbi_header (FILE *fptr, int width, int height, int depth, int resolution, const char *icc_profile);
#endif

/* --------------------------------------------------------------------------
 *
 * DEFINES
 *
 * --------------------------------------------------------------------------*/

/* Build number of this backend */
#define BUILD 1

/* Configuration filename */
#define PIEUSB_CONFIG_FILE "pieusb.conf"

/* Debug error levels */
#define DBG_error        1      /* errors */
#define DBG_warning      3      /* warnings */
#define DBG_info         5      /* information */
#define DBG_info_sane    7      /* information sane interface level */
#define DBG_inquiry      8      /* inquiry data */
#define DBG_info_proc    9      /* information pieusb backend functions */
#define DBG_info_scan   11      /* information scanner commands */
#define DBG_info_usb    13      /* information usb level functions */

/* --------------------------------------------------------------------------
 *
 * SUPPORTED DEVICES SPECIFICS
 *
 * --------------------------------------------------------------------------*/

struct Pieusb_USB_Device_Entry* pieusb_supported_usb_device_list = NULL;
struct Pieusb_USB_Device_Entry pieusb_supported_usb_device; /* for searching */

/* --------------------------------------------------------------------------
 *
 * LISTS OF ACTIVE DEVICE DEFINITIONS AND SCANNERS
 *
 * --------------------------------------------------------------------------*/

Pieusb_Device_Definition *pieusb_definition_list_head = NULL;
static Pieusb_Scanner *first_handle = NULL;
static const SANE_Device **devlist = NULL;

/* --------------------------------------------------------------------------
 *
 * SANE INTERFACE
 *
 * --------------------------------------------------------------------------*/

/**
 * Initializes the debugging system, the USB system, the version code and
 * 'attaches' available scanners, i.e. creates device definitions for all
 * scanner devices found.
 *
 * @param version_code
 * @param authorize
 * @return SANE_STATUS_GOOD
 */
SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback __sane_unused__ authorize)
{
    FILE *fp;
    char config_line[PATH_MAX];
    SANE_Word vendor_id;
    SANE_Word product_id;
    SANE_Word model_number;
    SANE_Status status;
    int i;

    /* Initialize debug logging */
    DBG_INIT ();

    DBG (DBG_info_sane, "sane_init() build %d\n", BUILD);

    /* Set version code to current major, minor and build number */
    /* TODO: use V_MINOR instead or SANE_CURRENT_MINOR? If so, why?  */
    if (version_code)
        *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, SANE_CURRENT_MINOR, BUILD);

    /* Initialize usb */
    sanei_usb_init ();
    sanei_usb_set_timeout (30 * 1000); /* 30 sec timeout */

    /* There are currently 3 scanners hardcoded into this backend, see below.
     * The config file may add other scanners using a line like
     * usb 0x1234 0x5678 0x9A
     * where the first two hex numbers are vendor and product id, and the last
     * number is the model number. Unfortunately, this is not according to the
     * config file standard. Anyone any suggestions? */

    /* Create default list */
    pieusb_supported_usb_device_list = calloc (4, sizeof(struct Pieusb_USB_Device_Entry));
    if (pieusb_supported_usb_device_list == NULL)
      return SANE_STATUS_NO_MEM;
    /* Reflecta CrystalScan 7200, model number 0x30 */
    pieusb_supported_usb_device_list[0].vendor = 0x05e3;
    pieusb_supported_usb_device_list[0].product = 0x0145;
    pieusb_supported_usb_device_list[0].model = 0x30;
    /* Reflecta ProScan 7200, model number 0x36 */
    pieusb_supported_usb_device_list[1].vendor = 0x05e3;
    pieusb_supported_usb_device_list[1].product = 0x0145;
    pieusb_supported_usb_device_list[1].model = 0x36;
    /* Reflecta 6000 Multiple Slide Scanner */
    pieusb_supported_usb_device_list[2].vendor = 0x05e3;
    pieusb_supported_usb_device_list[2].product = 0x0142;
    pieusb_supported_usb_device_list[2].model = 0x3a;
    /* end of list */
    pieusb_supported_usb_device_list[3].vendor = 0;
    pieusb_supported_usb_device_list[3].product = 0;
    pieusb_supported_usb_device_list[3].model = 0;

    /* Add entries from config file */
    fp = sanei_config_open (PIEUSB_CONFIG_FILE);
    if (!fp) {
        DBG (DBG_info_sane, "sane_init() did not find a config file, using default list of supported devices\n");
    } else {
        while (sanei_config_read (config_line, sizeof (config_line), fp)) {
            /* Ignore line comments and empty lines */
            if (config_line[0] == '#') continue;
            if (strlen (config_line) == 0) continue;
            /* Ignore lines which do not begin with 'usb ' */
            if (strncmp (config_line, "usb ", 4) != 0) continue;
            /* Parse vendor-id, product-id and model number and add to list */
            DBG (DBG_info_sane, "sane_init() config file parsing %s\n", config_line);
            status = sanei_pieusb_parse_config_line(config_line, &vendor_id, &product_id, &model_number);
            if (status == SANE_STATUS_GOOD) {
                DBG (DBG_info_sane, "sane_init() config file lists device %04x %04x %02x\n",vendor_id, product_id, model_number);
                if (!sanei_pieusb_supported_device_list_contains(vendor_id, product_id, model_number)) {
                    DBG (DBG_info_sane, "sane_init() adding device %04x %04x %02x\n",vendor_id, product_id, model_number);
                    sanei_pieusb_supported_device_list_add(vendor_id, product_id, model_number);
                } else {
                    DBG (DBG_info_sane, "sane_init() list already contains %04x %04x %02x\n", vendor_id, product_id, model_number);
                }
            } else {
                DBG (DBG_info_sane, "sane_init() config file parsing %s: error\n", config_line);
            }
	}
        fclose (fp);
    }

    /* Loop through supported device list */
    i = 0;
    while (pieusb_supported_usb_device_list[i].vendor != 0) {
        /* Check if the supported device is present. If so, create a device
         * definition structure for it.
         * The variable pieusb_supported_usb_device is set to current values,
         * which are used in the callback. */
        pieusb_supported_usb_device.vendor = pieusb_supported_usb_device_list[i].vendor;
        pieusb_supported_usb_device.product = pieusb_supported_usb_device_list[i].product;
        pieusb_supported_usb_device.model = pieusb_supported_usb_device_list[i].model;
        pieusb_supported_usb_device.device_number = -1; /* No device number (yet) */
        DBG( DBG_info_sane, "sane_init() looking for Reflecta scanner %04x %04x model %02x\n", pieusb_supported_usb_device.vendor, pieusb_supported_usb_device.product, pieusb_supported_usb_device.model);
        sanei_usb_find_devices (pieusb_supported_usb_device.vendor, pieusb_supported_usb_device.product, sanei_pieusb_find_device_callback);
        i++;
    }
    return SANE_STATUS_GOOD;
}

/**
 * Backend exit.
 * Clean up allocated memory.
 */
void
sane_exit (void)
{
    Pieusb_Device_Definition *dev, *next;

    DBG (DBG_info_sane, "sane_exit()\n");

    for (dev = pieusb_definition_list_head; dev; dev = next) {
        next = dev->next;
        free((void *)dev->sane.name);
        free((void *)dev->sane.vendor);
        free((void *)dev->sane.model);
        free (dev->version);
        free (dev);
    }
    pieusb_definition_list_head = NULL;

    if (devlist) {
        free (devlist);
        devlist = NULL;
    }
}

/**
 * Create a SANE device list from the device list generated by sane_init().
 *
 * @param device_list List of SANE_Device elements
 * @param local_only If true, disregard network scanners. Not applicable for USB scanners.
 * @return SANE_STATUS_GOOD, or SANE_STATUS_NO_MEM if the list cannot be allocated
 */
SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool __sane_unused__ local_only)
{
    Pieusb_Device_Definition *dev;
    int i;

    DBG (DBG_info_sane, "sane_get_devices\n");

    /* Create SANE_DEVICE list from device list created in sane_init() */
    i = 0;
    for (dev = pieusb_definition_list_head; dev; dev = dev->next) {
        i++;
    }
    if (devlist) {
        free (devlist);
    }
    devlist = malloc ((i + 1) * sizeof (devlist[0]));
    if (!devlist) {
        return SANE_STATUS_NO_MEM;
    }
    i = 0;
    for (dev = pieusb_definition_list_head; dev; dev = dev->next) {
        devlist[i++] = &dev->sane;
    }
    devlist[i] = NULL;
    *device_list = devlist;
    return SANE_STATUS_GOOD;
}

/**
 * Open the scanner with the given devicename and return a handle to it, which
 * is a pointer to a Pieusb_Scanner struct. The handle will be an input to
 * a couple of other functions of the SANE interface.
 *
 * @param devicename Name of the device, corresponds to SANE_Device.name
 * @param handle handle to scanner (pointer to a Pieusb_Scanner struct)
 * @return SANE_STATUS_GOOD if the device has been opened
 */
SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
    Pieusb_Device_Definition *dev;
    SANE_Status status;
    Pieusb_Scanner *scanner, *s;

    DBG (DBG_info_sane, "sane_open(%s)\n", devicename);

    /* Search for devicename */
    if (devicename[0]) {
        for (dev = pieusb_definition_list_head; dev; dev = dev->next) {
	  if (strcmp (dev->sane.name, devicename) == 0) {
	      break;
	  }
        }
        if (!dev) {
            /* Is it a valid USB device? */
            SANE_Word vendor;
            SANE_Word product;
            int i = 0;

            status = sanei_usb_get_vendor_product_byname (devicename, &vendor, &product);
            if (status != SANE_STATUS_GOOD) {
                DBG (DBG_error, "sane_open: sanei_usb_get_vendor_product_byname failed %s\n", devicename);
                return status;
            }
            /* Get vendor-product-model & verify that is is supported */
            /* Loop through supported device list */
            while (pieusb_supported_usb_device_list[i].vendor != 0) {
                /* Check if vendor and product match */
                if (pieusb_supported_usb_device_list[i].vendor == vendor
                        && pieusb_supported_usb_device_list[i].product == product) {
                    /* Check if a supported device is present
                     * If so, create a device definition structure for it. */
                    /* Set pieusb_supported_usb_device to current values: these are used in callback */
                    pieusb_supported_usb_device.vendor = vendor;
                    pieusb_supported_usb_device.product = product;
                    pieusb_supported_usb_device.model = pieusb_supported_usb_device_list[i].model;
                    pieusb_supported_usb_device.device_number = -1;
                    sanei_usb_find_devices (vendor, product, sanei_pieusb_find_device_callback);
                    if (pieusb_supported_usb_device.device_number == -1) {
                        /* Did not succeed in opening the USB device, which is an error.
                         * This error is not caught by sanei_usb_find_devices(), so handle
                         * it here. */
                        DBG (DBG_error, "sane_open: sanei_usb_find_devices did not open device %s\n", devicename);
                        return SANE_STATUS_INVAL;
                    }
                }
                i++;
            }
            /* Now rescan the device list to see if it is present */
            for (dev = pieusb_definition_list_head; dev; dev = dev->next) {
              if (strcmp (dev->sane.name, devicename) == 0) {
                  break;
              }
            }
	}
    } else {
        /* empty devicename -> use first device */
        dev = pieusb_definition_list_head;
    }
    /* If no device found, return error */
    if (!dev) {
        return SANE_STATUS_INVAL;
    }

    /* Now create a scanner structure to return */

    /* Check if we are not opening the same scanner again. */
    for (s = first_handle; s; s = s->next) {
        if (s->device->sane.name == devicename) {
            *handle = s;
            return SANE_STATUS_GOOD;
        }
    }

    /* Create a new scanner instance */
    scanner = malloc (sizeof (*scanner));
    if (!scanner) {
        return SANE_STATUS_NO_MEM;
    }
    memset (scanner, 0, sizeof (*scanner));
    scanner->device = dev;
    sanei_usb_open (dev->sane.name, &scanner->device_number);
    scanner->cancel_request = 0;
    scanner->shading_data_present = SANE_FALSE;
    /* Options and buffers */
    (void)sanei_pieusb_init_options (scanner);

    /* wait for warmup */
    status = sanei_pieusb_wait_ready (scanner, 0);
    if (status != SANE_STATUS_GOOD) {
      sanei_usb_close(scanner->device_number);
      free (scanner);
      DBG (DBG_error, "sane_open: scanner not ready\n");
      return status;
    }

    /* First time settings */
    /* ? */
    /* Insert newly opened handle into list of open handles: */
    scanner->next = first_handle;
    first_handle = scanner;

    *handle = scanner;
    return SANE_STATUS_GOOD;
}

/**
 * Close the scanner and remove the scanner from the list of active scanners.
 *
 * @param handle Scanner handle
 */
void
sane_close (SANE_Handle handle)
{
    Pieusb_Scanner *prev, *scanner;
    SANE_Int k;

    DBG (DBG_info_sane, "sane_close()\n");

    /* Find handle in list of open handles: */
    prev = 0;
    for (scanner = first_handle; scanner; scanner = scanner->next)  {
        if (scanner == handle) {
            break;
        }
        prev = scanner;
    }
    /* Not a handle we know about. This may happen since all different backend
     * scanner instances are all cast to SANE_Handle (a void pointer) */
    if (!scanner) {
        DBG (DBG_error, "sane_close(): invalid handle %p\n", handle);
        return;
    }

    /* Stop scan if still scanning */
    if (scanner->scanning) {
        sanei_pieusb_on_cancel(scanner);
    }

    /* USB scanners may be still open here */
    if (scanner->device_number >= 0) {
        sanei_usb_reset (scanner->device_number);
        sanei_usb_close (scanner->device_number);
    }
    /* Remove handle from list */
    if (prev) {
        prev->next = scanner->next;
    } else {
        first_handle = scanner->next;
    }

    /* Free scanner related allocated memory and the scanner itself */
    /*TODO: check if complete */
    if (scanner->buffer.data) sanei_pieusb_buffer_delete(&scanner->buffer);
    free (scanner->ccd_mask);
    for (k=0; k<4; k++) free (scanner->shading_ref[k]);
    free (scanner->val[OPT_MODE].s);
    free (scanner->val[OPT_HALFTONE_PATTERN].s);
    free (scanner);
}

/**
 * Get option descriptor. Return the option descriptor with the given index
 *
 * @param handle Scanner handle
 * @param option Index of option descriptor to return
 * @return The option descriptor
 */
const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
    Pieusb_Scanner *scanner = handle;

    DBG (DBG_info_proc, "sane_get_option_descriptor() option=%d\n", option);

    if ((unsigned) option >= NUM_OPTIONS)
    {
      return 0;
    }

    return scanner->opt + option;
}

/**
 * Set or inquire the current value of option number 'option' of the device
 * represented by the given handle.
 *
 * @param handle Scanner handle
 * @param option Index of option to set or get
 * @param action Determines if the option value is read or set
 * @param val Pointer to value to set or get
 * @param info About set result. May be NULL.
 * @return SANE_STATUS_GOOD, or SANE_STATUS_INVAL if a parameter cannot be set
 */
SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option, SANE_Action action,
		     void *val, SANE_Int * info)
{
    Pieusb_Scanner *scanner = handle;
    SANE_Status status;
    SANE_Word cap;
    SANE_String_Const name;

    DBG(DBG_info_sane,"sane_control_option()\n");
    if (info) {
        *info = 0;
    }

    /* Don't set or get options while the scanner is busy */
    if (scanner->scanning) {
        DBG(DBG_error,"Device busy scanning, no option returned\n");
        return SANE_STATUS_DEVICE_BUSY;
    }

    /* Check if option index is between bounds */
    if ((unsigned) option >= NUM_OPTIONS) {
        DBG(DBG_error,"Index too large, no option returned\n");
        return SANE_STATUS_INVAL;
    }

    /* Check if option is switched on */
    cap = scanner->opt[option].cap;
    if (!SANE_OPTION_IS_ACTIVE (cap))
    {
        DBG(DBG_error,"Option inactive (%s)\n", scanner->opt[option].name);
        return SANE_STATUS_INVAL;
    }

    /* Get name of option */
    name = scanner->opt[option].name;
    if (!name)
    {
      name = "(no name)";
    }

    /* */
    switch (action) {
        case SANE_ACTION_GET_VALUE:

            DBG (DBG_info_sane, "get %s [#%d]\n", name, option);

            switch (option) {

                /* word options: */
                case OPT_NUM_OPTS:
                case OPT_BIT_DEPTH:
                case OPT_RESOLUTION:
                case OPT_TL_X:
                case OPT_TL_Y:
                case OPT_BR_X:
                case OPT_BR_Y:
                case OPT_THRESHOLD:
                case OPT_SHARPEN:
                case OPT_SHADING_ANALYSIS:
                case OPT_FAST_INFRARED:
	        case OPT_ADVANCE_SLIDE:
                case OPT_CORRECT_SHADING:
                case OPT_CORRECT_INFRARED:
                case OPT_CLEAN_IMAGE:
                case OPT_SMOOTH_IMAGE:
                case OPT_TRANSFORM_TO_SRGB:
                case OPT_INVERT_IMAGE:
                case OPT_PREVIEW:
                case OPT_SAVE_SHADINGDATA:
                case OPT_SAVE_CCDMASK:
	        case OPT_LIGHT:
	        case OPT_DOUBLE_TIMES:
                case OPT_SET_EXPOSURE_R:
                case OPT_SET_EXPOSURE_G:
                case OPT_SET_EXPOSURE_B:
                case OPT_SET_EXPOSURE_I:
                case OPT_SET_GAIN_R:
                case OPT_SET_GAIN_G:
                case OPT_SET_GAIN_B:
                case OPT_SET_GAIN_I:
                case OPT_SET_OFFSET_R:
                case OPT_SET_OFFSET_G:
                case OPT_SET_OFFSET_B:
                case OPT_SET_OFFSET_I:
                    *(SANE_Word *) val = scanner->val[option].w;
                    DBG (DBG_info_sane, "get %s [#%d] val=%d\n", name, option,scanner->val[option].w);
                    return SANE_STATUS_GOOD;

                /* word-array options: => for exposure gain offset? */
                case OPT_CROP_IMAGE:
                    memcpy (val, scanner->val[option].wa, scanner->opt[option].size);
                    return SANE_STATUS_GOOD;

                /* string options */
                case OPT_MODE:
                case OPT_CALIBRATION_MODE:
                case OPT_GAIN_ADJUST:
                case OPT_HALFTONE_PATTERN:
                    strcpy (val, scanner->val[option].s);
                    DBG (DBG_info_sane, "get %s [#%d] val=%s\n", name, option,scanner->val[option].s);
                    return SANE_STATUS_GOOD;
            }
            break;

        case SANE_ACTION_SET_VALUE:

            switch (scanner->opt[option].type) {
                case SANE_TYPE_INT:
                    DBG (DBG_info_sane, "set %s [#%d] to %d, size=%d\n", name, option, *(SANE_Word *) val, scanner->opt[option].size);
                    break;
                case SANE_TYPE_FIXED:
                    DBG (DBG_info_sane, "set %s [#%d] to %f\n", name, option, SANE_UNFIX (*(SANE_Word *) val));
                    break;
                case SANE_TYPE_STRING:
                    DBG (DBG_info_sane, "set %s [#%d] to %s\n", name, option, (char *) val);
                    break;
                case SANE_TYPE_BOOL:
                    DBG (DBG_info_sane, "set %s [#%d] to %d\n", name, option, *(SANE_Word *) val);
                    break;
                default:
                    DBG (DBG_info_sane, "set %s [#%d]\n", name, option);
            }
            /* Check if option can be set */
            if (!SANE_OPTION_IS_SETTABLE (cap)) {
              return SANE_STATUS_INVAL;
            }
            /* Check if new value within bounds */
            status = sanei_constrain_value (scanner->opt + option, val, info);
            if (status != SANE_STATUS_GOOD) {
              return status;
            }
            /* Set option and handle info return */
            switch (option)
            {
                /* (mostly) side-effect-free word options: */
                case OPT_BIT_DEPTH:
                case OPT_RESOLUTION:
                case OPT_TL_X:
                case OPT_TL_Y:
                case OPT_BR_X:
                case OPT_BR_Y:
                case OPT_SHARPEN:
                case OPT_SHADING_ANALYSIS:
                case OPT_FAST_INFRARED:
                    if (info) {
                        *info |= SANE_INFO_RELOAD_PARAMS;
                    }
                  /* fall through */
                case OPT_NUM_OPTS:
                case OPT_PREVIEW:
	        case OPT_ADVANCE_SLIDE:
                case OPT_CORRECT_SHADING:
                case OPT_CORRECT_INFRARED:
                case OPT_CLEAN_IMAGE:
                case OPT_SMOOTH_IMAGE:
                case OPT_TRANSFORM_TO_SRGB:
                case OPT_INVERT_IMAGE:
                case OPT_SAVE_SHADINGDATA:
                case OPT_SAVE_CCDMASK:
                case OPT_THRESHOLD:
	        case OPT_LIGHT:
	        case OPT_DOUBLE_TIMES:
                case OPT_SET_GAIN_R:
                case OPT_SET_GAIN_G:
                case OPT_SET_GAIN_B:
                case OPT_SET_GAIN_I:
                case OPT_SET_OFFSET_R:
                case OPT_SET_OFFSET_G:
                case OPT_SET_OFFSET_B:
                case OPT_SET_OFFSET_I:
                case OPT_SET_EXPOSURE_R:
                case OPT_SET_EXPOSURE_G:
                case OPT_SET_EXPOSURE_B:
                case OPT_SET_EXPOSURE_I:
                    scanner->val[option].w = *(SANE_Word *) val;
                    break;

                /* side-effect-free word-array options: */
                case OPT_CROP_IMAGE:
                    memcpy (scanner->val[option].wa, val, scanner->opt[option].size);
                    break;

                /* options with side-effects: */
                case OPT_MODE:
                {
                    /* Free current setting */
                    if (scanner->val[option].s) {
                        free (scanner->val[option].s);
                    }
                    /* New setting */
                    scanner->val[option].s = (SANE_Char *) strdup (val);
                    /* Info */
                    if (info) {
                        *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
                    }
                    break;
                }

                case OPT_CALIBRATION_MODE:
                case OPT_GAIN_ADJUST:
                case OPT_HALFTONE_PATTERN:
                {
                     /* Free current setting */
                    if (scanner->val[option].s) {
                        free (scanner->val[option].s);
                    }
                    /* New setting */
                    scanner->val[option].s = (SANE_Char *) strdup (val);
                    break;
                }

            }

            /* Check the whole set */
            if (sanei_pieusb_analyse_options(scanner)) {
                return SANE_STATUS_GOOD;
            } else {
                return SANE_STATUS_INVAL;
            }

            break;
        default:
            return SANE_STATUS_INVAL;
            break;
    }
    return SANE_STATUS_INVAL;
}

/**
 * Obtain the current scan parameters. The returned parameters are guaranteed
 * to be accurate between the time a scan has been started (sane start() has
 * been called) and the completion of that request. Outside of that window, the
 * returned values are best-effort estimates of what the parameters will be when
 * sane start() gets invoked. - says the SANE standard.
 *
 * @param handle Scanner handle
 * @param params Scan parameters
 * @return SANE_STATUS_GOOD
 */
SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
    Pieusb_Scanner *scanner = handle;
    const char *mode;
    double resolution, width, height;
    SANE_Int colors;

    DBG (DBG_info_sane, "sane_get_parameters\n");

    if (params) {

        if (scanner->scanning) {
            /* sane_start() initialized a SANE_Parameters struct in the scanner */
            DBG (DBG_info_sane, "sane_get_parameters from scanner values\n");
            params->bytes_per_line = scanner->scan_parameters.bytes_per_line;
            params->depth = scanner->scan_parameters.depth;
            params->format = scanner->scan_parameters.format;
            params->last_frame = scanner->scan_parameters.last_frame;
            params->lines = scanner->scan_parameters.lines;
            params->pixels_per_line = scanner->scan_parameters.pixels_per_line;
        } else {
            /* Calculate appropriate values from option settings */
            DBG (DBG_info_sane, "sane_get_parameters from option values\n");
            if (scanner->val[OPT_PREVIEW].b) {
                resolution = scanner->device->fast_preview_resolution;
            } else {
                resolution = SANE_UNFIX(scanner->val[OPT_RESOLUTION].w);
            }
            DBG (DBG_info_sane, "  resolution %f\n", resolution);
            width = SANE_UNFIX(scanner->val[OPT_BR_X].w)-SANE_UNFIX(scanner->val[OPT_TL_X].w);
            height = SANE_UNFIX(scanner->val[OPT_BR_Y].w)-SANE_UNFIX(scanner->val[OPT_TL_Y].w);
            DBG (DBG_info_sane, "  width x height: %f x %f\n", width, height);
            params->lines = height / MM_PER_INCH * resolution;
            params->pixels_per_line = width / MM_PER_INCH * resolution;
            mode = scanner->val[OPT_MODE].s;
            if (strcmp(mode, SANE_VALUE_SCAN_MODE_LINEART) == 0) {
                params->format = SANE_FRAME_GRAY;
                params->depth = 1;
                colors = 1;
            } else if(strcmp(mode, SANE_VALUE_SCAN_MODE_HALFTONE) == 0) {
                params->format = SANE_FRAME_GRAY;
                params->depth = 1;
                colors = 1;
            } else if(strcmp(mode, SANE_VALUE_SCAN_MODE_GRAY) == 0) {
                params->format = SANE_FRAME_GRAY;
                params->depth = scanner->val[OPT_BIT_DEPTH].w;
                colors = 1;
            } else if(strcmp(mode, SANE_VALUE_SCAN_MODE_RGBI) == 0) {
                params->format = SANE_FRAME_RGB; /* was: SANE_FRAME_RGBI */
                params->depth = scanner->val[OPT_BIT_DEPTH].w;
                colors = 4;
            } else { /* SANE_VALUE_SCAN_MODE_COLOR */
                params->format = SANE_FRAME_RGB;
                params->depth = scanner->val[OPT_BIT_DEPTH].w;
                colors = 3;
            }
            DBG (DBG_info_sane, "  colors: %d\n", colors);
            if (params->depth == 1) {
                params->bytes_per_line = colors * (params->pixels_per_line + 7)/8;
            } else if (params->depth <= 8) {
                params->bytes_per_line = colors * params->pixels_per_line;
            } else if (params->depth <= 16) {
                params->bytes_per_line = 2 * colors * params->pixels_per_line;
            }
            params->last_frame = SANE_TRUE;
        }

        DBG(DBG_info_sane,"sane_get_parameters(): SANE parameters\n");
        DBG(DBG_info_sane," format = %d\n",params->format);
        DBG(DBG_info_sane," last_frame = %d\n",params->last_frame);
        DBG(DBG_info_sane," bytes_per_line = %d\n",params->bytes_per_line);
        DBG(DBG_info_sane," pixels_per_line = %d\n",params->pixels_per_line);
        DBG(DBG_info_sane," lines = %d\n",params->lines);
        DBG(DBG_info_sane," depth = %d\n",params->depth);

    } else {

        DBG(DBG_info_sane," no params argument, no values returned\n");

    }

    return SANE_STATUS_GOOD;
}

/**
 * Initiates aquisition of an image from the scanner.
 * SCAN Phase 1: initialization and calibration
 * (SCAN Phase 2: line-by-line scan & read is not implemented)
 * SCAN Phase 3: get CCD-mask
 * SCAN phase 4: scan slide and save data in scanner buffer

 * @param handle Scanner handle
 * @return
 */
SANE_Status
sane_start (SANE_Handle handle)
{
    struct Pieusb_Scanner *scanner = handle;
    struct Pieusb_Command_Status status;
    SANE_Byte colors;
    const char *mode;
    SANE_Bool shading_correction_relevant;
    SANE_Bool infrared_post_processing_relevant;
    SANE_Status st;
    SANE_Int bytes_per_line;

    SANE_Int shading_width;
    SANE_Int shading_idx;

    struct Pieusb_Exposure_Time exptime = {
      0x93, /* code 0x93 */
      3 * 2 * sizeof(SANE_Int), /* number of bytes in rest of structure */
      { { 0x02, 100 }, { 0x04, 100 }, { 0x08, 100 } }
    };

    struct Pieusb_Highlight_Shadow shadow = {
      0x94, /* code 0x94 */
      3 * 2 * sizeof(SANE_Int), /* number of bytes in rest of structure */
      { { 0x02, 100 }, { 0x04, 100 }, { 0x08, 100 } }
    };

    DBG (DBG_info_sane, "sane_start()\n");

    /* ----------------------------------------------------------------------
     *
     * Exit if currently scanning
     *
     * ---------------------------------------------------------------------- */
    if (scanner->scanning) {
        DBG (DBG_error, "sane_start(): scanner is already scanning, exiting\n");
        return SANE_STATUS_DEVICE_BUSY;
    }

    /* ----------------------------------------------------------------------
     *
     * Exit with pause if not warmed up
     *
     * ---------------------------------------------------------------------- */

    sanei_pieusb_cmd_read_state (scanner->device_number, &(scanner->state), &status);
    if (status.pieusb_status != PIEUSB_STATUS_GOOD) {
        if (status.pieusb_status == PIEUSB_STATUS_DEVICE_BUSY)
	  return SANE_STATUS_DEVICE_BUSY; /* was: SANE_STATUS_WARMING_UP */
        DBG (DBG_error, "sane_start(): warmed up check returns status: %s\n",  sane_strstatus (sanei_pieusb_convert_status(status.pieusb_status)));
        return SANE_STATUS_IO_ERROR;
    }
    if (scanner->state.warmingUp) {
        DBG (DBG_error, "sane_start(): warming up, exiting\n");
        /* Seen SANE_STATUS_WARMING_UP in scanimage => enabled */
        sleep (10); /* scanimage does not pause, so do it here */
        return SANE_STATUS_DEVICE_BUSY; /* was: SANE_STATUS_WARMING_UP */
    }

    /* ----------------------------------------------------------------------
     * set exposure time
     * ---------------------------------------------------------------------- */

    sanei_pieusb_cmd_set_exposure_time (scanner->device_number, &exptime, &status);
    if (status.pieusb_status != PIEUSB_STATUS_GOOD) {
      DBG (DBG_error, "sane_start(): sanei_pieusb_cmd_set_exposure_time failed: %d\n", status.pieusb_status);
      return SANE_STATUS_IO_ERROR;
    }

    /* ----------------------------------------------------------------------
     * set highlight shadow
     * ---------------------------------------------------------------------- */

    sanei_pieusb_cmd_set_highlight_shadow (scanner->device_number, &shadow, &status);
    if (status.pieusb_status != PIEUSB_STATUS_GOOD) {
      DBG (DBG_error, "sane_start(): sanei_pieusb_cmd_set_highlight_shadow failed: %d\n", status.pieusb_status);
      return SANE_STATUS_IO_ERROR;
    }

    /* ----------------------------------------------------------------------
     * get calibration info
     * ---------------------------------------------------------------------- */

    sanei_pieusb_cmd_get_shading_parms (scanner->device_number, scanner->device->shading_parameters, &status);
    if (status.pieusb_status != PIEUSB_STATUS_GOOD) {
      DBG (DBG_error, "sane_scan: sanei_pieusb_cmd_get_shading_parms failed: %d\n", status.pieusb_status);
      return SANE_STATUS_INVAL;
    }
    shading_width = scanner->device->shading_parameters[0].pixelsPerLine;
    DBG (DBG_info, "shading_width %d\n", shading_width);
    for (shading_idx = 0; shading_idx < SHADING_PARAMETERS_INFO_COUNT; shading_idx++) {
      scanner->shading_ref[shading_idx] =
        realloc(scanner->shading_ref[shading_idx], 2 * shading_width * sizeof(SANE_Int));
      if (scanner->shading_ref[shading_idx] == NULL) {
        return SANE_STATUS_NO_MEM;
      }
    }
    scanner->ccd_mask = realloc (scanner->ccd_mask, shading_width);
    scanner->ccd_mask_size = shading_width;
    if (scanner->ccd_mask == NULL) {
      return SANE_STATUS_NO_MEM;
    }

    /* ----------------------------------------------------------------------
     *
     * Standard run does;
     * - set exposure time 0x0A/0x13
     * - set highlight shadow 0x0A/0x14
     * - read shading parameters 0x0A/0x95/0x08
     * - set scan frame 0x0A/0x12
     *   "12 00 0a00 80 00 0300 0000 b829 e31a"
     *    => 0:12 1:0 2:10 4:80 5:0 6:3 8:0 10:10680 12:6883
     * - read gain offset 0xD7
     * - set gain offset 0xDC
     * - set mode 0x15
     *   "00 0f   2c01 80   04  04  00 01    0a     00 00 00  80  10 00"
     *       size res  pass dpt frm    ord   bitmap       ptn thr
     *       15   300  RGB  8   inx    intel 1=sharpen    0   128
     *                                       3=skipshad
     *
     * ---------------------------------------------------------------------- */

    /* ----------------------------------------------------------------------
     *
     * Show and check options
     *
     * ---------------------------------------------------------------------- */
    sanei_pieusb_print_options (scanner);
    if (!sanei_pieusb_analyse_options (scanner)) {
        return SANE_STATUS_IO_ERROR;
    }

    /* ----------------------------------------------------------------------
     *
     * Set scan frame
     *
     * ---------------------------------------------------------------------- */
    if (sanei_pieusb_set_frame_from_options (scanner) != SANE_STATUS_GOOD) {
        return SANE_STATUS_IO_ERROR;
    }

    /* ----------------------------------------------------------------------
     *
     * Function 17
     *
     * ---------------------------------------------------------------------- */

    sanei_pieusb_cmd_17 (scanner->device_number, 1, &status);
    if (status.pieusb_status != PIEUSB_STATUS_GOOD) {
      DBG (DBG_error, "sane_start(): sanei_pieusb_cmd_17 failed: %d\n", status.pieusb_status);
      return SANE_STATUS_IO_ERROR;
    }
    st = sanei_pieusb_wait_ready (scanner, 0);
    if (st != SANE_STATUS_GOOD) {
      DBG (DBG_error, "sane_start(): scanner not ready after sanei_pieusb_cmd_17: %d\n", st);
      return st;
    }

    /* ----------------------------------------------------------------------
     *
     * Get & set initial gains and offsets
     *
     * There does not seem to be much reason to set exposure/gain/offset
     * now, but it does make a large difference in speed, because it
     * creates a small BADF-table. This is probably because without SET GAIN
     * OFFSET, extraEntries has a random value (it is not initialised).
     *
     * TODO: test if this may be done just once, in sane_open().
     *
     * ---------------------------------------------------------------------- */

    if (sanei_pieusb_set_gain_offset (scanner, scanner->val[OPT_CALIBRATION_MODE].s) != SANE_STATUS_GOOD) {
        return SANE_STATUS_IO_ERROR;
    }

    st = sanei_pieusb_wait_ready (scanner, 0);
    if (st != SANE_STATUS_GOOD) {
      DBG (DBG_error, "sane_start: scanner not ready %d\n", st);
      return st;
    }

    /* ----------------------------------------------------------------------
     *
     * Set mode
     *
     * ---------------------------------------------------------------------- */
    if (sanei_pieusb_set_mode_from_options (scanner) != SANE_STATUS_GOOD) {
        return SANE_STATUS_IO_ERROR;
    }

    /* ----------------------------------------------------------------------
     *
     * Lamp on
     *
     * ---------------------------------------------------------------------- */
    sanei_pieusb_cmd_slide (scanner->device_number, SLIDE_LAMP_ON, &status);
    if (status.pieusb_status != PIEUSB_STATUS_GOOD) {
      DBG (DBG_error, "sane_start(): sanei_pieusb_cmd_slide failed: %d\n", status.pieusb_status);
      return SANE_STATUS_IO_ERROR;
    }
    st = sanei_pieusb_wait_ready (scanner, 0);
    if (st != SANE_STATUS_GOOD) {
      DBG (DBG_error, "sane_start: scanner not ready %d\n", st);
      return st;
    }

    /* Enter SCAN phase 1 */
    DBG (DBG_info_sane, "sane_start(): scan phase 1\n");

    /* ----------------------------------------------------------------------
     *
     * Start scan & wait until device ready
     *
     * ---------------------------------------------------------------------- */
    scanner->scanning = SANE_TRUE;
    scanner->cancel_request = SANE_FALSE;
    for (;;) {
      sanei_pieusb_cmd_start_scan (scanner->device_number, &status);
      if (status.pieusb_status != PIEUSB_STATUS_WARMING_UP)
	break;
      sleep(5);
    }
    sanei_pieusb_wait_ready (scanner, 0);
    if ((status.pieusb_status == PIEUSB_STATUS_MUST_CALIBRATE)
        || (scanner->val[OPT_SHADING_ANALYSIS].b != 0)) {

        /* Overriding skip calibration */
        DBG (DBG_info_sane, "sane_start(): process shading data\n");

        /* ------------------------------------------------------------------
         *
         * Get and set gain and offset
         * Get settings from scanner, from preview data, from options,
         * or use defaults.
         *
         * ------------------------------------------------------------------ */
        if (sanei_pieusb_set_gain_offset (scanner, scanner->val[OPT_CALIBRATION_MODE].s) != SANE_STATUS_GOOD) {
            sanei_pieusb_cmd_stop_scan (scanner->device_number, &status);
            scanner->scanning = SANE_FALSE;
            return SANE_STATUS_IO_ERROR;
        }
        /* ------------------------------------------------------------------
         *
         * Obtain shading data
         * Get parameters from scanner->device->shading_parameters[0] although
         * it's 45 lines, scanner->ccd_mask_size pixels, 16 bit depth in all cases.
         *
         * ------------------------------------------------------------------ */
        if (sanei_pieusb_get_shading_data (scanner) != SANE_STATUS_GOOD) {
            sanei_pieusb_cmd_stop_scan (scanner->device_number, &status);
            scanner->scanning = SANE_FALSE;
            return SANE_STATUS_IO_ERROR;
        }
    }

    /* Enter SCAN phase 2 */
    DBG (DBG_info_sane, "sane_start(): scan phase 2\n");

    /* SCAN phase 2 (line-by-line scan) not implemented */

    st = sanei_pieusb_wait_ready (scanner, 0);
    if (st != SANE_STATUS_GOOD) {
      DBG (DBG_error, "sane_start: scanner not ready %d\n", st);
      return st;
    }

    /* Enter SCAN phase 3 */

    DBG (DBG_info_sane, "sane_start(): scan phase 3\n");

    /* Handle cancel request */
    if (scanner->cancel_request) {
        return sanei_pieusb_on_cancel (scanner);
    }

    /* ----------------------------------------------------------------------
     *
     * Get CCD mask
     *
     * ---------------------------------------------------------------------- */

    if (sanei_pieusb_get_ccd_mask (scanner) != SANE_STATUS_GOOD) {
        sanei_pieusb_cmd_stop_scan (scanner->device_number, &status);
        scanner->scanning = SANE_FALSE;
        return SANE_STATUS_IO_ERROR;
    }

    /* Enter SCAN phase 4 */

    /* ----------------------------------------------------------------------
     *
     * Read scan parameters & wait until ready for reading
     *
     * ---------------------------------------------------------------------- */
    if (sanei_pieusb_get_parameters (scanner, &bytes_per_line) != SANE_STATUS_GOOD) {
        sanei_pieusb_cmd_stop_scan (scanner->device_number, &status);
        scanner->scanning = SANE_FALSE;
        return SANE_STATUS_IO_ERROR;
    }

    st = sanei_pieusb_wait_ready (scanner, 0);
    if (st != SANE_STATUS_GOOD) {
      DBG (DBG_error, "sane_start: scanner not ready %d\n", st);
      return st;
    }

    /* ----------------------------------------------------------------------
     *
     * Prepare read buffer
     * Currently this buffer is always a memory mapped buffer
     * Might be faster to use RAM buffers for small images (such as preview)
     *
     * ---------------------------------------------------------------------- */
    colors = 0x00;
    switch (scanner->mode.passes) {
        case SCAN_FILTER_RED: colors = 0x01; break;
        case SCAN_FILTER_GREEN: colors = 0x02; break;
        case SCAN_FILTER_BLUE: colors = 0x04; break;
        case SCAN_FILTER_INFRARED: colors = 0x08; break;
        case SCAN_ONE_PASS_COLOR: colors = 0x07; break;
        case SCAN_ONE_PASS_RGBI: colors = 0x0F; break;
    }
    if (scanner->buffer.data) sanei_pieusb_buffer_delete(&scanner->buffer); /* free resources from previous invocation */
    st = sanei_pieusb_buffer_create (&(scanner->buffer), scanner->scan_parameters.pixels_per_line,
			       scanner->scan_parameters.lines, colors,
			       scanner->scan_parameters.depth);
    if (st != SANE_STATUS_GOOD) {
        scanner->scanning = SANE_FALSE;
        return st;
    }

    /* ----------------------------------------------------------------------
     *
     * Read all image data into the buffer
     *
     * ---------------------------------------------------------------------- */
    if (sanei_pieusb_get_scan_data (scanner, bytes_per_line) != SANE_STATUS_GOOD) {
        scanner->scanning = SANE_FALSE;
        return SANE_STATUS_IO_ERROR;
    }
    sleep(2);
    st = sanei_pieusb_wait_ready (scanner, 0);
    if (st != SANE_STATUS_GOOD) {
      DBG (DBG_error, "sane_start(): scanner not ready after sanei_pieusb_get_scan_data: %d\n", st);
      scanner->scanning = SANE_FALSE;
      return st;
    }

    /* ----------------------------------------------------------------------
     *
     * Advance to next slide (except for preview)
     *
     * ---------------------------------------------------------------------- */
    if (scanner->val[OPT_ADVANCE_SLIDE].b && !scanner->val[OPT_PREVIEW].b) {
      sanei_pieusb_cmd_slide (scanner->device_number, SLIDE_NEXT, &status);
      if (status.pieusb_status != PIEUSB_STATUS_GOOD) {
	DBG (DBG_error, "sane_start(): sanei_pieusb_cmd_slide failed: %d\n", status.pieusb_status);

      }
    }

    /* ----------------------------------------------------------------------
     *
     * Post processing:
     * 1. Correct for shading
     * 2. Remove R-component from IR data
     * 3. Remove dust
     *
     * ---------------------------------------------------------------------- */

    mode = scanner->val[OPT_MODE].s;
    if (strcmp(mode, SANE_VALUE_SCAN_MODE_LINEART) == 0) {
        shading_correction_relevant = SANE_FALSE; /* Shading correction irrelavant at bit depth 1 */
        infrared_post_processing_relevant = SANE_FALSE; /* No infrared, no postprocessing */
    } else if (strcmp(mode, SANE_VALUE_SCAN_MODE_HALFTONE) == 0) {
        shading_correction_relevant = SANE_FALSE; /* Shading correction irrelavant at bit depth 1 */
        infrared_post_processing_relevant = SANE_FALSE; /* No infrared, no postprocessing */
    } else if (strcmp(mode, SANE_VALUE_SCAN_MODE_GRAY) == 0) {
        shading_correction_relevant = SANE_TRUE;
        infrared_post_processing_relevant = SANE_FALSE; /* No infrared, no postprocessing */
    } else if (scanner->val[OPT_PREVIEW].b) {
        /* Catch preview here, otherwise next ifs get complicated */
        shading_correction_relevant = SANE_TRUE;
        infrared_post_processing_relevant = SANE_FALSE;
    } else if (strcmp(mode, SANE_VALUE_SCAN_MODE_RGBI) == 0) {
        shading_correction_relevant = SANE_TRUE;
        infrared_post_processing_relevant = SANE_TRUE;
    } else if (strcmp(mode, SANE_VALUE_SCAN_MODE_COLOR) == 0 && scanner->val[OPT_CLEAN_IMAGE].b) {
        shading_correction_relevant = SANE_TRUE;
        infrared_post_processing_relevant = SANE_TRUE;
    } else { /* SANE_VALUE_SCAN_MODE_COLOR */
        shading_correction_relevant = SANE_TRUE;
        infrared_post_processing_relevant = SANE_TRUE;
    }
    if (scanner->val[OPT_CORRECT_SHADING].b && shading_correction_relevant) {
        if (scanner->shading_data_present) {
            sanei_pieusb_correct_shading (scanner, &scanner->buffer);
        } else {
            DBG(DBG_warning, "sane_start(): unable to correct for shading, no shading data available\n");
        }
    }
    if ((scanner->val[OPT_CORRECT_INFRARED].b || scanner->val[OPT_CLEAN_IMAGE].b) && !scanner->val[OPT_PREVIEW].b && infrared_post_processing_relevant) {
        /* Create array of pointers to color planes R, G, B, I */
        SANE_Uint *planes[PLANES];
        SANE_Int N;
        N = scanner->buffer.width * scanner->buffer.height;
        planes[0] = scanner->buffer.data;
        planes[1] = scanner->buffer.data + N;
        planes[2] = scanner->buffer.data + 2 * N;
        planes[3] = scanner->buffer.data + 3 * N;
        sanei_ir_init ();
        sanei_pieusb_post (scanner, planes, scanner->buffer.colors);
    }

    /* Save preview data. Preview data only used once to set gain and offset. */
    if (scanner->val[OPT_PREVIEW].b) {
        sanei_pieusb_analyze_preview(scanner);
    } else {
        scanner->preview_done = SANE_FALSE;
    }

    /* Modify buffer in case the buffer has infrared, but no infrared should be returned */
    if (scanner->buffer.colors == PLANES && (strcmp(mode,SANE_VALUE_SCAN_MODE_COLOR) == 0 && scanner->val[OPT_CLEAN_IMAGE].b)) {
        DBG(DBG_info_sane, "sane_start(): modifying buffer to ignore I\n");
        /* Base buffer parameters */
        scanner->buffer.colors = 3;
        /* Derived quantities */
        scanner->buffer.image_size_bytes = scanner->buffer.colors * scanner->buffer.height * scanner->buffer.line_size_bytes;
        scanner->buffer.color_index_infrared = -1;
        scanner->buffer.bytes_unread = scanner->buffer.bytes_unread * 3 / 4;
        scanner->buffer.bytes_written = scanner->buffer.bytes_written * 3 / 4;
    }

    return SANE_STATUS_GOOD;

}

/**
 * Read image data from the scanner buffer.
 *
 * @param handle
 * @param buf
 * @param max_len
 * @param len
 * @return
 */
SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len, SANE_Int * len)
{

    struct Pieusb_Scanner *scanner = handle;
    SANE_Int return_size;

    DBG(DBG_info_sane, "sane_read(): requested %d bytes\n", max_len);

    /* No reading if not scanning */
    if (!scanner->scanning) {
        *len = 0;
        return SANE_STATUS_IO_ERROR; /* SANE standard does not allow a SANE_STATUS_INVAL return */
    }

    /* Handle cancel request */
    if (scanner->cancel_request) {
        return sanei_pieusb_on_cancel(scanner);
    }
#if 0
    /* Return image data, just read from scanner buffer */
    DBG(DBG_info_sane, "sane_read():\n");
    DBG(DBG_info_sane, "  image size %d\n", scanner->buffer.image_size_bytes);
    DBG(DBG_info_sane, "  unread     %d\n", scanner->buffer.bytes_unread);
    DBG(DBG_info_sane, "  read       %d\n", scanner->buffer.bytes_read);
    DBG(DBG_info_sane, "  max_len    %d\n", max_len);
#endif
    if (scanner->buffer.bytes_read > scanner->buffer.image_size_bytes) {
        /* Test if not reading past buffer boundaries */
        DBG(DBG_error, "sane_read(): reading past buffer boundaries (contains %d, read %d)\n", scanner->buffer.image_size_bytes, scanner->buffer.bytes_read);
        *len = 0;
        sanei_pieusb_on_cancel(scanner);
        return SANE_STATUS_EOF;
    } else if (scanner->buffer.bytes_read == scanner->buffer.image_size_bytes) {
        /* Return EOF since all data of this frame has already been read. */
        *len = 0;
        scanner->scanning = SANE_FALSE;
        return SANE_STATUS_EOF;
    } else if (scanner->buffer.bytes_unread >= max_len) {
        /* Already enough data to return, do not read */
        DBG(DBG_info_sane, "sane_read(): buffer suffices (contains %d, requested %d)\n", scanner->buffer.bytes_unread, max_len);
        return_size = max_len;
    } else if (scanner->buffer.bytes_read + scanner->buffer.bytes_unread == scanner->buffer.image_size_bytes) {
        /* All the remaining data is in the buffer, do not read */
        DBG(DBG_info_sane, "sane_read(): buffer suffices (contains %d, requested %d, last batch though)\n", scanner->buffer.bytes_unread, max_len);
        return_size = scanner->buffer.bytes_unread;
    } else {
        /* Should not happen in this implementation - all data read by sane_start() */
        DBG(DBG_error, "sane_read(): shouldn't be here...\n");
        return SANE_STATUS_IO_ERROR;
    }

    /* Check */
    if (return_size == 0 && scanner->buffer.bytes_read < scanner->buffer.image_size_bytes) {
        DBG(DBG_error, "sane_read(): unable to service read request, %d bytes in frame, %d read\n", scanner->buffer.image_size_bytes, scanner->buffer.bytes_read);
    }

    /* Return the available data: Output return_size bytes from buffer */
    sanei_pieusb_buffer_get(&scanner->buffer, buf, max_len, len);
#if 0
    DBG(DBG_info_sane, "sane_read(): currently read %.2f lines of %d\n",
      (double)scanner->buffer.bytes_written/(scanner->buffer.line_size_bytes*scanner->buffer.colors),
      scanner->buffer.height);
    DBG(DBG_info_sane, "sane_read(): returning %d bytes (requested %d), returned %d of %d \n",
      *len, max_len,scanner->buffer.bytes_read, scanner->buffer.image_size_bytes);
#endif
    return SANE_STATUS_GOOD;

}

/**
 * Request cancellation of current scanning process.
 *
 * @param handle Scanner handle
 */
void
sane_cancel (SANE_Handle handle)
{
    struct Pieusb_Scanner *scanner = handle;

    DBG (DBG_info_sane, "sane_cancel\n");

    if (scanner->scanning) {
        scanner->cancel_request = 1;
    }
}

/**
 * Set the I/O mode of handle h. The I/O mode can be either blocking or
 * non-blocking, but for USB devices, only blocking mode is supported.
 *
 * @param handle Scanner handle
 * @param non_blocking
 * @return SANE_STATUS_UNSUPPORTED;
 */
SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
    /* Pieusb_Scanner *scanner = handle; */

    DBG (DBG_info_sane, "sane_set_io_mode: handle = %p, non_blocking = %s\n", handle, non_blocking == SANE_TRUE ? "true" : "false");

    if (non_blocking) {
	return SANE_STATUS_UNSUPPORTED;
    }

    return SANE_STATUS_GOOD;
}

/**
 * Obtain a file-descriptor for the scanner that is readable if image data is
 * available. The select file-descriptor is returned in *fd.
 * The function has not been implemented since USB-device only operate in
 * blocking mode.
 *
 * @param handle Scanner handle
 * @param fd File descriptor with imae data
 * @return SANE_STATUS_INVAL
 */
SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
    DBG(DBG_info_sane,"sane_get_select_fd(): not supported (only for non-blocking IO)\n");
    handle = handle;
    fd = fd;
    return SANE_STATUS_UNSUPPORTED;
}
