/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 Geoffrey T. Dairiki
   Support for HP PhotoSmart Photoscanner by Peter Kirchgessner
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

   This file is part of a SANE backend for HP Scanners supporting
   HP Scanner Control Language (SCL).
*/

static char *hp_backend_version = "1.06";
static char *hp_backend_revision = "$Revision$";
/* Changes:

   V 1.06:
   $Log$
   Revision 1.22  2008/11/26 21:21:25  kitno-guest
   * backend/ *.[ch]: nearly every backend used V_MAJOR
   instead of SANE_CURRENT_MAJOR in sane_init()
   * backend/snapscan.c: remove EXPECTED_VERSION check
   since new SANE standard is forward compatible

   Revision 1.21  2004-10-04 18:09:05  kig-guest
   Rename global function hp_init_openfd to sanei_hp_init_openfd

   Revision 1.20  2004/03/27 13:52:39  kig-guest
   Keep USB-connection open (was problem with Linux 2.6.x)


   V 1.05:
   Revision 1.19  2003/10/24 17:26:07  kig-guest
   Use new sanei-thread-interface

   Revision 1.18  2003/10/09 19:37:29  kig-guest
   Redo when TEST UNIT READY failed
   Redo when read returns with 0 bytes (non-SCSI only)
   Bug #300241: fix invers image on 3c/4c/6100C at 10 bit depth

   Revision 1.17  2003/10/06 19:54:07  kig-guest
   Bug #300248: correct "Negatives" to "Negative" in option description
 

   V 1.04, 24-Jul-2003, PK (peter@kirchgessner.net)
      - Add internationalization

   V 1.03, 14-Apr-2003, PK (peter@kirchgessner.net)
      - check valp in call of sane_control_option()

   V 1.02, 02-Feb-2003, PK (peter@kirchgessner.net)
      - add OS/2-support by Franz Bakan

   V 1.01, 06-Dec-2002, PK (peter@kirchgessner.net)
      - add option dumb-read to work around problems
        with BusLogic SCSI driver (error during device I/O)

   V 1.00, 17-Nov-2002, PK (peter@kirchgessner.net)
      - add libusb support

   V 0.96, 05-Aug-2002, PK (peter@kirchgessner.net)
      - check USB device names

   V 0.95, 07-Jul-2001, PK (peter@kirchgessner.net)
      - add support for active XPA
      - check if paper in ADF for ADF scan
      - add option lamp off
      - remove some really unused parameters

   V 0.94, 31-Dec-2000, PK (peter@kirchgessner.net)
      - always switch off lamp after scan

   V 0.93, 04-Dec-2000, PK (peter@kirchgessner.net)
      - fix problem with ADF-support on ScanJet 6350 (and maybe others)

   V 0.92, 03-Oct-2000, Rupert W. Curwen (rcurwen@uk.research.att.com):
      - try to not allocate accessors twice (only for accessors
        that have fixed length)
      - fix problem with leaving connection open for some error conditions

   V 0.91, 04-Sep-2000, David Paschal (paschal@rcsis.com):
      - Added support for flatbed HP OfficeJets
      - (PK) fix problem with cancel preview

   V 0.90, 02-Sep-2000, PK:
      - fix timing problem between killing child and writing to pipe
      - change fprintf(stderr,...) to DBG
      - change include <sane..> to "sane.." in hp.h
      - change handling of options that have global effects.
        i.e. if option scanmode is received (has global effect),
        all options that "may change" are send to the scanner again.
        This fixes a problem that --resolution specified infront of
        --mode on command line of scanimage was ignored.
        NOTE: This change does not allow to specify --depth 12 infront of
        --mode color, because --depth is only enabled with --mode color.
      - add depth greater 8 bits for mode grayscale
      - add option for 8 bit output but 10/12 bit scanning
   V 0.88, 25-Jul-2000, PK:
      - remove inlines
   V 0.88, 20-Jul-2000, PK:
      - Use sanei_config_read()
      - dont write chars < 32 to DBG
   V 0.88, 09-Jul-2000, PK:
      - Add front button support by Chris S. Cowles, Houston, Texas,
        c_cowles@ieee.org
   V 0.87, 28-Jun-2000, PK:
      - ADF-support for ScanJet IIp
      - Return error SANE_STATUS_NO_DOCS if no paper in ADF
   V 0.86, 12-Feb-2000, PK:
      - fix gcc warnings
      - fix problems with bitdepths > 8
      - allow hp_data_resize to be called with newsize==bufsiz
        (Jens Heise, <heisbeee@calvados.zrz.TU-Berlin.DE>)
      - add option enable-image-buffering
   V 0.85, 30-Jan-2000, PK:
      - correct and enhace data widths > 8 (Ewald de Wit  <ewald@pobox.com>)
      - enable data width for all scanners
      - PhotoSmart: exposure "Off" changed to "Default"
      - PhotoSmart: even if max. datawidth 24 is reported, allow 30 bits.
      - change keyword -data-width to -depth and use value for bits per sample
      - change keyword -halftone-type to -halftone-pattern
      - change keyword -scantype to -source
      - fix problem with multiple definition of sanei_debug_hp
   V 0.83, 04-Jul-99, PK:
      - reset scanner before downloading parameters (fixes problem
        with sleep mode of scanners)
      - fix problem with coredump if non-scanner HP SCSI devices
        are connected (CDR)
      - option scan-from-adf replaced by scantype normal/adf/xpa
      - change value "Film strip" to "Film-strip" for option
        --media-type
      - PhotoScanner: allow only scanning at multiple of 300 dpi
        for scanning slides/film strips. This also fixes a problem with the
        preview which uses arbitrary resolutions.
      - Marian Szebenyi: close pipe (endless loop on Digital UNIX)

   V 0.82, 28-Feb-99, Ewald de Wit <ewald@pobox.com>:
      - add options 'exposure time' and 'data width'

   V 0.81, 11-Jan-99, PK:
      - occasionally 'scan from ADF' was active for Photoscanner

   V 0.80, 10-Jan-99, PK:
      - fix problem with scan size for ADF-scan
        (thanks to Christop Biardzki <cbi@allgaeu.org> for tests)
      - add option "unload after scan" for HP PhotoScanner
      - no blanks in command line options
      - fix problem with segmentation fault for scanimage -d hp:/dev/sga
        with /dev/sga not included in hp.conf

   V 0.72, 25-Dec-98, PK:
      - add patches from mike@easysw.com to fix problems:
        - core dumps by memory alignment
        - config file to accept matching devices (scsi HP)
      - add simulation for brightness/contrast/custom gamma table
        if not supported by scanner
      - add configuration options for connect-...

   V 0.72c, 04-Dec-98, PK:
      - use sanei_pio
      - try ADF support

   V 0.72b, 29-Nov-98 James Carter <james@cs.york.ac.uk>, PK:
      - try to add parallel scanner support

   V 0.71, 14-Nov-98 PK:
      - add HP 6200 C
      - cleanup hp_scsi_s structure
      - show calibrate button on photoscanner only for print media
      - suppress halftone mode on photoscanner
      - add media selection for photoscanner

   V 0.70, 26-Jul-98 PK:
      - Rename global symbols to sanei_...
        Change filenames to hp-...
        Use backend name hp

   V 0.65, 18-Jul-98 PK:
      - Dont use pwd.h for VACPP-Compiler to get home-directory,
        check $SANE_HOME_XHP instead

   V 0.64, 12-Jul-98 PK:
      - only download calibration file for media = 1 (prints)
      - Changes for VACPP-Compiler (check macros __IBMC__, __IBMCPP__)

   V 0.63, 07-Jun-98 PK:
      - fix problem with custom gamma table
      - Add unload button

   V 0.62, 25-May-98 PK:
      - make it compilable under sane V 0.73

   V 0.61, 28-Mar-98, Peter Kirchgessner <pkirchg@aol.com>:
      - Add support for HP PhotoSmart Photoscanner
      - Use more inquiries to see what the scanner supports
      - Add options: calibrate/Mirror horizontal+vertical
      - Upload/download calibration data
*/

