/* sane - Scanner Access Now Easy.

   Copyright (C) 1997-2005, 2013 Franck Schnefra, Michel Roelofs,
   Emmanuel Blot, Mikko Tyolajarvi, David Mosberger-Tang, Wolfgang Goeller,
   Simon Munton, Petter Reinholdtsen, Gary Plewa, Sebastien Sable,
   Mikael Magnusson, Max Ushakov, Andrew Goodbody, Oliver Schwartz
   and Kevin Charter

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

   This file is a component of the implementation of a backend for many
   of the AGFA SnapScan and Acer Vuego/Prisa flatbed scanners. */


/* $Id$
   SANE SnapScan backend */

#include "../include/sane/config.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/sanei_scsi.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/sanei_thread.h"

#ifndef PATH_MAX
#define PATH_MAX        1024
#endif

#define MINOR_VERSION        4
#define BUILD               53
#define BACKEND_NAME snapscan

#ifdef __GNUC__
#define UNUSEDARG __attribute__ ((unused))
#else
#define UNUSEDARG
#endif

#include "../include/sane/sanei_backend.h"
#include "../include/sane/saneopts.h"

#include "snapscan.h"

#define MIN(x,y) ((x)<(y) ? (x) : (y))
#define MAX(x,y) ((x)>(y) ? (x) : (y))
#define LIMIT(x,min,max) MIN(MAX(x, min), max)

#ifdef INOPERATIVE
#define P_200_TO_255(per) SANE_UNFIX(255.0*((per + 100)/200.0))
#endif

#include "../include/sane/sanei_config.h"

/* debug levels */
#define DL_INFO         10
#define DL_MINOR_INFO   15
#define DL_MAJOR_ERROR  1
#define DL_MINOR_ERROR  2
#define DL_DATA_TRACE   50
#define DL_OPTION_TRACE 70
#define DL_CALL_TRACE   30
#define DL_VERBOSE      20

#define CHECK_STATUS(s,caller,cmd) \
if ((s) != SANE_STATUS_GOOD) { DBG(DL_MAJOR_ERROR, "%s: %s command failed: %s\n", caller, (cmd), sane_strstatus(s)); return s; }

/*----- internal scanner operations -----*/

/* hardware configuration byte masks */

#define HCFG_ADC         0x80         /* AD converter 1 ==> 10bit, 0 ==> 8bit */
#define HCFG_ADF         0x40         /* automatic document feeder */
#define HCFG_TPO         0x20         /* transparency option */
#define HCFG_RB          0x10         /* ring buffer */
#define HCFG_HT16        0x08         /* 16x16 halftone matrices */
#define HCFG_HT8         0x04         /* 8x8 halftone matrices */
#define HCFG_SRA         0x02         /* scanline row average (high-speed colour) */
#define HCFG_CAL_ALLOWED 0x01         /* 1 ==> calibration allowed */

#define HCFG_HT   0x0C                /* support halftone matrices at all */

#define MM_PER_IN 25.4                /* # millimetres per inch */
#define IN_PER_MM 0.03937        /* # inches per millimetre  */

#define GAMMA_8BIT	0
#define GAMMA_16BIT	1
#define GAMMA_12_16BIT	2

#ifndef SANE_I18N
#define SANE_I18N(text) text
#endif

/* authorization stuff */
static SANE_Auth_Callback auth = NULL;
#if UNUSED
static SANE_Char username[SANE_MAX_USERNAME_LEN];
static SANE_Char password[SANE_MAX_PASSWORD_LEN];
#endif

/* function prototypes */

static void gamma_n (double gamma, int brightness, int contrast,
                     u_char *buf, int length, int gamma_mode);
static void gamma_to_sane (int length, u_char *in, SANE_Int *out);

static size_t max_string_size(SANE_String_Const strings[]);

/* inline functions */
static inline SnapScan_Mode actual_mode (SnapScan_Scanner *pss)
{
    if (pss->preview == SANE_TRUE)
        return pss->preview_mode;
    return pss->mode;
}

static inline int is_colour_mode (SnapScan_Mode m)
{
    return (m == MD_COLOUR) || (m == MD_BILEVELCOLOUR);
}

static inline int calibration_line_length(SnapScan_Scanner *pss)
{
    int pos_factor;
    int pixel_length;

    switch (pss->pdev->model)
    {
    case STYLUS_CX1500:
    case PRISA5000E:
    case PRISA5000:
    case PRISA5150:
    case PERFECTION1270:
    case PERFECTION1670:
    case PERFECTION2480:
    case PERFECTION3490:
        pos_factor = pss->actual_res / 2;
        pixel_length = pos_factor * 8.5;
        break;
    case SCANWIT2720S:
        pixel_length = 2550;
        break;
    default:
        pos_factor = pss->actual_res;
        pixel_length = pos_factor * 8.5;
        break;
    }

    if(is_colour_mode(actual_mode(pss))) {
        return 3 * pixel_length;
    } else {
        return pixel_length;
    }
}

/*----- global data structures and access utilities -----*/

/* available device list */

static SnapScan_Device *first_device = NULL;        /* device list head */
static SANE_Int n_devices = 0;                           /* the device count */
static SANE_Char *default_firmware_filename;
static SANE_Bool cancelRead;

/* list returned from sane_get_devices() */
static const SANE_Device **get_devices_list = NULL;

/* external routines */
#include "snapscan-scsi.c"
#include "snapscan-sources.c"
#include "snapscan-usb.c"
#include "snapscan-options.c"

/* Initialize gamma tables */
static SANE_Status alloc_gamma_tables(SnapScan_Scanner * ps)
{
    static const char me[] = "alloc_gamma_tables";

    ps->gamma_length = 1 << ps->bpp;
    DBG (DL_MINOR_INFO, "%s: using 4*%d bytes for gamma table\n",
         me,
         ps->gamma_length);

    ps->gamma_tables =
        (SANE_Int *) malloc(4 * ps->gamma_length * sizeof(SANE_Int));

    if (!ps->gamma_tables)
    {
        return SANE_STATUS_NO_MEM;
    }

    ps->gamma_table_gs = &ps->gamma_tables[0 * ps->gamma_length];
    ps->gamma_table_r = &ps->gamma_tables[1 * ps->gamma_length];
    ps->gamma_table_g = &ps->gamma_tables[2 * ps->gamma_length];
    ps->gamma_table_b = &ps->gamma_tables[3 * ps->gamma_length];

    return SANE_STATUS_GOOD;
}

static SANE_Status init_gamma(SnapScan_Scanner * ps)
{
    u_char *gamma;

    gamma = (u_char*) malloc(ps->gamma_length * sizeof(u_char) * 2);

    if (!gamma)
    {
        return SANE_STATUS_NO_MEM;
    }

    /* Default tables */
    gamma_n (SANE_UNFIX(ps->gamma_gs), ps->bright, ps->contrast, gamma, ps->bpp, 1);
    gamma_to_sane (ps->gamma_length, gamma, ps->gamma_table_gs);

    gamma_n (SANE_UNFIX(ps->gamma_r), ps->bright, ps->contrast, gamma, ps->bpp, 1);
    gamma_to_sane (ps->gamma_length, gamma, ps->gamma_table_r);

    gamma_n (SANE_UNFIX(ps->gamma_g), ps->bright, ps->contrast, gamma, ps->bpp, 1);
    gamma_to_sane (ps->gamma_length, gamma, ps->gamma_table_g);

    gamma_n (SANE_UNFIX(ps->gamma_b), ps->bright, ps->contrast, gamma, ps->bpp, 1);
    gamma_to_sane (ps->gamma_length, gamma, ps->gamma_table_b);

    free (gamma);
    return SANE_STATUS_GOOD;
}

/* Max string size */

static size_t max_string_size (SANE_String_Const strings[])
{
    size_t size;
    size_t max_size = 0;
    int i;

    for (i = 0;  strings[i];  ++i)
    {
        size = strlen (strings[i]) + 1;
        if (size > max_size)
            max_size = size;
    }
    return max_size;
}

/* gamma table computation */
static void gamma_n (double gamma, int brightness, int contrast,
                      u_char *buf, int bpp, int gamma_mode)
{
    int i;
    double i_gamma = 1.0/gamma;
    int length = 1 << bpp;
    int max = length - 1;
    double mid = max / 2.0;

    for (i = 0;  i < length;  i++)
    {
        int x;
        double val = (i - mid) * (1.0 + contrast / 100.0)
            + (1.0 + brightness / 100.0) * mid;
        val = LIMIT(val, 0, max);
        switch (gamma_mode)
        {
        case GAMMA_16BIT:
            x = LIMIT(65535*pow ((double) val/max, i_gamma) + 0.5, 0, 65535);

            buf[2*i] = (u_char) x;
            buf[2*i + 1] = (u_char) (x >> 8);
            break;
        case GAMMA_12_16BIT:
            buf[2*i] = (u_char) i;
            buf[2*i + 1] = (u_char) (i >> 8);
            break;
        case GAMMA_8BIT:
            buf[i] =
                (u_char) LIMIT(255*pow ((double) val/max, i_gamma) + 0.5, 0, 255);
            break;
        default:
            break;
        }
    }
}

static void gamma_from_sane (int length, SANE_Int *in, u_char *out, int gamma_mode)
{
    int i;
    for (i = 0; i < length; i++)
        if (gamma_mode != GAMMA_8BIT)
        {
            out[2*i] = (u_char) LIMIT(in[i], 0, 65535);
            out[2*i + 1] = (u_char) (LIMIT(in[i], 0, 65535) >> 8);
        }
        else
            out[i] = (u_char) LIMIT(in[i] / 256, 0, 255);
}

static void gamma_to_sane (int length, u_char *in, SANE_Int *out)
{
    int i;
    for (i = 0; i < length; i++)
        out[i] = in[2*i] + 256 * in[2*i + 1];
}

/* dispersed-dot dither matrices; this is discussed in Foley, Van Dam,
   Feiner and Hughes: Computer Graphics: principles and practice,
   2nd ed. (Addison-Wesley), pp 570-571.

   The function mfDn computes the nth dispersed-dot dither matrix Dn
   given D(n/2) and n; n is presumed to be a power of 2. D8 and D16
   are the matrices of interest to us, since the SnapScan supports
   only 8x8 and 16x16 dither matrices. */

static u_char D2[] ={0, 2, 3, 1};

static u_char D4[16], D8[64], D16[256];

static void mkDn (u_char *Dn, u_char *Dn_half, unsigned n)
{
    unsigned int x, y;
    for (y = 0; y < n; y++) {
        for (x = 0; x < n; x++) {
            /* Dn(x,y) = D2(2*x/n, 2*y/n) +4*Dn_half(x%(n/2), y%(n/2)) */
            Dn[y*n + x] = D2[((int)(2*y/n))*2 + (int)(2*x/n)]
                          + 4*Dn_half[(y%(n/2))*(n/2) + x%(n/2)];
        }
    }
}

static SANE_Bool device_already_in_list (SnapScan_Device *current,
                                         SANE_String_Const name)
{
    for (  ;  NULL != current;  current = current->pnext)
    {
        if (0 == strcmp (name, current->dev.name))
            return SANE_TRUE;
    }
    return SANE_FALSE;
}

static SANE_Char* get_driver_name(SnapScan_Model model_num) {
    SANE_Int i;
    for (i=0; i<known_drivers; i++) {
        if (drivers[i].id == model_num) break;
    }
    if (i == known_drivers) {
        DBG(0, "Implementation error: Driver name not found\n");
        return ("Unknown");
    }
    return (drivers[i].driver_name);
}