#define VERSIO                                8

#include "../include/sane/config.h"
#include "hp.h"

#include <string.h>
/* #include <sys/types.h> */
/* #include "../include/sane/sane.h" */
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/sanei_thread.h"
/* #include "../include/sane/sanei_debug.h" */
#include "hp-device.h"
#include "hp-handle.h"

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

#ifndef NDEBUG
#include <ctype.h>
void
sanei_hp_dbgdump (const void * bufp, size_t len)
{
  const hp_byte_t *buf	= bufp;
  int		offset	= 0;
  int		i;
  char line[128], pt[32];

  for (offset = 0; offset < (int)len; offset += 16)
    {
      sprintf (line," 0x%04X ", offset);
      for (i = offset; i < offset + 16 && i < (int)len; i++)
      {
	  sprintf (pt," %02X", buf[i]);
          strcat (line, pt);
      }
      while (i++ < offset + 16)
	  strcat (line, "   ");
      strcat (line, "  ");
      for (i = offset; i < offset + 16 && i < (int)len; i++)
      {
	  sprintf (pt, "%c", isprint(buf[i]) ? buf[i] : '.');
          strcat (line, pt);
      }
      DBG(16,"%s\n",line);
    }
}

#endif

typedef struct info_list_el_s * HpDeviceInfoList;
struct info_list_el_s
{
    HpDeviceInfoList    next;
    HpDeviceInfo        info;
};