static SANE_Status snapscani_check_device(
    int fd,
    SnapScan_Bus bus_type,
    char* vendor,
    char* model,
    SnapScan_Model* model_num
) {
    static const char me[] = "snapscani_check_device";
    SANE_Status status = SANE_STATUS_GOOD;
    int supported_vendor = 0;
    int i;

    DBG (DL_CALL_TRACE, "%s()\n", me);

    /* check that the device is legitimate */
    if ((status = mini_inquiry (bus_type, fd, vendor, model)) != SANE_STATUS_GOOD)
    {
        DBG (DL_MAJOR_ERROR,
             "%s: mini_inquiry failed with %s.\n",
             me,
             sane_strstatus (status));
        return status;
    }

    DBG (DL_VERBOSE,
         "%s: Is vendor \"%s\" model \"%s\" a supported scanner?\n",
         me,
         vendor,
         model);

    /* check if this is one of our supported vendors */
    for (i = 0;  i < known_vendors;  i++)
    {
        if (0 == strcasecmp (vendor, vendors[i]))
        {
            supported_vendor = 1;
            break;
        }
    }
    if (supported_vendor)
    {
        /* Known vendor.  Check if it is one of our supported models */
        *model_num = snapscani_get_model_id(model, fd, bus_type);
    }
    if (!supported_vendor  ||  UNKNOWN == model_num)
    {
        DBG (DL_MINOR_ERROR,
             "%s: \"%s %s\" is not one of %s\n",
             me,
             vendor,
             model,
             "AGFA SnapScan 300, 310, 600, 1212, 1236, e10, e20, e25, e26, "
             "e40, e42, e50, e52 or e60\n"
             "Acer 300, 310, 610, 610+, "
             "620, 620+, 640, 1240, 3300, 4300 or 5300\n"
             "Guillemot MaxiScan A4 Deluxe");
        status = SANE_STATUS_INVAL;
    } else {
        DBG(DL_VERBOSE, "%s: Autodetected driver: %s\n", me, get_driver_name(*model_num));
    }
    return status;
}

static SANE_Status snapscani_init_device_structure(
    SnapScan_Device **pd,
    const SnapScan_Bus bus_type,
    SANE_String_Const name,
    const char* vendor,
    const char* model,
    const SnapScan_Model model_num
) {
    static const char me[] = "snapscani_init_device_structure";
    SANE_Status status = SANE_STATUS_GOOD;;

    DBG (DL_CALL_TRACE, "%s()\n", me);

    (*pd) = (SnapScan_Device *) malloc (sizeof (SnapScan_Device));
    if (!(*pd))
    {
        DBG (DL_MAJOR_ERROR, "%s: out of memory allocating device.", me);
        return SANE_STATUS_NO_MEM;
    }
    (*pd)->dev.name = strdup (name);
    if (strcmp(vendor, "Color") == 0) {
        (*pd)->dev.vendor = strdup ("Acer");
    } else {
        (*pd)->dev.vendor = strdup (vendor);
    }
    (*pd)->dev.model = strdup (model);
    if (model_num == SCANWIT2720S)
    {
        (*pd)->dev.type = strdup (SNAPSCAN_FS_TYPE);
    }
    else
    {
        (*pd)->dev.type = strdup (SNAPSCAN_TYPE);
    }
    (*pd)->bus = bus_type;
    (*pd)->model = model_num;

    if (!(*pd)->dev.name  ||  !(*pd)->dev.vendor  ||  !(*pd)->dev.model  ||  !(*pd)->dev.type)
    {
        DBG (DL_MAJOR_ERROR,
             "%s: out of memory allocating device descriptor strings.\n",
             me);
        free (*pd);
        return SANE_STATUS_NO_MEM;
    }
    (*pd)->x_range.min = x_range_fb.min;
    (*pd)->x_range.quant = x_range_fb.quant;
    (*pd)->x_range.max = x_range_fb.max;
    (*pd)->y_range.min = y_range_fb.min;
    (*pd)->y_range.quant = y_range_fb.quant;
    (*pd)->y_range.max = y_range_fb.max;
    (*pd)->firmware_filename = NULL;

    (*pd)->pnext = first_device;
    first_device = (*pd);
    n_devices++;
    return status;
}

static SANE_Status add_scsi_device (SANE_String_Const full_name)
{
    int fd;
    static const char me[] = "add_scsi_device";
    SANE_Status status = SANE_STATUS_GOOD;
    SnapScan_Device *pd;
    SnapScan_Model model_num = UNKNOWN;
    SnapScan_Bus bus_type = SCSI;
    char vendor[8];
    char model[17];
    SANE_Char *name = NULL;

    DBG (DL_CALL_TRACE, "%s(%s)\n", me, full_name);

    sanei_config_get_string(full_name, &name);
    if (!name)
    {
    	return SANE_STATUS_INVAL;
    }
    /* Avoid adding the same device more then once */
    if (device_already_in_list (first_device, name)) {
        free(name);
        name = 0;
        return SANE_STATUS_GOOD;
    }

    vendor[0] = model[0] = '\0';

    DBG (DL_VERBOSE, "%s: Detected (kind of) a SCSI device\n", me);

    status = sanei_scsi_open (name, &fd, sense_handler, NULL);
    if (status != SANE_STATUS_GOOD)
    {
        DBG (DL_MAJOR_ERROR,
            "%s: error opening device %s: %s\n",
            me,
            name,
            sane_strstatus (status));
    } else {
        status = snapscani_check_device(fd, bus_type, vendor, model, &model_num);
        sanei_scsi_close(fd);
    }
    if (status == SANE_STATUS_GOOD) {
        status = snapscani_init_device_structure(
            &pd,
            bus_type,
            name,
            vendor,
            model,
            model_num
        );
    }
    free(name);
    name = 0;
    return status;
}

static SANE_Status add_usb_device (SANE_String_Const full_name) {
    static const char me[] = "add_usb_device";
    int fd;
    SnapScan_Device *pd;
    SnapScan_Model model_num = UNKNOWN;
    SANE_Word vendor_id, product_id;
    int supported_usb_vendor = 0;
    char vendor[8];
    char model[17];
    SANE_Status status = SANE_STATUS_GOOD;
    SnapScan_Bus bus_type = USB;
    int i;
    SANE_Char *name = NULL;

    DBG (DL_CALL_TRACE, "%s(%s)\n", me, full_name);
    sanei_config_get_string(full_name, &name);
    if (!name)
    {
        return SANE_STATUS_INVAL;
    }
    /* Avoid adding the same device more then once */
    if (device_already_in_list (first_device, name)) {
        free(name);
        name = 0;
        return SANE_STATUS_GOOD;
    }

    vendor[0] = model[0] = '\0';

    DBG (DL_VERBOSE, "%s: Detected (kind of) an USB device\n", me);
    bus_type = USB;
    status = snapscani_usb_shm_init();
    if (status != SANE_STATUS_GOOD)
    {
        return status;
    }
    status = snapscani_usb_open (name, &fd, sense_handler, NULL);
    if (status != SANE_STATUS_GOOD)
    {
        DBG (DL_MAJOR_ERROR,
            "%s: error opening device %s: %s\n",
            me,
            name,
            sane_strstatus (status));
    } else {
        if (sanei_usb_get_vendor_product(fd, &vendor_id, &product_id) ==
                SANE_STATUS_GOOD)
        {
            /* check for known USB vendors to avoid hanging scanners by
               inquiry-command.
            */
            DBG(DL_INFO, "%s: Checking if 0x%04x is a supported USB vendor ID\n",
                me, vendor_id);
            for (i = 0; i < known_usb_vendor_ids; i++) {
                if (vendor_id == usb_vendor_ids[i]) {
                    supported_usb_vendor = 1;
                }
            }
            if (!supported_usb_vendor) {
                DBG(DL_MINOR_ERROR,
                    "%s: USB vendor ID 0x%04x is currently NOT supported by the snapscan backend.\n",
                    me, vendor_id);
                status=SANE_STATUS_INVAL;
                snapscani_usb_close(fd);
            }
        }
    }
    if (status == SANE_STATUS_GOOD) {
        status = snapscani_check_device(fd, bus_type, vendor, model, &model_num);
        snapscani_usb_close(fd);
    }
    /* deinit shared memory, will be initialized again in open_scanner */
    snapscani_usb_shm_exit();
    if (status == SANE_STATUS_GOOD) {
        status = snapscani_init_device_structure(
            &pd,
            bus_type,
            name,
            vendor,
            model,
            model_num
        );
    }
    free(name);
    name = 0;
    return status;
}

/* find_device: find a device in the available list by name

   ARG: the device name

   RET: a pointer to the corresponding device record, or NULL if there
   is no such device */

static SnapScan_Device *find_device (SANE_String_Const name)
{
    static char me[] = "find_device";
    SnapScan_Device *psd;

    DBG (DL_CALL_TRACE, "%s\n", me);

    for (psd = first_device;  psd;  psd = psd->pnext)
    {
        if (strcmp (psd->dev.name, name) == 0)
            return psd;
    }
    return NULL;
}

/*----- functions in the scanner interface -----*/

SANE_Status sane_init (SANE_Int *version_code,
                       SANE_Auth_Callback authorize)
{
    static const char me[] = "sane_snapscan_init";
    char dev_name[PATH_MAX];
    size_t len;
    FILE *fp;
    SANE_Status status;

    DBG_INIT ();

    DBG (DL_CALL_TRACE, "%s\n", me);
    DBG (DL_VERBOSE, "%s: Snapscan backend version %d.%d.%d\n",
        me,
        SANE_CURRENT_MAJOR, MINOR_VERSION, BUILD);

    if (version_code != NULL)
    {
        *version_code =
            SANE_VERSION_CODE (SANE_CURRENT_MAJOR, MINOR_VERSION, BUILD);
    }

    auth = authorize;
    /* Initialize data structures */
    default_firmware_filename = NULL;
    first_device = NULL;
    n_devices = 0;

    sanei_usb_init();
    sanei_thread_init();
    /* build a device structure */
    fp = sanei_config_open (SNAPSCAN_CONFIG_FILE);
    if (!fp)
    {
        /* default to DEFAULT_DEVICE instead of insisting on config file */
        DBG (DL_INFO,
             "%s: configuration file not found, defaulting to %s.\n",
             me,
             DEFAULT_DEVICE);
        status = add_scsi_device (DEFAULT_DEVICE);
        if (status != SANE_STATUS_GOOD)
        {
            DBG (DL_MINOR_ERROR,
                 "%s: failed to add device \"%s\"\n",
                 me,
                 dev_name);
        }
    }
    else
    {
        while (sanei_config_read (dev_name, sizeof (dev_name), fp))
        {
            len = strlen (dev_name);
            if (!len)
                continue;                /* ignore empty lines */
            if (dev_name[0] == '#')        /* ignore line comments */
                continue;
            if (strncasecmp(dev_name, FIRMWARE_KW, strlen(FIRMWARE_KW)) == 0) {
                if (!default_firmware_filename) {
                    sanei_config_get_string(dev_name + strlen(FIRMWARE_KW), &default_firmware_filename);
                    if (default_firmware_filename == NULL) {
                        DBG (0, "%s: Illegal firmware entry %s.\n", me, dev_name);
                    }
                }
            }
            else if (strncasecmp(dev_name, OPTIONS_KW, strlen(OPTIONS_KW)) == 0)
                continue;                   /* ignore options lines */

            else if (strncmp(dev_name, "usb", 3) == 0) {
                sanei_usb_attach_matching_devices (dev_name, add_usb_device);
            }
            else if (strncmp(dev_name, "scsi", 4) == 0) {
                sanei_config_attach_matching_devices (dev_name, add_scsi_device);
            }
            else if (strstr (dev_name, "usb")) {
                add_usb_device(dev_name);
            }
            else {
                add_scsi_device(dev_name);
            }
        }
        fclose (fp);
    }

    /* compute the dither matrices */

    mkDn (D4, D2, 4);
    mkDn (D8, D4, 8);
    mkDn (D16, D8, 16);
    /* scale the D8 matrix from 0..63 to 0..255 */
    {
        u_char i;
        for (i = 0;  i < 64;  i++)
            D8[i] = (u_char) (4 * D8[i] + 2);
    }
    return SANE_STATUS_GOOD;
}