typedef struct device_list_el_s * HpDeviceList;
struct device_list_el_s
{
    HpDeviceList	next;
    HpDevice	 	dev;
};

/* Global state */
static struct hp_global_s {
    hp_bool_t	is_up;
    hp_bool_t	config_read;

    const SANE_Device ** devlist;

    HpDeviceList	device_list;
    HpDeviceList	handle_list;
    HpDeviceInfoList    infolist;

    HpDeviceConfig      config;
} global;


/* Get the info structure for a device. If not available in global list */
/* add new entry and return it */
static HpDeviceInfo *
hp_device_info_create (const char *devname)

{
 HpDeviceInfoList  *infolist = &(global.infolist);
 HpDeviceInfoList  infolistelement;
 HpDeviceInfo *info;
 int k, found;

 if (!global.is_up) return 0;

 found = 0;
 infolistelement = 0;
 info = 0;
 while (*infolist)
 {
   infolistelement = *infolist;
   info = &(infolistelement->info);
   if (strcmp (info->devname, devname) == 0)  /* Already in list ? */
   {
     found = 1;
     break;
   }
   infolist = &(infolistelement->next);
 }

 if (found)  /* Clear old entry */
 {
   memset (infolistelement, 0, sizeof (*infolistelement));
 }
 else   /* New element */
 {
   infolistelement = (HpDeviceInfoList)
                        sanei_hp_allocz (sizeof (*infolistelement));
   if (!infolistelement) return 0;
   info = &(infolistelement->info);
   *infolist = infolistelement;
 }

 k = sizeof (info->devname);
 strncpy (info->devname, devname, k);
 info->devname[k-1] = '\0';
 info->max_model = -1;
 info->active_xpa = -1;

 return info;
}

static void
hp_init_config (HpDeviceConfig *config)

{
  if (config)
  {
    config->connect = HP_CONNECT_SCSI;
    config->use_scsi_request = 1;
    config->use_image_buffering = 0;
    config->got_connect_type = 0;
    config->dumb_read = 0;
  }
}

static HpDeviceConfig *
hp_global_config_get (void)

{
 if (!global.is_up) return 0;
 return &(global.config);
}

static SANE_Status
hp_device_config_add (const char *devname)

{
 HpDeviceInfo *info;
 HpDeviceConfig *config;

 info = hp_device_info_create (devname);
 if (!info) return SANE_STATUS_INVAL;

 config = hp_global_config_get ();

 if (config)
 {
   memcpy (&(info->config), config, sizeof (info->config));
   info->config_is_up = 1;
 }
 else     /* Initialize with default configuration */
 {
   DBG(3, "hp_device_config_add: No configuration found for device %s.\n\tUseing default\n",
       devname);
   hp_init_config (&(info->config));
   info->config_is_up = 1;
 }
 return SANE_STATUS_GOOD;
}

HpDeviceInfo *
sanei_hp_device_info_get (const char *devname)

{
 HpDeviceInfoList  *infolist;
 HpDeviceInfoList  infolistelement;
 HpDeviceInfo *info;
 int retries = 1;

 if (!global.is_up)
 {
   DBG(17, "sanei_hp_device_info_get: global.is_up = %d\n", (int)global.is_up);
   return 0;
 }

 DBG(250, "sanei_hp_device_info_get: searching %s\n", devname);
 do
 {
 infolist = &(global.infolist);
 while (*infolist)
 {
   infolistelement = *infolist;
   info = &(infolistelement->info);
   DBG(250, "sanei_hp_device_info_get: check %s\n", info->devname);
   if (strcmp (info->devname, devname) == 0)  /* Found ? */
   {
     return info;
   }
   infolist = &(infolistelement->next);
 }

 /* No configuration found. Assume default */
 DBG(1, "hp_device_info_get: device %s not configured. Using default\n",
     devname);
 if (hp_device_config_add (devname) != SANE_STATUS_GOOD)
   return 0;
 }
 while (retries-- > 0);

 return 0;
}

HpDevice
sanei_hp_device_get (const char *devname)
{
  HpDeviceList  ptr;

  for (ptr = global.device_list; ptr; ptr = ptr->next)
      if (strcmp(sanei_hp_device_sanedevice(ptr->dev)->name, devname) == 0)
	  return ptr->dev;

  return 0;
}

static void
hp_device_info_remove (void)
{
 HpDeviceInfoList  next, infolistelement = global.infolist;
 HpDeviceInfo *info;

 if (!global.is_up) return;

 while (infolistelement)
 {
   info = &(infolistelement->info);
   next = infolistelement->next;
   sanei_hp_free (infolistelement);
   infolistelement = next;
 }
}

static SANE_Status
hp_device_list_add (HpDeviceList * list, HpDevice dev)
{
  HpDeviceList new = sanei_hp_alloc(sizeof(*new));

  if (!new)
      return SANE_STATUS_NO_MEM;
  while (*list)
      list = &(*list)->next;

  *list = new;
  new->next = 0;
  new->dev = dev;
  return SANE_STATUS_GOOD;
}

static SANE_Status
hp_device_list_remove (HpDeviceList * list, HpDevice dev)
{
  HpDeviceList old;

  while (*list && (*list)->dev != dev)
      list = &(*list)->next;

  if (!*list)
      return SANE_STATUS_INVAL;

  old = *list;
  *list = (*list)->next;
  sanei_hp_free(old);
  return SANE_STATUS_GOOD;
}

static SANE_Status
hp_handle_list_add (HpDeviceList * list, HpHandle h)
{
  return hp_device_list_add(list, (HpDevice)h);
}

static SANE_Status
hp_handle_list_remove (HpDeviceList * list, HpHandle h)
{
  return hp_device_list_remove(list, (HpDevice)h);
}




static SANE_Status
hp_init (void)
{
  memset(&global, 0, sizeof(global));
  global.is_up++;
  DBG(3, "hp_init: global.is_up = %d\n", (int)global.is_up);
  return SANE_STATUS_GOOD;
}

static void
hp_destroy (void)
{
  if (global.is_up)
    {
      /* Close open handles */
      while (global.handle_list)
	  sane_close(global.handle_list->dev);

      /* Remove device infos */
      hp_device_info_remove ();

      sanei_hp_free_all();
      global.is_up = 0;
      DBG(3, "hp_destroy: global.is_up = %d\n", (int)global.is_up);
    }
}