static void free_device_list(SnapScan_Device *psd) {
    if (psd->pnext != NULL) {
        free_device_list(psd->pnext);
    }
    free(psd);
}

void sane_exit (void)
{
    DBG (DL_CALL_TRACE, "sane_snapscan_exit\n");

    if (get_devices_list)
        free (get_devices_list);
    get_devices_list = NULL;

    /* just for safety, reset things to known values */
    auth = NULL;

    if (first_device) {
        free_device_list(first_device);
        first_device = NULL;
    }
    n_devices = 0;
}


SANE_Status sane_get_devices (const SANE_Device ***device_list,
                              SANE_Bool local_only)
{
    static const char *me = "sane_snapscan_get_devices";
    DBG (DL_CALL_TRACE,
         "%s (%p, %ld)\n",
         me,
         (const void *) device_list,
         (long) local_only);

    /* Waste the last list returned from this function */
    if (NULL != get_devices_list)
        free (get_devices_list);

    *device_list =
        (const SANE_Device **) malloc ((n_devices + 1) * sizeof (SANE_Device *));

    if (*device_list)
    {
        int i;
        SnapScan_Device *pdev;
        for (i = 0, pdev = first_device;  pdev;  i++, pdev = pdev->pnext)
            (*device_list)[i] = &(pdev->dev);
        (*device_list)[i] = 0x0000 /*NULL */;
    }
    else
    {
        DBG (DL_MAJOR_ERROR, "%s: out of memory\n", me);
        return SANE_STATUS_NO_MEM;
    }

    get_devices_list = *device_list;

    return SANE_STATUS_GOOD;
}

SANE_Status sane_open (SANE_String_Const name, SANE_Handle * h)
{
    static const char *me = "sane_snapscan_open";
    SnapScan_Device *psd;
    SANE_Status status;

    DBG (DL_CALL_TRACE, "%s (%s, %p)\n", me, name, (void *) h);

    /* possible authorization required */

    /* no device name: use first device */
    if ((strlen(name) == 0) && (first_device != NULL))
    {
        name = first_device->dev.name;
    }

    /* device exists? */
    psd = find_device (name);
    if (!psd)
    {
        DBG (DL_MINOR_ERROR,
             "%s: device \"%s\" not in current device list.\n",
             me,
             name);
        return SANE_STATUS_INVAL;
    }

    /* create and initialize the scanner structure */

    *h = (SnapScan_Scanner *) calloc (sizeof (SnapScan_Scanner), 1);
    if (!*h)
    {
        DBG (DL_MAJOR_ERROR,
             "%s: out of memory creating scanner structure.\n",
             me);
        return SANE_STATUS_NO_MEM;
    }

    {
        SnapScan_Scanner *pss = *(SnapScan_Scanner **) h;

        {
            pss->devname = strdup (name);
            if (!pss->devname)
            {
                free (*h);
                DBG (DL_MAJOR_ERROR,
                     "%s: out of memory copying device name.\n",
                     me);
                return SANE_STATUS_NO_MEM;
            }
            pss->pdev = psd;
            pss->opens = 0;
            pss->sense_str = NULL;
            pss->as_str = NULL;
            pss->phys_buf_sz = DEFAULT_SCANNER_BUF_SZ;
            if ((pss->pdev->model == PERFECTION2480) || (pss->pdev->model == PERFECTION3490))
                pss->phys_buf_sz *= 2;
            if (psd->bus == SCSI) {
                pss->phys_buf_sz = sanei_scsi_max_request_size;
            }
            DBG (DL_DATA_TRACE,
                "%s: Allocating %lu bytes as scanner buffer.\n",
                me, (u_long) pss->phys_buf_sz);
            pss->buf = (u_char *) malloc(pss->phys_buf_sz);
            if (!pss->buf) {
                DBG (DL_MAJOR_ERROR,
                "%s: out of memory creating scanner buffer.\n",
                me);
                return SANE_STATUS_NO_MEM;
            }

            DBG (DL_VERBOSE,
                 "%s: allocated scanner structure at %p\n",
                 me,
                 (void *) pss);
        }
        status = snapscani_usb_shm_init();
        if (status != SANE_STATUS_GOOD)
        {
            return status;
        }
        status = open_scanner (pss);
        if (status != SANE_STATUS_GOOD)
        {
            DBG (DL_MAJOR_ERROR,
                 "%s: open_scanner failed, status: %s\n",
                 me,
                 sane_strstatus (status));
            free (pss);
            return SANE_STATUS_ACCESS_DENIED;
        }

        DBG (DL_MINOR_INFO, "%s: waiting for scanner to warm up.\n", me);
        status = wait_scanner_ready (pss);
        if (status != SANE_STATUS_GOOD)
        {
            DBG (DL_MAJOR_ERROR,
                "%s: error waiting for scanner to warm up: %s\n",
                me,
                sane_strstatus(status));
            free (pss);
            return status;
        }
        DBG (DL_MINOR_INFO, "%s: performing scanner self test.\n", me);
        status = send_diagnostic (pss);
        if (status != SANE_STATUS_GOOD)
        {
            DBG (DL_MINOR_INFO, "%s: send_diagnostic reports %s\n",
                 me, sane_strstatus (status));
            free (pss);
            return status;
        }
        DBG (DL_MINOR_INFO, "%s: self test passed.\n", me);

        /* option initialization depends on getting the hardware configuration
           byte */
        status = inquiry (pss);
        if (status != SANE_STATUS_GOOD)
        {
            DBG (DL_MAJOR_ERROR,
                    "%s: error in inquiry command: %s\n",
                me,
                        sane_strstatus (status));
            free (pss);
            return status;
        }

		if (pss->pdev->bus == USB)
		{
			if (sanei_usb_get_vendor_product(pss->fd, &pss->usb_vendor, &pss->usb_product) != SANE_STATUS_GOOD)
			{
				pss->usb_vendor = 0;
				pss->usb_product = 0;
			}
	        /* Download Firmware for USB scanners */
	        if (pss->hwst & 0x02)
	        {
	            char vendor[8];
	            char model[17];

	            status = download_firmware(pss);
	            CHECK_STATUS (status, me, "download_firmware");
	            /* send inquiry command again, wait for scanner to initialize */
	            status = wait_scanner_ready(pss);
	            CHECK_STATUS (status, me, "wait_scanner_ready after firmware upload");
	            status =  mini_inquiry (pss->pdev->bus, pss->fd, vendor, model);
	            CHECK_STATUS (status, me, "mini_inquiry after firmware upload");
	            /* The model identifier may change after firmware upload */
	            DBG (DL_INFO,
	                "%s (after firmware upload): Checking if \"%s\" is a supported scanner\n",
	                me,
	                model);
	            /* Check if it is one of our supported models */
	            pss->pdev->model = snapscani_get_model_id(model, pss->fd, pss->pdev->bus);

	            if (pss->pdev->model == UNKNOWN) {
	                DBG (DL_MINOR_ERROR,
	                    "%s (after firmware upload): \"%s\" is not a supported scanner\n",
	                    me,
	                    model);
	            }
	            /* run "real" inquiry command once again for option initialization */
	            status = inquiry (pss);
	            CHECK_STATUS (status, me, "inquiry after firmware upload");
	        }
		}
        close_scanner(pss);

        status = alloc_gamma_tables (pss);
        if (status != SANE_STATUS_GOOD)
        {
            DBG (DL_MAJOR_ERROR,
                 "%s: error in alloc_gamma_tables: %s\n",
                 me,
                 sane_strstatus (status));
            free (pss);
            return status;
        }

        init_options (pss);
        status = init_gamma (pss);
        if (status != SANE_STATUS_GOOD)
        {
            DBG (DL_MAJOR_ERROR,
                 "%s: error in init_gamma: %s\n",
                 me,
                 sane_strstatus (status));
            free (pss);
            return status;
        }

        pss->state = ST_IDLE;
    }
    return SANE_STATUS_GOOD;
}

void sane_close (SANE_Handle h)
{
    SnapScan_Scanner *pss = (SnapScan_Scanner *) h;
    DBG (DL_CALL_TRACE, "sane_snapscan_close (%p)\n", (void *) h);
    switch (pss->state)
    {
    case ST_SCAN_INIT:
    case ST_SCANNING:
        release_unit (pss);
        break;
    default:
        break;
    }
    close_scanner (pss);
    snapscani_usb_shm_exit();
    free (pss->gamma_tables);
    free (pss->buf);
    free (pss);
}



SANE_Status sane_get_parameters (SANE_Handle h,
                                 SANE_Parameters *p)
{
    static const char *me = "sane_snapscan_get_parameters";
    SnapScan_Scanner *pss = (SnapScan_Scanner *) h;
    SANE_Status status = SANE_STATUS_GOOD;
    SnapScan_Mode mode = actual_mode(pss);

    DBG (DL_CALL_TRACE, "%s (%p, %p)\n", me, (void *) h, (void *) p);

    p->last_frame = SANE_TRUE;        /* we always do only one frame */

    if ((pss->state == ST_SCAN_INIT) || (pss->state == ST_SCANNING))
    {
        /* we are in the middle of a scan, so we can use the data
           that the scanner has reported */
        if (pss->psrc != NULL)
        {
            DBG(DL_DATA_TRACE, "%s: Using source chain data\n", me);
            /* use what the source chain says */
            p->pixels_per_line = pss->psrc->pixelsPerLine(pss->psrc);
            p->bytes_per_line = pss->psrc->bytesPerLine(pss->psrc);
            /* p->lines = pss->psrc->remaining(pss->psrc)/p->bytes_per_line; */
            p->lines = pss->lines;
        }
        else
        {
            DBG(DL_DATA_TRACE, "%s: Using current data\n", me);
            /* estimate based on current data */
            p->pixels_per_line = pss->pixels_per_line;
            p->bytes_per_line = pss->bytes_per_line;
            p->lines = pss->lines;
            if (mode == MD_BILEVELCOLOUR)
                p->bytes_per_line = p->pixels_per_line*3;
        }
    }
    else
    {
        /* no scan in progress. The scanner data may not be up to date.
           we have to calculate an estimate. */
        double width, height;
        int dpi;
        double dots_per_mm;

        DBG(DL_DATA_TRACE, "%s: Using estimated data\n", me);
        width = SANE_UNFIX (pss->brx - pss->tlx);
        height = SANE_UNFIX (pss->bry - pss->tly);
        dpi = pss->res;
        dots_per_mm = dpi / MM_PER_IN;
        p->pixels_per_line = width * dots_per_mm;
        p->lines = height * dots_per_mm;
        switch (mode)
        {
        case MD_COLOUR:
        case MD_BILEVELCOLOUR:
            p->bytes_per_line = 3 * p->pixels_per_line * ((pss->bpp_scan+7)/8);
            break;
        case MD_LINEART:
            p->bytes_per_line = (p->pixels_per_line + 7) / 8;
            break;
        default:
            /* greyscale */
            p->bytes_per_line = p->pixels_per_line * ((pss->bpp_scan+7)/8);
            break;
        }
    }
    p->format = (is_colour_mode(mode)) ? SANE_FRAME_RGB : SANE_FRAME_GRAY;
    if (mode == MD_LINEART)
        p->depth = 1;
    else if (pss->pdev->model == SCANWIT2720S)
        p->depth = 16;
    else if (pss->preview)
        p->depth = 8;
    else
        p->depth = pss->val[OPT_BIT_DEPTH].w;

    DBG (DL_DATA_TRACE, "%s: depth = %ld\n", me, (long) p->depth);
    DBG (DL_DATA_TRACE, "%s: lines = %ld\n", me, (long) p->lines);
    DBG (DL_DATA_TRACE,
         "%s: pixels per line = %ld\n",
         me,
         (long) p->pixels_per_line);
    DBG (DL_DATA_TRACE,
         "%s: bytes per line = %ld\n",
         me,
         (long) p->bytes_per_line);

    return status;
}