static SANE_Status
hp_get_dev (const char *devname, HpDevice* devp)
{
  HpDeviceList  ptr;
  HpDevice	new;
  const HpDeviceInfo *info;
  char         *connect;
  HpConnect     hp_connect;
  SANE_Status   status;

  for (ptr = global.device_list; ptr; ptr = ptr->next)
      if (strcmp(sanei_hp_device_sanedevice(ptr->dev)->name, devname) == 0)
	{
	  if (devp)
	      *devp = ptr->dev;
	  return SANE_STATUS_GOOD;
	}

  info = sanei_hp_device_info_get (devname);
  hp_connect = info->config.connect;

  if (hp_connect == HP_CONNECT_SCSI) connect = "scsi";
  else if (hp_connect == HP_CONNECT_DEVICE) connect = "device";
  else if (hp_connect == HP_CONNECT_PIO) connect = "pio";
  else if (hp_connect == HP_CONNECT_USB) connect = "usb";
  else if (hp_connect == HP_CONNECT_RESERVE) connect = "reserve";
  else connect = "unknown";

  DBG(3, "hp_get_dev: New device %s, connect-%s, scsi-request=%lu\n",
      devname, connect, (unsigned long)info->config.use_scsi_request);

  if (!ptr)
  {
     status =  sanei_hp_device_new (&new, devname);

     if ( status != SANE_STATUS_GOOD )
       return status;
  }

  if (devp)
      *devp = new;

  RETURN_IF_FAIL( hp_device_list_add(&global.device_list, new) );

  return SANE_STATUS_GOOD;
}

static SANE_Status
hp_attach (const char *devname)
{
  DBG(7,"hp_attach: \"%s\"\n", devname);
  hp_device_config_add (devname);
  return hp_get_dev (devname, 0);
}

static void
hp_attach_matching_devices (HpDeviceConfig *config, const char *devname)
{
 static int usb_initialized = 0;

 if (strncmp (devname, "usb", 3) == 0)
 {
   config->connect = HP_CONNECT_USB;
   config->use_scsi_request = 0;
   DBG(1,"hp_attach_matching_devices: usb attach matching \"%s\"\n",devname);
   if (!usb_initialized)
   {
      sanei_usb_init ();
      usb_initialized = 1;
   }
   sanei_usb_attach_matching_devices (devname, hp_attach);
 }
 else
 {
   DBG(1, "hp_attach_matching_devices: attach matching %s\n", devname);
   sanei_config_attach_matching_devices (devname, hp_attach);
 }
}