/* scan data reader routine for child process */

#define READER_WRITE_SIZE 4096

static void reader (SnapScan_Scanner *pss)
{
    static char me[] = "Child reader process";
    SANE_Status status;
    SANE_Byte *wbuf = NULL;


    DBG (DL_CALL_TRACE, "%s\n", me);

    wbuf = (SANE_Byte*) malloc(READER_WRITE_SIZE);
    if (wbuf == NULL)
    {
        DBG (DL_MAJOR_ERROR, "%s: failed to allocate write buffer.\n", me);
        return;
    }

    while ((pss->preadersrc->remaining(pss->preadersrc) > 0) && !cancelRead)
    {
        SANE_Int ndata = READER_WRITE_SIZE;
        status = pss->preadersrc->get(pss->preadersrc, wbuf, &ndata);
        if (status != SANE_STATUS_GOOD)
        {
            DBG (DL_MAJOR_ERROR,
                 "%s: %s on read.\n",
                 me,
                 sane_strstatus (status));
            return;
        }
        {
            SANE_Byte *buf = wbuf;
            DBG (DL_DATA_TRACE, "READ %d BYTES (%d)\n", ndata, cancelRead);
            while (ndata > 0)
            {
                int written = write (pss->rpipe[1], buf, ndata);
                DBG (DL_DATA_TRACE, "WROTE %d BYTES\n", written);
                if (written == -1)
                {
                    DBG (DL_MAJOR_ERROR,
                         "%s: error writing scan data on parent pipe.\n",
                         me);
                    perror ("pipe error: ");
                }
                else
                {
                    ndata -= written;
                    buf += written;
                }
            }
        }
    }
}

/** signal handler to kill the child process
 */
static RETSIGTYPE usb_reader_process_sigterm_handler( int signo )
{
    DBG( DL_INFO, "(SIG) reader_process: terminated by signal %d\n", signo );
    cancelRead = SANE_TRUE;
}

static RETSIGTYPE sigalarm_handler( int signo UNUSEDARG)
{
    DBG( DL_INFO, "ALARM!!!\n" );
}

/** executed as a child process
 * read the data from the driver and send them to the parent process
 */
static int reader_process( void *args )
{
    SANE_Status      status;
    struct SIGACTION act;
    sigset_t         ignore_set;
    SnapScan_Scanner *pss = (SnapScan_Scanner *) args;

    if( sanei_thread_is_forked()) {
        DBG( DL_MINOR_INFO, "reader_process started (forked)\n" );
        /* child process - close read side, make stdout the write side of the pipe */
        close( pss->rpipe[0] );
        pss->rpipe[0] = -1;
    } else {
        DBG( DL_MINOR_INFO, "reader_process started (as thread)\n" );
    }

    sigfillset ( &ignore_set );
    sigdelset  ( &ignore_set, SIGUSR1 );
    sigprocmask( SIG_SETMASK, &ignore_set, 0 );

    memset   ( &act, 0, sizeof (act));
    sigaction( SIGTERM, &act, 0 );

    cancelRead = SANE_FALSE;

    /* install the signal handler */
    sigemptyset(&(act.sa_mask));
    act.sa_flags = 0;

    act.sa_handler = usb_reader_process_sigterm_handler;
    sigaction( SIGUSR1, &act, 0 );

    status = create_base_source (pss, SCSI_SRC, &(pss->preadersrc));
    if (status == SANE_STATUS_GOOD)
    {
        reader (pss);
    }
    else
    {
        DBG (DL_MAJOR_ERROR,
                "Reader process: failed to create SCSISource.\n");
    }
    pss->preadersrc->done(pss->preadersrc);
    free(pss->preadersrc);
    pss->preadersrc = 0;
    close( pss->rpipe[1] );
    pss->rpipe[1] = -1;
    DBG( DL_MINOR_INFO, "reader_process: finished reading data\n" );
    return SANE_STATUS_GOOD;
}


static SANE_Status start_reader (SnapScan_Scanner *pss)
{
    SANE_Status status = SANE_STATUS_GOOD;
    static char me[] = "start_reader";

    DBG (DL_CALL_TRACE, "%s\n", me);

    pss->nonblocking = SANE_FALSE;
    pss->rpipe[0] = pss->rpipe[1] = -1;
    pss->child = -1;

    if (pipe (pss->rpipe) != -1)
    {
        pss->orig_rpipe_flags = fcntl (pss->rpipe[0], F_GETFL, 0);
        pss->child =  sanei_thread_begin(reader_process, (void *) pss);

        cancelRead = SANE_FALSE;

        if (pss->child == -1)
        {
            /* we'll have to read in blocking mode */
            DBG (DL_MAJOR_ERROR,
                 "%s: Error while calling sanei_thread_begin; must read in blocking mode.\n",
                 me);
            close (pss->rpipe[0]);
            close (pss->rpipe[1]);
            status = SANE_STATUS_UNSUPPORTED;
        }
        if (sanei_thread_is_forked())
        {
            /* parent; close write side */
            close (pss->rpipe[1]);
            pss->rpipe[1] = -1;
        }
        pss->nonblocking = SANE_TRUE;
    }
    return status;
}

static SANE_Status send_gamma_table (SnapScan_Scanner *pss, u_char dtc, u_char dtcq)
{
    static char me[] = "send_gamma_table";
    SANE_Status status = SANE_STATUS_GOOD;
    status = send (pss, dtc, dtcq);
    CHECK_STATUS (status, me, "send");
    switch (pss->pdev->model)
    {
        case PERFECTION1270:
        case PERFECTION1670:
        case PERFECTION2480:
        case PERFECTION3490:
            /* Some epson scanners need the gamma table twice */
            status = send (pss, dtc, dtcq);
            CHECK_STATUS (status, me, "2nd send");
            break;
        case PRISA5150:
            /* 5150 needs the gamma table twice, with dtc = 0x04 for the second one */
            status = send (pss, DTC_GAMMA2, dtcq);
            CHECK_STATUS (status, me, "2nd send");
            break;
        default:
            break;
    }
    return status;
}

static SANE_Status download_gamma_tables (SnapScan_Scanner *pss)
{
    static char me[] = "download_gamma_tables";
    SANE_Status status = SANE_STATUS_GOOD;
    double gamma_gs = SANE_UNFIX (pss->gamma_gs);
    double gamma_r = SANE_UNFIX (pss->gamma_r);
    double gamma_g = SANE_UNFIX (pss->gamma_g);
    double gamma_b = SANE_UNFIX (pss->gamma_b);
    SnapScan_Mode mode = actual_mode (pss);
    int dtcq_gamma_gray;
    int dtcq_gamma_red;
    int dtcq_gamma_green;
    int dtcq_gamma_blue;
    int gamma_mode = GAMMA_8BIT;

    DBG (DL_CALL_TRACE, "%s\n", me);
    switch (mode)
    {
    case MD_COLOUR:
        break;
    case MD_BILEVELCOLOUR:
        if (!pss->halftone)
        {
            gamma_r =
            gamma_g =
            gamma_b = 1.0;
        }
        break;
    case MD_LINEART:
        if (!pss->halftone)
            gamma_gs = 1.0;
        break;
    default:
        /* no further action for greyscale */
        break;
    }

    switch (pss->bpp)
    {
    case 10:
        DBG (DL_DATA_TRACE, "%s: Sending 8bit gamma table for %d bpp\n", me, pss->bpp);
        dtcq_gamma_gray = DTCQ_GAMMA_GRAY10;
        dtcq_gamma_red = DTCQ_GAMMA_RED10;
        dtcq_gamma_green = DTCQ_GAMMA_GREEN10;
        dtcq_gamma_blue = DTCQ_GAMMA_BLUE10;
        break;
    case 12:
        if (pss->pdev->model == SCANWIT2720S)
        {
            DBG (DL_DATA_TRACE, "%s: Sending 16bit gamma table for %d bpp\n", me, pss->bpp);
            dtcq_gamma_gray = DTCQ_GAMMA_GRAY12_16BIT;
            dtcq_gamma_red = DTCQ_GAMMA_RED12_16BIT;
            dtcq_gamma_green = DTCQ_GAMMA_GREEN12_16BIT;
            dtcq_gamma_blue = DTCQ_GAMMA_BLUE12_16BIT;
            gamma_mode = GAMMA_12_16BIT;
        }
        else
        {
            DBG (DL_DATA_TRACE, "%s: Sending 8bit gamma table for %d bpp\n", me, pss->bpp);
            dtcq_gamma_gray = DTCQ_GAMMA_GRAY12;
            dtcq_gamma_red = DTCQ_GAMMA_RED12;
            dtcq_gamma_green = DTCQ_GAMMA_GREEN12;
            dtcq_gamma_blue = DTCQ_GAMMA_BLUE12;
        }
        break;
    case 14:
        if (pss->bpp_scan == 16)
        {
            DBG (DL_DATA_TRACE, "%s: Sending 16bit gamma table for %d bpp\n", me, pss->bpp);
            dtcq_gamma_gray = DTCQ_GAMMA_GRAY14_16BIT;
            dtcq_gamma_red = DTCQ_GAMMA_RED14_16BIT;
            dtcq_gamma_green = DTCQ_GAMMA_GREEN14_16BIT;
            dtcq_gamma_blue = DTCQ_GAMMA_BLUE14_16BIT;
            gamma_mode = GAMMA_16BIT;
        }
        else
        {
            DBG (DL_DATA_TRACE, "%s: Sending 8bit gamma table for %d bpp\n", me, pss->bpp);
            dtcq_gamma_gray = DTCQ_GAMMA_GRAY14;
            dtcq_gamma_red = DTCQ_GAMMA_RED14;
            dtcq_gamma_green = DTCQ_GAMMA_GREEN14;
            dtcq_gamma_blue = DTCQ_GAMMA_BLUE14;
        }
        break;
    default:
        DBG (DL_DATA_TRACE, "%s: Sending 8bit gamma table for %d bpp\n", me, pss->bpp);
        dtcq_gamma_gray = DTCQ_GAMMA_GRAY8;
        dtcq_gamma_red = DTCQ_GAMMA_RED8;
        dtcq_gamma_green = DTCQ_GAMMA_GREEN8;
        dtcq_gamma_blue = DTCQ_GAMMA_BLUE8;
        break;
    }

    if (is_colour_mode(mode))
    {
        if (pss->val[OPT_CUSTOM_GAMMA].b)
        {
            if (pss->val[OPT_GAMMA_BIND].b)
            {
                /* Use greyscale gamma for all rgb channels */
                gamma_from_sane (pss->gamma_length, pss->gamma_table_gs,
                                 pss->buf + SEND_LENGTH, gamma_mode);
                status = send_gamma_table(pss, DTC_GAMMA, dtcq_gamma_red);
                CHECK_STATUS (status, me, "send");

                gamma_from_sane (pss->gamma_length, pss->gamma_table_gs,
                                 pss->buf + SEND_LENGTH, gamma_mode);
                status = send_gamma_table(pss, DTC_GAMMA, dtcq_gamma_green);
                CHECK_STATUS (status, me, "send");

                gamma_from_sane (pss->gamma_length, pss->gamma_table_gs,
                                 pss->buf + SEND_LENGTH, gamma_mode);
                status = send_gamma_table(pss, DTC_GAMMA, dtcq_gamma_blue);
                CHECK_STATUS (status, me, "send");
            }
            else
            {
                gamma_from_sane (pss->gamma_length, pss->gamma_table_r,
                                 pss->buf + SEND_LENGTH, gamma_mode);
                status = send_gamma_table(pss, DTC_GAMMA, dtcq_gamma_red);
                CHECK_STATUS (status, me, "send");

                gamma_from_sane (pss->gamma_length, pss->gamma_table_g,
                                 pss->buf + SEND_LENGTH, gamma_mode);
                status = send_gamma_table(pss, DTC_GAMMA, dtcq_gamma_green);
                CHECK_STATUS (status, me, "send");

                gamma_from_sane (pss->gamma_length, pss->gamma_table_b,
                                 pss->buf + SEND_LENGTH, gamma_mode);
                status = send_gamma_table(pss, DTC_GAMMA, dtcq_gamma_blue);
                CHECK_STATUS (status, me, "send");
            }
        }
        else
        {
            if (pss->val[OPT_GAMMA_BIND].b)
            {
                /* Use greyscale gamma for all rgb channels */
                gamma_n (gamma_gs, pss->bright, pss->contrast,
                         pss->buf + SEND_LENGTH, pss->bpp, gamma_mode);
                status = send_gamma_table(pss, DTC_GAMMA, dtcq_gamma_red);
                CHECK_STATUS (status, me, "send");

                gamma_n (gamma_gs, pss->bright, pss->contrast,
                         pss->buf + SEND_LENGTH, pss->bpp, gamma_mode);
                status = send_gamma_table(pss, DTC_GAMMA, dtcq_gamma_green);
                CHECK_STATUS (status, me, "send");

                gamma_n (gamma_gs, pss->bright, pss->contrast,
                         pss->buf + SEND_LENGTH, pss->bpp, gamma_mode);
                status = send_gamma_table(pss, DTC_GAMMA, dtcq_gamma_blue);
                CHECK_STATUS (status, me, "send");
            }
            else
            {
                gamma_n (gamma_r, pss->bright, pss->contrast,
                         pss->buf + SEND_LENGTH, pss->bpp, gamma_mode);
                status = send_gamma_table(pss, DTC_GAMMA, dtcq_gamma_red);
                CHECK_STATUS (status, me, "send");

                gamma_n (gamma_g, pss->bright, pss->contrast,
                         pss->buf + SEND_LENGTH, pss->bpp, gamma_mode);
                status = send_gamma_table(pss, DTC_GAMMA, dtcq_gamma_green);
                CHECK_STATUS (status, me, "send");

                gamma_n (gamma_b, pss->bright, pss->contrast,
                         pss->buf + SEND_LENGTH, pss->bpp, gamma_mode);
                status = send_gamma_table(pss, DTC_GAMMA, dtcq_gamma_blue);
                CHECK_STATUS (status, me, "send");
            }
        }
    }
    else
    {
        if(pss->val[OPT_CUSTOM_GAMMA].b)
        {
            gamma_from_sane (pss->gamma_length, pss->gamma_table_gs,
                             pss->buf + SEND_LENGTH, gamma_mode);
            status = send_gamma_table(pss, DTC_GAMMA, dtcq_gamma_gray);
            CHECK_STATUS (status, me, "send");
        }
        else
        {
            gamma_n (gamma_gs, pss->bright, pss->contrast,
                     pss->buf + SEND_LENGTH, pss->bpp, gamma_mode);
            status = send_gamma_table(pss, DTC_GAMMA, dtcq_gamma_gray);
            CHECK_STATUS (status, me, "send");
        }
    }
    return status;
}

static SANE_Status download_halftone_matrices (SnapScan_Scanner *pss)
{
    static char me[] = "download_halftone_matrices";
    SANE_Status status = SANE_STATUS_GOOD;
    if ((pss->halftone) &&
        ((actual_mode(pss) == MD_LINEART) || (actual_mode(pss) == MD_BILEVELCOLOUR)))
    {
        u_char *matrix;
        size_t matrix_sz;
        u_char dtcq;

        if (pss->dither_matrix == dm_dd8x8)
        {
            matrix = D8;
            matrix_sz = sizeof (D8);
        }
        else
        {
            matrix = D16;
            matrix_sz = sizeof (D16);
        }

        memcpy (pss->buf + SEND_LENGTH, matrix, matrix_sz);

        if (is_colour_mode(actual_mode(pss)))
        {
            if (matrix_sz == sizeof (D8))
                dtcq = DTCQ_HALFTONE_COLOR8;
            else
                dtcq = DTCQ_HALFTONE_COLOR16;

            /* need copies for green and blue bands */
            memcpy (pss->buf + SEND_LENGTH + matrix_sz,
                    matrix,
                    matrix_sz);
            memcpy (pss->buf + SEND_LENGTH + 2 * matrix_sz,
                    matrix,
                    matrix_sz);
        }
        else
        {
            if (matrix_sz == sizeof (D8))
                dtcq = DTCQ_HALFTONE_BW8;
            else
                dtcq = DTCQ_HALFTONE_BW16;
        }

        status = send (pss, DTC_HALFTONE, dtcq);
        CHECK_STATUS (status, me, "send");
    }
    return status;
}

static SANE_Status measure_transfer_rate (SnapScan_Scanner *pss)
{
    static char me[] = "measure_transfer_rate";
    SANE_Status status = SANE_STATUS_GOOD;

    if (pss->hconfig & HCFG_RB)
    {
        /* We have a ring buffer. We simulate one round of a read-store
           cycle on the size of buffer we will be using. For this read only,
           the buffer size must be rounded to a 128-byte boundary. */

        DBG (DL_VERBOSE, "%s: have ring buffer\n", me);
	if ((pss->pdev->model == PERFECTION2480) || (pss->pdev->model == PERFECTION3490))
	{
	    /* Epson 2480: read a multiple of bytes per line, limit to less than 0xfff0 */
	    if (pss->bytes_per_line > 0xfff0)
        	pss->expected_read_bytes = 0xfff0;
	    else
                pss->expected_read_bytes = (0xfff0 / pss->bytes_per_line) * pss->bytes_per_line;
	}
	else
            pss->expected_read_bytes =
                (pss->buf_sz%128)  ?  (pss->buf_sz/128 + 1)*128  :  pss->buf_sz;

        status = scsi_read (pss, READ_TRANSTIME);
        CHECK_STATUS (status, me, "scsi_read");
        pss->expected_read_bytes = 0;
        status = scsi_read (pss, READ_TRANSTIME);
        CHECK_STATUS (status, me, "scsi_read");
    }
    else
    {
        /* we don't have a ring buffer. The test requires transferring one
           scan line of data (rounded up to next 128 byte boundary). */

        DBG (DL_VERBOSE, "%s: we don't have a ring buffer.\n", me);
        pss->expected_read_bytes = pss->bytes_per_line;

        if (pss->expected_read_bytes%128)
        {
            pss->expected_read_bytes =
                (pss->expected_read_bytes/128 + 1)*128;
        }
        status = scsi_read (pss, READ_TRANSTIME);
        CHECK_STATUS (status, me, "scsi_read");
        DBG (DL_VERBOSE, "%s: read %ld bytes.\n", me, (long) pss->read_bytes);
    }

    pss->expected_read_bytes = 0;
    status = scsi_read (pss, READ_TRANSTIME);
    if (status != SANE_STATUS_GOOD)
    {
        DBG (DL_MAJOR_ERROR, "%s: test read failed.\n", me);
        return status;
    }

    DBG (DL_VERBOSE, "%s: successfully calibrated transfer rate.\n", me);
    return status;
}


SANE_Status sane_start (SANE_Handle h)
{
    static const char *me = "sane_snapscan_start";
    SANE_Status status;
    SnapScan_Scanner *pss = (SnapScan_Scanner *) h;

    DBG (DL_CALL_TRACE, "%s (%p)\n", me, (void *) h);

    /* possible authorization required */

    status = open_scanner (pss);
    CHECK_STATUS (status, me, "open_scanner");

    status = wait_scanner_ready (pss);
    CHECK_STATUS (status, me, "wait_scanner_ready");

    /* start scanning; reserve the unit first, because a release_unit is
       necessary to abort a scan in progress */

    pss->state = ST_SCAN_INIT;

    if ((pss->pdev->model == SCANWIT2720S) && (pss->focus_mode == MD_AUTO))
    {
        status = get_focus(pss);
        CHECK_STATUS (status, me, "get_focus");
    }

    reserve_unit(pss);

    if (pss->pdev->model == SCANWIT2720S)
    {
        status = set_frame(pss, 0);
        CHECK_STATUS (status, me, "set_frame");
        status = set_focus(pss, pss->focus);
        CHECK_STATUS (status, me, "set_focus");
    }

    /* set up the window and fetch the resulting scanner parameters */
    status = set_window(pss);
    CHECK_STATUS (status, me, "set_window");

    status = inquiry(pss);
    CHECK_STATUS (status, me, "inquiry");

    /* download the gamma and halftone tables */

    status = download_gamma_tables(pss);
    CHECK_STATUS (status, me, "download_gamma_tables");

    status = download_halftone_matrices(pss);
    CHECK_STATUS (status, me, "download_halftone_matrices");

    if (pss->val[OPT_QUALITY_CAL].b && (pss->usb_vendor == USB_VENDOR_EPSON))
    {
        status = calibrate(pss);
        if (status != SANE_STATUS_GOOD)
        {
            DBG (DL_MAJOR_ERROR, "%s: calibration failed.\n", me);
            release_unit (pss);
            return status;
        }
    }

    /* we must measure the data transfer rate between the host and the
       scanner, and the method varies depending on whether there is a
       ring buffer or not. */

    status = measure_transfer_rate(pss);
    CHECK_STATUS (status, me, "measure_transfer_rate");

    /* now perform an inquiry again to retrieve the scan speed */
    status = inquiry(pss);
    CHECK_STATUS (status, me, "inquiry");

    DBG (DL_DATA_TRACE,
         "%s: after measuring speed:\n\t%lu bytes per scan line\n"
         "\t%f milliseconds per scan line.\n\t==>%f bytes per millisecond\n",
         me,
         (u_long) pss->bytes_per_line,
         pss->ms_per_line,
         pss->bytes_per_line/pss->ms_per_line);


    if (pss->val[OPT_QUALITY_CAL].b && (pss->usb_vendor != USB_VENDOR_EPSON))
    {
        status = calibrate(pss);
        if (status != SANE_STATUS_GOOD)
        {
            DBG (DL_MAJOR_ERROR, "%s: calibration failed.\n", me);
            release_unit (pss);
            return status;
        }
    }

    status = scan(pss);
    if (status != SANE_STATUS_GOOD)
    {
        DBG (DL_MAJOR_ERROR, "%s: scan command failed: %s.\n", me, sane_strstatus(status));
        release_unit (pss);
        return status;
    }

    if (pss->pdev->model == SCANWIT2720S)
    {
        status = set_frame(pss, pss->frame_no);
        CHECK_STATUS (status, me, "set_frame");
    }

    if (pss->source == SRC_ADF)
    {
        /* Wait for scanner ready again (e.g. until paper is loaded from an ADF) */
        /* Maybe replace with get_data_buffer_status()? */
        status = wait_scanner_ready (pss);
        if (status != SANE_STATUS_GOOD)
        {
            DBG (DL_MAJOR_ERROR, "%s: scan command failed while waiting for scanner: %s.\n", me, sane_strstatus(status));
            release_unit (pss);
            return status;
        }
    }

    DBG (DL_MINOR_INFO, "%s: starting the reader process.\n", me);
    status = start_reader(pss);
    {
        BaseSourceType st = FD_SRC;
        if (status != SANE_STATUS_GOOD)
            st = SCSI_SRC;
        status = create_source_chain (pss, st, &(pss->psrc));
    }

    return status;
}