static SANE_Status
hp_read_config (void)
{
  FILE *	fp;
  char		 buf[PATH_MAX], arg1[PATH_MAX], arg2[PATH_MAX], arg3[PATH_MAX];
  int           nl, nargs;
  HpDeviceConfig *config, df_config, dev_config;
  hp_bool_t     is_df_config;
  char          cu_device[PATH_MAX];

  if (!global.is_up)
      return SANE_STATUS_INVAL;
  if (global.config_read)
      return SANE_STATUS_GOOD;

  /* The default config will keep options set up until the first device is specified */
  hp_init_config (&df_config);
  config = &df_config;
  is_df_config = 1;
  cu_device[0] = '\0';

  DBG(1, "hp_read_config: hp backend v%s/%s starts reading config file\n",
      hp_backend_version, hp_backend_revision);

  if ((fp = sanei_config_open(HP_CONFIG_FILE)) != 0)
    {
      while (sanei_config_read(buf, sizeof(buf), fp))
	{
	  char *dev_name;

          nl = strlen (buf);
          while (nl > 0)
          {
            nl--;
            if (   (buf[nl] == ' ') || (buf[nl] == '\t')
                || (buf[nl] == '\r') || (buf[nl] == '\n'))
              buf[nl] = '\0';
            else
              break;
          }

          DBG(1, "hp_read_config: processing line <%s>\n", buf);

          nargs = sscanf (buf, "%s%s%s", arg1, arg2, arg3);
          if ((nargs <= 0) || (arg1[0] == '#')) continue;

          /* Option to process ? */
          if ((strcmp (arg1, "option") == 0) && (nargs >= 2))
          {
            if (strcmp (arg2, "connect-scsi") == 0)
            {
              config->connect = HP_CONNECT_SCSI;
              config->got_connect_type = 1;
            }
            else if (strcmp (arg2, "connect-device") == 0)
            {
              config->connect = HP_CONNECT_DEVICE;
              config->got_connect_type = 1;
              config->use_scsi_request = 0;
            }
            else if (strcmp (arg2, "connect-pio") == 0)
            {
              config->connect = HP_CONNECT_PIO;
              config->got_connect_type = 1;
              config->use_scsi_request = 0;
            }
            else if (strcmp (arg2, "connect-usb") == 0)
            {
              config->connect = HP_CONNECT_USB;
              config->got_connect_type = 1;
              config->use_scsi_request = 0;
            }
            else if (strcmp (arg2, "connect-reserve") == 0)
            {
              config->connect = HP_CONNECT_RESERVE;
              config->got_connect_type = 1;
              config->use_scsi_request = 0;
            }
            else if (strcmp (arg2, "disable-scsi-request") == 0)
            {
              config->use_scsi_request = 0;
            }
            else if (strcmp (arg2, "enable-image-buffering") == 0)
            {
              config->use_image_buffering = 1;
            }
            else if (strcmp (arg2, "dumb-read") == 0)
            {
              config->dumb_read = 1;
            }
            else
            {
              DBG(1,"hp_read_config: Invalid option %s\n", arg2);
            }
          }
          else   /* No option. This is the start of a new device */
          {
            if (is_df_config) /* Did we only read default configurations ? */
            {
              is_df_config = 0;  /* Stop reading default config */
                          /* Initialize device config with default-config */
              memcpy (&dev_config, &df_config, sizeof (dev_config));
              config = &dev_config;   /* Start reading a device config */
            }
            if (cu_device[0] != '\0')  /* Did we work on a device ? */
            {
              memcpy (hp_global_config_get(), &dev_config,sizeof (dev_config));
              hp_attach_matching_devices (hp_global_config_get(), cu_device);
              cu_device[0] = '\0';
            }

            /* Initialize new device with default config */
            memcpy (&dev_config, &df_config, sizeof (dev_config));

            /* Cut off leading blanks of device name */
            dev_name = buf+strspn (buf, " \t\n\r");
            strcpy (cu_device, dev_name);    /* Save the device name */
          }
        }
        if (cu_device[0] != '\0')  /* Did we work on a device ? */
        {
          memcpy (hp_global_config_get (), &dev_config, sizeof (dev_config));
          DBG(1, "hp_read_config: attach %s\n", cu_device);
          hp_attach_matching_devices (hp_global_config_get (), cu_device);
          cu_device[0] = '\0';
        }
      fclose (fp);
      DBG(1, "hp_read_config: reset to default config\n");
      memcpy (hp_global_config_get (), &df_config, sizeof (df_config));
    }
  else
    {
      /* default to /dev/scanner instead of insisting on config file */
      char *dev_name = "/dev/scanner";

      memcpy (hp_global_config_get (), &df_config, sizeof (df_config));
      hp_attach_matching_devices (hp_global_config_get (), dev_name);
    }

  global.config_read++;
  return SANE_STATUS_GOOD;
}

static SANE_Status
hp_update_devlist (void)
{
  HpDeviceList	devp;
  const SANE_Device **devlist;
  int		count	= 0;

  RETURN_IF_FAIL( hp_read_config() );

  if (global.devlist)
      sanei_hp_free(global.devlist);

  for (devp = global.device_list; devp; devp = devp->next)
      count++;

  if (!(devlist = sanei_hp_alloc((count + 1) * sizeof(*devlist))))
      return SANE_STATUS_NO_MEM;

  global.devlist = devlist;

  for (devp = global.device_list; devp; devp = devp->next)
      *devlist++ = sanei_hp_device_sanedevice(devp->dev);
  *devlist = 0;

  return SANE_STATUS_GOOD;
}


/*
 *
 */

SANE_Status
sane_init (SANE_Int *version_code, SANE_Auth_Callback UNUSEDARG authorize)
{SANE_Status status;

  DBG_INIT();
  DBG(3, "sane_init called\n");
  sanei_thread_init ();

  sanei_hp_init_openfd ();
  hp_destroy();

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, VERSIO);

  status = hp_init();
  DBG(3, "sane_init will finish with %s\n", sane_strstatus (status));
  return status;
}