SANE_Status sane_read (SANE_Handle h,
                       SANE_Byte *buf,
                       SANE_Int maxlen,
                       SANE_Int *plen)
{
    static const char *me = "sane_snapscan_read";
    SnapScan_Scanner *pss = (SnapScan_Scanner *) h;
    SANE_Status status = SANE_STATUS_GOOD;

    DBG (DL_CALL_TRACE,
        "%s (%p, %p, %ld, %p)\n",
        me,
        (void *) h,
        (void *) buf,
        (long) maxlen,
        (void *) plen);

    *plen = 0;

    if (pss->state == ST_CANCEL_INIT) {
        pss->state = ST_IDLE;
        return SANE_STATUS_CANCELLED;
    }

    if (pss->psrc == NULL  ||  pss->psrc->remaining(pss->psrc) == 0)
    {
        if (pss->child != -1)
        {
            sanei_thread_waitpid (pss->child, 0);        /* ensure no zombies */
            pss->child = -1;
        }
        release_unit (pss);
        close_scanner (pss);
        if (pss->psrc != NULL)
        {
            pss->psrc->done(pss->psrc);
            free(pss->psrc);
            pss->psrc = NULL;
        }
        pss->state = ST_IDLE;
        return SANE_STATUS_EOF;
    }

    *plen = maxlen;
    status = pss->psrc->get(pss->psrc, buf, plen);

    switch (pss->state)
    {
    case ST_IDLE:
        DBG (DL_MAJOR_ERROR,
            "%s: weird error: scanner state should not be idle on call to "
            "sane_read.\n",
            me);
        break;
    case ST_SCAN_INIT:
        /* we've read some data */
        pss->state = ST_SCANNING;
        break;
    case ST_CANCEL_INIT:
        /* stop scanning */
        status = SANE_STATUS_CANCELLED;
        break;
    default:
        break;
    }

    return status;
}

void sane_cancel (SANE_Handle h)
{
    char *me = "sane_snapscan_cancel";
    SnapScan_Scanner *pss = (SnapScan_Scanner *) h;
    struct SIGACTION act;
    SANE_Pid         res;

    DBG (DL_CALL_TRACE, "%s\n", me);
    switch (pss->state)
    {
    case ST_IDLE:
        break;
    case ST_SCAN_INIT:
    case ST_SCANNING:
        /* signal a cancellation has occurred */
        pss->state = ST_CANCEL_INIT;
        /* signal the reader, if any */
        if (pss->child != -1)
        {
            DBG( DL_INFO, ">>>>>>>> killing reader_process <<<<<<<<\n" );

            sigemptyset(&(act.sa_mask));
            act.sa_flags = 0;

            act.sa_handler = sigalarm_handler;
            sigaction( SIGALRM, &act, 0 );

            if (sanei_thread_is_forked())
            {
                /* use SIGUSR1 to set cancelRead in child process */
                sanei_thread_sendsig( pss->child, SIGUSR1 );
            }
            else
            {
                cancelRead = SANE_TRUE;
            }

            /* give'em 10 seconds 'til done...*/
            alarm(10);
            res = sanei_thread_waitpid( pss->child, 0 );
            alarm(0);

            if( res != pss->child ) {
                DBG( DL_MINOR_ERROR,"sanei_thread_waitpid() failed !\n");

                /* do it the hard way...*/
#ifdef USE_PTHREAD
                sanei_thread_kill( pss->child );
#else
                sanei_thread_sendsig( pss->child, SIGKILL );
#endif
            }
            pss->child = -1;
            DBG( DL_INFO,"reader_process killed\n");
        }
        release_unit (pss);
        close_scanner (pss);
        break;
    case ST_CANCEL_INIT:
        DBG (DL_INFO, "%s: cancellation already initiated.\n", me);
        break;
    default:
        DBG (DL_MAJOR_ERROR,
             "%s: weird error: invalid scanner state (%ld).\n",
             me,
             (long) pss->state);
        break;
    }
}

SANE_Status sane_set_io_mode (SANE_Handle h, SANE_Bool m)
{
    static char me[] = "sane_snapscan_set_io_mode";
    SnapScan_Scanner *pss = (SnapScan_Scanner *) h;
    char *op;

    DBG (DL_CALL_TRACE, "%s\n", me);

    if (pss->state != ST_SCAN_INIT)
        return SANE_STATUS_INVAL;

    if (m)
    {
        if (pss->child == -1)
        {
            DBG (DL_MINOR_INFO,
                 "%s: no reader child; must use blocking mode.\n",
                 me);
            return SANE_STATUS_UNSUPPORTED;
        }
        op = "ON";
        fcntl (pss->rpipe[0], F_SETFL, O_NONBLOCK | pss->orig_rpipe_flags);
    }
    else
    {
        op = "OFF";
        fcntl (pss->rpipe[0], F_SETFL, pss->orig_rpipe_flags);
    }
    DBG (DL_MINOR_INFO, "%s: turning nonblocking mode %s.\n", me, op);
    pss->nonblocking = m;
    return SANE_STATUS_GOOD;
}

SANE_Status sane_get_select_fd (SANE_Handle h, SANE_Int * fd)
{
    static char me[] = "sane_snapscan_get_select_fd";
    SnapScan_Scanner *pss = (SnapScan_Scanner *) h;

    DBG (DL_CALL_TRACE, "%s\n", me);

    if (pss->state != ST_SCAN_INIT)
        return SANE_STATUS_INVAL;

    if (pss->child == -1)
    {
        DBG (DL_MINOR_INFO,
             "%s: no reader child; cannot provide select file descriptor.\n",
             me);
        return SANE_STATUS_UNSUPPORTED;
    }
    *fd = pss->rpipe[0];
    return SANE_STATUS_GOOD;
}

/*
 * $Log$
 * Revision 1.73  2008/11/26 21:21:29  kitno-guest
 * * backend/ *.[ch]: nearly every backend used V_MAJOR
 * instead of SANE_CURRENT_MAJOR in sane_init()
 * * backend/snapscan.c: remove EXPECTED_VERSION check
 * since new SANE standard is forward compatible
 *
 * Revision 1.72  2008-05-15 12:50:24  ellert-guest
 * Fix for bug #306751: sanei-thread with pthreads on 64 bit
 *
 * Revision 1.71  2008-01-29 17:48:42  kitno-guest
 * fix snapscan bug, add LiDE 600F
 *
 * Revision 1.70  2007-11-18 10:59:18  ellert-guest
 * Fix handling of valid "negative" PIDs
 *
 * Revision 1.69  2007-11-16 08:04:02  ellert-guest
 * Correct the test of the return value from sanei_thread_begin
 *
 * Revision 1.68  2006-09-03 10:00:11  oliver-guest
 * Bugfix for firmware download by Paul Smedley
 *
 * Revision 1.67  2006/01/10 19:32:16  oliver-guest
 * Added 12 bit gamma tables for Epson Stylus CX-1500
 *
 * Revision 1.66  2006/01/06 20:59:17  oliver-guest
 * Some fixes for the Epson Stylus CX 1500
 *
 * Revision 1.65  2006/01/01 23:02:55  oliver-guest
 * Added snapscan-data.c to Makefile.in
 *
 * Revision 1.64  2005/12/05 20:38:23  oliver-guest
 * Small bugfix for Benq 5150
 *
 * Revision 1.63  2005/12/04 15:03:00  oliver-guest
 * Some fixes for Benq 5150
 *
 * Revision 1.62  2005/12/02 19:15:42  oliver-guest
 * Change SnapScan version number to 1.4.50
 *
 * Revision 1.61  2005/11/15 20:11:19  oliver-guest
 * Enabled quality calibration for the Epson 3490
 *
 * Revision 1.60  2005/11/10 19:42:02  oliver-guest
 * Added deinterlacing for Epson 3490
 *
 * Revision 1.59  2005/11/02 22:12:54  oliver-guest
 * Correct cut'n'paste error
 *
 * Revision 1.58  2005/11/02 19:22:06  oliver-guest
 * Fixes for Benq 5000
 *
 * Revision 1.57  2005/10/31 21:08:47  oliver-guest
 * Distinguish between Benq 5000/5000E/5000U
 *
 * Revision 1.56  2005/10/24 19:46:40  oliver-guest
 * Preview and range fix for Epson 2480/2580
 *
 * Revision 1.55  2005/10/23 21:28:58  oliver-guest
 * Fix for buffer size in high res modes, fixes for delay code
 *
 * Revision 1.54  2005/10/13 22:43:30  oliver-guest
 * Fixes for 16 bit scan mode from Simon Munton
 *
 * Revision 1.53  2005/10/11 18:47:07  oliver-guest
 * Fixes for Epson 3490 and 16 bit scan mode
 *
 * Revision 1.52  2005/09/28 21:33:11  oliver-guest
 * Added 16 bit option for Epson scanners (untested)
 *
 * Revision 1.51  2005/08/15 18:56:55  oliver-guest
 * Added temporary debug code for 2480/2580 distinction
 *
 * Revision 1.50  2005/08/15 18:06:37  oliver-guest
 * Added support for Epson 3490/3590 (thanks to Matt Judge)
 *
 * Revision 1.49  2005/08/07 12:37:29  oliver-guest
 * Use first known device if no device is specified
 *
 * Revision 1.48  2004/12/09 23:21:48  oliver-guest
 * Added quality calibration for Epson 2480 (by Simon Munton)
 *
 * Revision 1.47  2004/12/01 22:49:14  oliver-guest
 * Fix for allocation of gamma tables by Simon Munton
 *
 * Revision 1.46  2004/12/01 22:12:03  oliver-guest
 * Added support for Epson 1270
 *
 * Revision 1.45  2004/10/03 17:34:36  hmg-guest
 * 64 bit platform fixes (bug #300799).
 *
 * Revision 1.44  2004/09/02 20:59:12  oliver-guest
 * Added support for Epson 2480
 *
 * Revision 1.43  2004/06/16 19:52:26  oliver-guest
 * Don't enforce even number of URB packages on 1212u_2. Fixes bug #300753.
 *
 * Revision 1.42  2004/06/15 12:17:37  hmg-guest
 * Only use __attribute__ if gcc is used for compilation. Some other compilers
 * don't know __attribute__ and therefore can't compile sane-backends without this
 * fix. See bug #300803.
 *
 * Revision 1.41  2004/05/26 22:37:01  oliver-guest
 * Use shared memory for urb counters in snapscan backend
 *
 * Revision 1.40  2004/04/09 11:59:02  oliver-guest
 * Fixes for pthread implementation
 *
 * Revision 1.39  2004/04/08 21:53:10  oliver-guest
 * Use sanei_thread in snapscan backend
 *
 * Revision 1.37  2003/11/27 23:11:32  oliver-guest
 * Send gamma table twice for Epson Perfection 1670
 *
 * Revision 1.36  2003/11/09 21:43:45  oliver-guest
 * Disabled quality calibration for Epson Perfection 1670
 *
 * Revision 1.35  2003/11/07 23:26:49  oliver-guest
 * Final bugfixes for bascic support of Epson 1670
 *
 * Revision 1.34  2003/10/21 20:43:25  oliver-guest
 * Bugfixes for SnapScan backend
 *
 * Revision 1.33  2003/10/07 18:29:20  oliver-guest
 * Initial support for Epson 1670, minor bugfix
 *
 * Revision 1.32  2003/09/24 18:05:39  oliver-guest
 * Bug #300198: Check second argument of sanei_config_get_string
 *
 * Revision 1.31  2003/09/12 16:10:33  hmg-guest
 * Moved union Option_Value from backend header files to sanei_backend.h. No need
 * to copy it over and over again. Changed header inclusion order in backend
 * files to include backend.h after sanei_backend.h. Based on a patch from stef
 * <stef-listes@wanadoo.fr>.
 *
 * Revision 1.30  2003/08/19 21:05:08  oliverschwartz
 * Scanner ID cleanup
 *
 * Revision 1.29  2003/04/30 20:49:40  oliverschwartz
 * SnapScan backend 1.4.26
 *
 * Revision 1.58  2003/04/30 20:43:07  oliverschwartz
 * Set backend version number to 1.4.26
 *
 * Revision 1.57  2003/04/02 21:17:14  oliverschwartz
 * Fix for 1200 DPI with Acer 5000
 *
 * Revision 1.56  2003/02/08 10:45:09  oliverschwartz
 * Use 600 DPI as optical resolution for Benq 5000
 *
 * Revision 1.55  2003/01/08 21:16:17  oliverschwartz
 * Added support for Acer / Benq 310U
 *
 * Revision 1.54  2002/12/10 20:14:12  oliverschwartz
 * Enable color offset correction for SnapScan300
 *
 * Revision 1.53  2002/10/31 19:29:41  oliverschwartz
 * Set version to 1.4.17
 *
 * Revision 1.52  2002/10/12 10:40:48  oliverschwartz
 * Added support for Snapscan e10
 *
 * Revision 1.51  2002/09/26 19:27:44  oliverschwartz
 * Version 1.4.16
 *
 * Revision 1.50  2002/09/24 16:07:44  oliverschwartz
 * Added support for Benq 5000
 *
 * Revision 1.49  2002/07/12 22:53:54  oliverschwartz
 * Version 1.4.15
 *
 * Revision 1.48  2002/07/12 22:53:16  oliverschwartz
 * call sanei_usb_init() before sanei_usb_attach_matching_devices()
 *
 * Revision 1.47  2002/06/06 21:16:23  oliverschwartz
 * Set backend version to 1.4.14
 *
 * Revision 1.46  2002/06/06 20:40:01  oliverschwartz
 * Changed default scan area for transparancy unit of SnapScan e50
 *
 * Revision 1.45  2002/05/02 18:29:34  oliverschwartz
 * - Added ADF support
 * - Fixed status handling after cancel
 *
 * Revision 1.44  2002/04/27 14:42:30  oliverschwartz
 * Cleanup of debug logging
 *
 * Revision 1.43  2002/04/23 22:40:33  oliverschwartz
 * Improve config file reading
 *
 * Revision 1.42  2002/04/10 21:00:09  oliverschwartz
 * Check for NULL pointer before deleting device list
 *
 * Revision 1.41  2002/03/24 12:12:36  oliverschwartz
 * - Moved option functions to snapscan-options.c
 * - Autodetect USB scanners
 * - Cleanup
 *
 * Revision 1.40  2002/02/09 14:55:23  oliverschwartz
 * Added language translation support (SANE_I18N)
 *
 * Revision 1.39  2002/01/23 20:40:54  oliverschwartz
 * Don't use quantization for scan area parameter
 * Improve recognition of Acer 320U
 * Version 1.4.7
 *
 * Revision 1.38  2002/01/14 21:11:56  oliverschwartz
 * Add workaround for bug semctl() call in libc for PPC
 *
 * Revision 1.37  2002/01/10 21:33:12  oliverschwartz
 * Set version number to 1.4.4
 *
 * Revision 1.36  2002/01/06 18:34:02  oliverschwartz
 * Added support for Snapscan e42 thanks to Yari Ad� Petralanda
 *
 * Revision 1.35  2001/12/20 23:18:01  oliverschwartz
 * Remove tmpfname
 *
 * Revision 1.34  2001/12/18 18:28:35  oliverschwartz
 * Removed temporary file
 *
 * Revision 1.33  2001/12/12 19:43:30  oliverschwartz
 * - Set version number to 1.4.3
 * - Clean up CVS Log
 *
 * Revision 1.32  2001/12/09 23:06:45  oliverschwartz
 * - use sense handler for USB if scanner reports CHECK_CONDITION
 *
 * Revision 1.31  2001/12/08 11:50:34  oliverschwartz
 * Fix dither matrix computation
 *
 * Revision 1.30  2001/11/29 22:50:14  oliverschwartz
 * Add support for SnapScan e52
 *
 * Revision 1.29  2001/11/27 23:16:17  oliverschwartz
 * - Fix color alignment for SnapScan 600
 * - Added documentation in snapscan-sources.c
 * - Guard against TL_X < BR_X and TL_Y < BR_Y
 *
 * Revision 1.28  2001/11/25 18:51:41  oliverschwartz
 * added support for SnapScan e52 thanks to Rui Lopes
 *
 * Revision 1.27  2001/11/16 20:28:35  oliverschwartz
 * add support for Snapscan e26
 *
 * Revision 1.26  2001/11/16 20:23:16  oliverschwartz
 * Merge with sane-1.0.6
 *   - Check USB vendor IDs to avoid hanging scanners
 *   - fix bug in dither matrix computation
 *
 * Revision 1.25  2001/10/25 11:06:22  oliverschwartz
 * Change snapscan backend version number to 1.4.0
 *
 * Revision 1.24  2001/10/11 14:02:10  oliverschwartz
 * Distinguish between e20/e25 and e40/e50
 *
 * Revision 1.23  2001/10/09 22:34:23  oliverschwartz
 * fix compiler warnings
 *
 * Revision 1.22  2001/10/08 19:26:01  oliverschwartz
 * - Disable quality calibration for scanners that do not support it
 *
 * Revision 1.21  2001/10/08 18:22:02  oliverschwartz
 * - Disable quality calibration for Acer Vuego 310F
 * - Use sanei_scsi_max_request_size as scanner buffer size
 *   for SCSI devices
 * - Added new devices to snapscan.desc
 *
 * Revision 1.20  2001/09/18 15:01:07  oliverschwartz
 * - Read scanner id string again after firmware upload
 *   to indentify correct model
 * - Make firmware upload work for AGFA scanners
 * - Change copyright notice
 *
 * Revision 1.19  2001/09/17 10:01:08  sable
 * Added model AGFA 1236U
 *
 * Revision 1.18  2001/09/10 10:16:32  oliverschwartz
 * better USB / SCSI recognition, correct max scan area for 1236+TPO
 *
 * Revision 1.17  2001/09/09 18:06:32  oliverschwartz
 * add changes from Acer (new models; automatic firmware upload for USB scanners); fix distorted colour scans after greyscale scans (call set_window only in sane_start); code cleanup
 *
 * Revision 1.16  2001/09/07 09:42:13  oliverschwartz
 * Sync with Sane-1.0.5
 *
 * Revision 1.15  2001/05/15 20:51:14  oliverschwartz
 * check for pss->devname instead of name in sane_open()
 *
 * Revision 1.14  2001/04/10 13:33:06  sable
 * Transparency adapter bug and xsane crash corrections thanks to Oliver Schwartz
 *
 * Revision 1.13  2001/04/10 13:00:31  sable
 * Moving sanei_usb_* to snapscani_usb*
 *
 * Revision 1.12  2001/04/10 11:04:31  sable
 * Adding support for snapscan e40 an e50 thanks to Giuseppe Tanzilli
 *
 * Revision 1.11  2001/03/17 22:53:21  sable
 * Applying Mikael Magnusson patch concerning Gamma correction
 * Support for 1212U_2
 *
 * Revision 1.4  2001/03/04 16:50:53  mikael
 * Added Scan Mode, Geometry, Enhancement and Advanced groups. Implemented brightness and contrast controls with gamma tables. Added Quality Calibration, Analog Gamma Bind, Custom Gamma and Gamma Vector GS,R,G,B options.
 *
 * Revision 1.3  2001/02/16 18:32:28  mikael
 * impl calibration, signed position, increased buffer size
 *
 * Revision 1.2  2001/02/10 18:18:29  mikael
 * Extended x and y ranges
 *
 * Revision 1.1.1.1  2001/02/10 17:09:29  mikael
 * Imported from snapscan-11282000.tar.gz
 *
 * Revision 1.10  2000/11/10 01:01:59  sable
 * USB (kind of) autodetection
 *
 * Revision 1.9  2000/11/01 01:26:43  sable
 * Support for 1212U
 *
 * Revision 1.8  2000/10/30 22:31:13  sable
 * Auto preview mode
 *
 * Revision 1.7  2000/10/28 14:16:10  sable
 * Bug correction for SnapScan310
 *
 * Revision 1.6  2000/10/28 14:06:35  sable
 * Add support for Acer300f
 *
 * Revision 1.5  2000/10/15 19:52:06  cbagwell
 * Changed USB support to a 1 line modification instead of multi-file
 * changes.
 *
 * Revision 1.4  2000/10/13 03:50:27  cbagwell
 * Updating to source from SANE 1.0.3.  Calling this versin 1.1
 *
 * Revision 1.3  2000/08/12 15:09:35  pere
 * Merge devel (v1.0.3) into head branch.
 *
 * Revision 1.1.1.1.2.5  2000/07/29 16:04:33  hmg
 * 2000-07-29  Henning Meier-Geinitz <hmg@gmx.de>
 *
 *         * backend/GUIDE: Added some comments about portability and
 *           documentation.
 *         * backend/abaton.c backend/agfafocus.c backend/apple.c
 *           backend/canon.c backend/coolscan.c backend/dc210.c backend/dc25.c
 *           backend/dll.c backend/dmc.c backend/microtek.c backend/microtek2.c
 *            backend/microtek2.c backend/mustek_pp.c backend/net.c backend/pint.c
 *           backend/pnm.c backend/qcam.c backend/ricoh.c backend/s9036.c
 *           backend/sane_strstatus.c backend/sharp.c backend/snapscan.c
 *           backend/st400.c backend/stubs.c backend/tamarack.c backend/v4l.c:
 *           Changed include statements from #include <sane/...> to
 *           #include "sane...".
 *         * backend/avision.c backend/dc25.c: Use DBG(0, ...) instead of
 *            fprintf (stderr, ...)
 *         * backend/avision.c backend/canon-sane.c backend/coolscan.c
 *           backend/dc25.c backend/microtek.c backend/microtek2.c
 *            backend/st400.c: Use sanei_config_read() instead of fgets().
 *         * backend/coolscan.desc backend/microtek.desc backend/microtek2.desc
 *           backend/st400.desc: Added :interface and :manpage entries.
 *         * backend/nec.desc: Status is beta now (was: new). Fixed typo.
 *         * doc/canon.README: Removed, because the information is included in
 *            the manpage now.
 *         * doc/Makefile.in: Added sane-coolscan to list of mapages to install.
 *         * README: Added Link to coolscan manpage.
 *         * backend/mustek.*: Update to Mustek backend 1.0-94. Fixed the
 *           #include <sane/...> bug.
 *
 * Revision 1.1.1.1.2.4  2000/07/25 21:47:43  hmg
 * 2000-07-25  Henning Meier-Geinitz <hmg@gmx.de>
 *
 *         * backend/snapscan.c: Use DBG(0, ...) instead of fprintf (stderr, ...).
 *         * backend/abaton.c backend/agfafocus.c backend/apple.c backend/dc210.c
 *            backend/dll.c backend/dmc.c backend/microtek2.c backend/pint.c
 *           backend/qcam.c backend/ricoh.c backend/s9036.c backend/snapscan.c
 *           backend/tamarack.c: Use sanei_config_read instead of fgets.
 *         * backend/dc210.c backend/microtek.c backend/pnm.c: Added
 *           #include "../include/sane/config.h".
 *         * backend/dc25.c backend/m3096.c  backend/sp15.c
 *            backend/st400.c: Moved #include "../include/sane/config.h" to the beginning.
 *         * AUTHORS: Changed agfa to agfafocus.
 *
 * Revision 1.1.1.1.2.3  2000/07/17 21:37:28  hmg
 * 2000-07-17  Henning Meier-Geinitz <hmg@gmx.de>
 *
 *         * backend/snapscan.c backend/snapscan-scsi.c: Replace C++ comment
 *           with C comment.
 *
 * Revision 1.1.1.1.2.2  2000/07/13 04:47:46  pere
 * New snapscan backend version dated 20000514 from Steve Underwood.
 *
 * Revision 1.2  2000/05/14 13:30:20  coppice
 * R, G and B images now merge correctly. Still some outstanding issues,
 * but a lot more useful than before.
 *
 * Revision 1.2  2000/03/05 13:55:20  pere
 * Merged main branch with current DEVEL_1_9.
 *
 * Revision 1.1.1.1.2.1  1999/09/15 18:20:44  charter
 * Early version 1.0 snapscan.c
 *
 * Revision 2.2  1999/09/09 18:22:45  charter
 * Checkpoint. Now using Sources for scanner data, and have removed
 * references to the old snapscan-310.c stuff. This stuff must still
 * be incorporated into the RGBRouter to get trilinear CCD SnapScan
 * models working.
 *
 * Revision 2.1  1999/09/08 03:07:05  charter
 * Start of branch 2; same as 1.47.
 *
 * Revision 1.47  1999/09/08 03:03:53  charter
 * The actions for the scanner command options now use fprintf for
 * printing, rather than DGB. I want the output to come out no matter
 * what the value of the snapscan debug level.
 *
 * Revision 1.46  1999/09/07 20:53:41  charter
 * Changed expected_data_len to bytes_remaining.
 *
 * Revision 1.45  1999/09/06 23:32:37  charter
 * Split up sane_start() into sub-functions to improve readability (again).
 * Introduced actual_mode() and is_colour_mode() (again).
 * Fixed problems with cancellation. Works fine with my system now.
 *
 * Revision 1.44  1999/09/02 05:28:01  charter
 * Added Gary Plewa's name to the list of copyrighted contributors.
 *
 * Revision 1.43  1999/09/02 05:23:54  charter
 * Added Gary Plewa's patch for the Acer PRISA 620s.
 *
 * Revision 1.42  1999/09/02 02:05:34  charter
 * Check-in of revision 1.42 (release 0.7 of the backend).
 * This is part of the recovery from the great disk crash of Sept 1, 1999.
 *
 * Revision 1.42  1999/07/09 22:37:55  charter
 * Potential bugfix for problems with sane_get_parameters() and
 * the new generic scsi driver (suggested by Francois Desarmeni,
 * Douglas Gilbert, Abel Deuring).
 *
 * Revision 1.41  1999/07/09 20:58:07  charter
 * Changes to support SnapScan 1236s (Petter Reinholdsten).
 *
 * Revision 1.40  1998/12/16 18:43:06  charter
 * Fixed major version problem precipitated by release of SANE-1.00.
 *
 * Revision 1.39  1998/09/07  06:09:26  charter
 * Formatting (whitespace) changes.
 *
 * Revision 1.38  1998/09/07  06:06:01  charter
 * Merged in Wolfgang Goeller's changes (Vuego 310S, bugfixes).
 *
 * Revision 1.37  1998/08/06  06:16:39  charter
 * Now using sane_config_attach_matching_devices() in sane_snapscan_init().
 * Change contributed by David Mosberger-Tang.
 *
 * Revision 1.36  1998/05/11  17:02:53  charter
 * Added Mikko's threshold stuff.
 *
 * Revision 1.35  1998/03/10 23:43:23  eblot
 * Bug correction
 *
 * Revision 0.72  1998/03/10 23:40:42  eblot
 * More support for 310/600 models: color preview, large window
 *
 * Revision 1.35  1998/03/10  21:32:07  eblot
 * Debugging
 *
 * Revision 1.34  1998/02/15  21:55:53  charter
 * From Emmanuel Blot:
 * First routines to support SnapScan 310 scanned data.
 *
 * Revision 1.33  1998/02/06  02:30:28  charter
 * Now using a mode enum (instead of the static string pointers directly).
 * Now check for the SnapScan 310 and 600 explicitly (start of support
 * for these models).
 *
 * Revision 1.32  1998/02/01  21:56:48  charter
 * Patches to fix compilation problems on Solaris supplied by
 * Jim McBeath.
 *
 * Revision 1.31  1998/02/01  03:36:40  charter
 * Now check for BRX < TLX and BRY < TLY and whether the area of the
 * scanning window is approaching zero in set_window. I'm setting a
 * minimum window size of 75x75 hardware pixels (0.25 inches a side).
 * If the area falls to zero, the scanner seems to hang in the middle
 * of the set_window command.
 *
 * Revision 1.30  1998/02/01  00:00:33  charter
 * TLX, TLY, BRX and BRY are now lengths expressed in mm. The frontends
 * can now allow changes in the units, and units that are more user-
 * friendly.
 *
 * Revision 1.29  1998/01/31  21:09:19  charter
 * Fixed another problem with add_device(): if mini_inquiry ends
 * up indirectly invoking the sense handler, there'll be a segfault
 * because the sense_handler isn't set. Had to fix sense_handler so
 * it can handle a NULL pss pointer and then use the sanei_scsi stuff
 * everywhere. This error is most likely to occur if the scanner is
 * turned off.
 *
 * Revision 1.28  1998/01/31  18:45:22  charter
 * Last fix botched, produced a compile error. Thought I'd already
 * compiled successfully.
 *
 * Revision 1.27  1998/01/31  18:32:42  charter
 * Fixed stupid bug in add_device that causes segfault when no snapscan
 * found: closing a scsi fd opened with open() using sanei_scsi_close().
 *
 * Revision 1.26  1998/01/30  21:19:02  charter
 * sane_snapscan_init() handles failure of add_device() in the same
 * way when there is no snapscan.conf file available as when there is
 * one.
 *
 * Revision 1.25  1998/01/30  19:41:11  charter
 * Waiting for child process termination at regular end of scan (not
 * just on cancellation); before I was getting zombies.
 *
 * Revision 1.24  1998/01/30  19:19:27  charter
 * Changed from strncmp() to strncasecmp() to do vendor and model
 * comparisons in sane_snapscan_init. There are some snapcsan models
 * that use lower case.
 * Now have debug level defines instead of raw numbers, and better debug
 * information categories.
 * Don't complain at debug level 0 when a snapscan isn't found on a
 * requested device.
 * Changed CHECK_STATUS to take caller parameter instead of always
 * assuming an available string "me".
 *
 * Revision 1.23  1998/01/30  11:03:04  charter
 * Fixed * vs [] operator precedence screwup in sane_snapscan_get_devices()
 * that caused a segfault in scanimage -h.
 * Fixed problem with not closing the scsi fd between certain commands
 * that caused scanimage to hang; now using open_scanner() and close_scanner().
 *
 * Revision 1.22  1998/01/28  09:02:55  charter
 * Fixed bug: zero allocation length in request sense command buffer
 * was preventing sense information from being received. The
 * backend now correctly waits for the scanner to warm up.
 * Now using the hardware configuration byte to check whether
 * both 8x8 and 16x16 halftoning should be made available.
 *
 * Revision 1.21  1998/01/25  09:57:57  charter
 * Added more SCSI command buttons (and a group for them).
 * Made the output of the Inquiry command a bit nicer.
 *
 * Revision 1.20  1998/01/25  08:53:14  charter
 * Have added bi-level colour mode, with halftones too.
 * Can now select preview mode (but it's an advanced option, since
 * you usually don't want to do it).
 *
 * Revision 1.19  1998/01/25  02:25:02  charter
 * Fixed bug: preview mode gives blank image at initial startup.
 * Fixed bug: lineart mode goes weird after a preview or gs image.
 * More changes to option relationships;
 * now using test_unit_ready and send_diagnostic in sane_snapscan_open().
 * Added negative option.
 *
 * Revision 1.18  1998/01/24  05:15:32  charter
 * Now have RGB gamma correction and dispersed-dot dither halftoning
 * for BW images. Cleaned up some spots in the code and have set up
 * option interactions a bit better (e.g. halftoning and GS gamma
 * correction made inactive in colour mode, etc). TL_[XY] and BR_[XY]
 * now change in ten-pixel increments (I had problems with screwed-up
 * scan lines when the dimensions were weird at low res... could be the
 * problem).
 *
 * Revision 1.17  1998/01/23  13:03:17  charter
 * Several changes, all aimed at getting scanning performance working
 * correctly, and the progress/cancel window functioning. Cleaned up
 * a few nasty things as well.
 *
 * Revision 1.16  1998/01/23  07:40:23  charter
 * Reindented using GNU convention at David Mosberger-Tang's request.
 * Also applied David's patch fixing problems on 64-bit architectures.
 * Now using scanner's reported speed to guage amount of data to request
 * in a read on the scsi fd---nonblocking mode operates better now.
 * Fixed stupid bug I introduced in preview mode data transfer.
 *
 * Revision 1.15  1998/01/22  06:18:57  charter
 * Raised the priority of a couple of DBG messages in reserve_unit()
 * and release_unit(), and got rid of some unecessary ones.
 *
 * Revision 1.14  1998/01/22  05:15:35  charter
 * Have replaced the bit depth option with a mode option; various
 * changes associated with that.
 * Also, I again close the STDERR_FILENO in the reader child and
 * dup the STDOUT file descriptor onto it. This prevents an "X io"
 * error when the child exits, while still allowing the use of
 * DBG.
 *
 * Revision 1.13  1998/01/21  20:41:22  charter
 * Added copyright info.
 * Also now seem to have cancellation working. This requires using a
 * new scanner state variable and checking in all the right places
 * in the reader child and the sane_snapscan_read function. I've
 * tested it using both blocking and nonblocking I/O and it seems
 * to work both ways.
 * I've also switched to GTK+-0.99.2 and sane-0.69, and the
 * mysterious problems with the preview window have disappeared.
 * Problems with scanimage doing weird things to options have also
 * gone away and the frontends seem more stable.
 *
 * Revision 1.12  1998/01/21  11:05:53  charter
 * Inoperative code largely #defined out; I had the preview window
 * working correctly by having the window coordinates properly
 * constrained, but now the preview window bombs with a floating-
 * point error each time... I'm not sure yet what happened.
 * I've also figured out that we need to use reserve_unit and
 * release_unit in order to cancel scans in progress. This works
 * under scanimage, but I can't seem to find a way to fit cancellation
 * into xscanimage properly.
 *
 * Revision 1.11  1998/01/20  22:42:08  charter
 * Applied Franck's patch from Dec 17; preview mode is now grayscale.
 *
 * Revision 1.10  1997/12/10  23:33:12  charter
 * Slight change to some floating-point computations in the brightness
 * and contrast stuff. The controls don't seem to do anything to the
 * scanner though (I think these aren't actually supported in the
 * SnapScan).
 *
 * Revision 1.9  1997/11/26  15:40:50  charter
 * Brightness and contrast added by Michel.
 *
 * Revision 1.8  1997/11/12  12:55:40  charter
 * No longer exec after forking to do nonblocking scanning; found how
 * to fix the problems with SIGPIPEs from before.
 * Now support a config file like the other scanner drivers, and
 * can check whether a given device is an AGFA SnapScan (mini_inquiry()).
 *
 * Revision 1.7  1997/11/10  05:52:08  charter
 * Now have the child reader process and pipe stuff working, and
 * nonblocking mode. For large scans the nonblocking mode actually
 * seems to cut down on cpu hogging (though there is still a hit).
 *
 * Revision 1.6  1997/11/03  07:45:54  charter
 * Added the predef_window stuff. I've tried it with 6x4, and it seems
 * to work; I think something gets inconsistent if a preview is
 * performed though.
 *
 * Revision 1.5  1997/11/03  03:15:27  charter
 * Global static variables have now become part of the scanner structure;
 * the inquiry command automatically retrieves window parameters into
 * scanner structure members. Things are a bit cleaned up.
 *
 * Revision 1.4  1997/11/02  23:35:28  charter
 * After much grief.... I can finally scan reliably. Now it's a matter
 * of getting the band arrangement sorted out.
 *
 * Revision 1.3  1997/10/30  07:36:37  charter
 * Fixed a stupid bug in the #defines for the inquiry command, pointed out
 * by Franck.
 *
 * Revision 1.2  1997/10/14  06:00:11  charter
 * Option manipulation and some basic SCSI commands done; the basics
 * for scanning are written but there are bugs. A full scan always hangs
 * the SCSI driver, and preview mode scans complete but it isn't clear
 * whether any meaningful data is received.
 *
 * Revision 1.1  1997/10/13  02:25:54  charter
 * Initial revision
 * */