void
sane_exit (void)
{
  DBG(3, "sane_exit called\n");
  hp_destroy();
  DBG(3, "sane_exit will finish\n");
}

SANE_Status
sane_get_devices (const SANE_Device ***device_list,
                  SANE_Bool UNUSEDARG local_only)
{
  DBG(3, "sane_get_devices called\n");

  RETURN_IF_FAIL( hp_update_devlist() );
  *device_list = global.devlist;
  DBG(3, "sane_get_devices will finish with %s\n",
      sane_strstatus (SANE_STATUS_GOOD));
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle *handle)
{
  HpDevice	dev	= 0;
  HpHandle	h;

  DBG(3, "sane_open called\n");

  RETURN_IF_FAIL( hp_read_config() );

  if (devicename[0])
      RETURN_IF_FAIL( hp_get_dev(devicename, &dev) );
  else
    {
      /* empty devicname -> use first device */
      if (global.device_list)
	  dev = global.device_list->dev;
    }
  if (!dev)
      return SANE_STATUS_INVAL;

  if (!(h = sanei_hp_handle_new(dev)))
      return SANE_STATUS_NO_MEM;

  RETURN_IF_FAIL( hp_handle_list_add(&global.handle_list, h) );

  *handle = h;
  DBG(3, "sane_open will finish with %s\n", sane_strstatus (SANE_STATUS_GOOD));
  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  HpHandle	h  = handle;

  DBG(3, "sane_close called\n");

  if (!FAILED( hp_handle_list_remove(&global.handle_list, h) ))
      sanei_hp_handle_destroy(h);

  DBG(3, "sane_close will finish\n");
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int optnum)
{
  HpHandle 	h = handle;
  const SANE_Option_Descriptor *optd;

  DBG(10, "sane_get_option_descriptor called\n");

  optd = sanei_hp_handle_saneoption(h, optnum);

  DBG(10, "sane_get_option_descriptor will finish\n");

  return optd;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int optnum,
		     SANE_Action action, void *valp, SANE_Int *info)
{
  HpHandle h = handle;
  SANE_Status status;

  DBG(10, "sane_control_option called\n");

  status = sanei_hp_handle_control(h, optnum, action, valp, info);

  DBG(10, "sane_control_option will finish with %s\n",
      sane_strstatus (status));
  return status;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters *params)
{
  HpHandle h = handle;
  SANE_Status status;

  DBG(10, "sane_get_parameters called\n");

  status = sanei_hp_handle_getParameters(h, params);

  DBG(10, "sane_get_parameters will finish with %s\n",
      sane_strstatus (status));
  return status;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  HpHandle h = handle;
  SANE_Status status;

  DBG(3, "sane_start called\n");

  status = sanei_hp_handle_startScan(h);

  DBG(3, "sane_start will finish with %s\n", sane_strstatus (status));
  return status;
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte *buf, SANE_Int max_len, SANE_Int *len)
{
  HpHandle	h 	= handle;
  size_t	length	= max_len;
  SANE_Status	status;

  DBG(16, "sane_read called\n");

  status =  sanei_hp_handle_read(h, buf, &length);
  *len = length;

  DBG(16, "sane_read will finish with %s\n", sane_strstatus (status));
  return status;
}

void
sane_cancel (SANE_Handle handle)
{
  HpHandle h = handle;

  DBG(3, "sane_cancel called\n");

  sanei_hp_handle_cancel(h);

  DBG(3, "sane_cancel will finish\n");
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  HpHandle h = handle;
  SANE_Status status;

  DBG(3, "sane_set_io_mode called\n");

  status = sanei_hp_handle_setNonblocking(h, non_blocking);

  DBG(3, "sane_set_io_mode will finish with %s\n",
      sane_strstatus (status));
  return status;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int *fd)
{
  HpHandle h = handle;
  SANE_Status status;

  DBG(10, "sane_get_select_fd called\n");

  status = sanei_hp_handle_getPipefd(h, fd);

  DBG(10, "sane_get_select_fd will finish with %s\n",
      sane_strstatus (status));
  return status;
}
