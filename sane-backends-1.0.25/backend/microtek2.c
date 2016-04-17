/***************************************************************************
 * SANE - Scanner Access Now Easy.

   microtek2.c

   This file (C) 1998, 1999 Bernd Schroeder
   modifications 2000, 2001 Karsten Festag

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

 ***************************************************************************

   This file implements a SANE backend for Microtek scanners with
   SCSI-2 command set.

   (feedback to:  bernd@aquila.muc.de)
   (              karsten.festag@t-online.de)
 ***************************************************************************/


#ifdef _AIX
# include <lalloca.h>   /* MUST come first for AIX! */
#endif

#include "../include/sane/config.h"
#include "../include/lalloca.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <math.h>

#include "../include/_stdint.h"

#ifdef HAVE_AUTHORIZATION
#include <sys/stat.h>
#endif

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_scsi.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_thread.h"

#ifndef TESTBACKEND
#define BACKEND_NAME microtek2
#else
#define BACKEND_NAME microtek2_test
#endif

/* for testing*/
/*#define NO_PHANTOMTYPE_SHADING*/

#include "../include/sane/sanei_backend.h"

#include "microtek2.h"

#ifdef HAVE_AUTHORIZATION
static SANE_Auth_Callback auth_callback;
#endif

static int md_num_devices = 0;          /* number of devices from config file */
static Microtek2_Device *md_first_dev = NULL;        /* list of known devices */
static Microtek2_Scanner *ms_first_handle = NULL;    /* list of open scanners */

/* options that can be configured in the config file */
static Config_Options md_options =
                               { 1.0, "off", "off", "off", "off", "off", "off"};
static Config_Temp *md_config_temp = NULL;
static int md_dump = 0;                 /* from config file: */
                                        /* 1: inquiry + scanner attributes */
                                        /* 2: + all scsi commands and data */
                                        /* 3: + all scan data */
static int md_dump_clear = 1;


/*---------- sane_cancel() ---------------------------------------------------*/

void
sane_cancel (SANE_Handle handle)
{
    Microtek2_Scanner *ms = handle;

    DBG(30, "sane_cancel: handle=%p\n", handle);

    if ( ms->scanning == SANE_TRUE )
        cleanup_scanner(ms);
    ms->cancelled = SANE_TRUE;
    ms->fd[0] = ms->fd[1] = -1;
}


/*---------- sane_close() ----------------------------------------------------*/


void
sane_close (SANE_Handle handle)
{
    Microtek2_Scanner *ms = handle;

    DBG(30, "sane_close: ms=%p\n", (void *) ms);

    if ( ! ms )
        return;

    /* free malloc'ed stuff */
    cleanup_scanner(ms);

    /* remove Scanner from linked list */
    if ( ms_first_handle == ms )
        ms_first_handle = ms->next;
    else
      {
        Microtek2_Scanner *ts = ms_first_handle;
        while ( (ts != NULL) && (ts->next != ms) )
            ts = ts->next;
        ts->next = ts->next->next; /* == ms->next */
      }
    DBG(100, "free ms at %p\n", (void *) ms);
    free((void *) ms);
    ms = NULL;
}


/*---------- sane_exit() -----------------------------------------------------*/

void
sane_exit (void)
{
    Microtek2_Device *next;
    int i;

    DBG(30, "sane_exit:\n");

    /* close all leftover Scanners */
    while (ms_first_handle != NULL)
        sane_close(ms_first_handle);
    /* free up device list */
    while (md_first_dev != NULL)
      {
        next = md_first_dev->next;

        for ( i = 0; i < 4; i++ )
          {
            if ( md_first_dev->custom_gamma_table[i] )
              {
                DBG(100, "free md_first_dev->custom_gamma_table[%d] at %p\n",
                          i, (void *) md_first_dev->custom_gamma_table[i]);
                free((void *) md_first_dev->custom_gamma_table[i]);
                md_first_dev->custom_gamma_table[i] = NULL;
              }
          }

        if ( md_first_dev->shading_table_w )
          {
            DBG(100, "free md_first_dev->shading_table_w at %p\n",
                      md_first_dev->shading_table_w);
            free((void *) md_first_dev->shading_table_w);
            md_first_dev->shading_table_w = NULL;
          }

        if ( md_first_dev->shading_table_d )
          {
            DBG(100, "free md_first_dev->shading_table_d at %p\n",
                      md_first_dev->shading_table_d);
            free((void *) md_first_dev->shading_table_d);
            md_first_dev->shading_table_d = NULL;
          }

        DBG(100, "free md_first_dev at %p\n", (void *) md_first_dev);
        free((void *) md_first_dev);
        md_first_dev = next;
      }
    sane_get_devices(NULL, SANE_FALSE);     /* free list of SANE_Devices */

    DBG(30, "sane_exit: MICROTEK2 says goodbye.\n");
}


/*---------- sane_get_devices()-----------------------------------------------*/

SANE_Status
sane_get_devices(const SANE_Device ***device_list, SANE_Bool local_only)
{
    /* return a list of available devices; available here means that we get */
    /* a positive response to an 'INQUIRY' and possibly to a */
    /* 'READ SCANNER ATTRIBUTE' call */

    static const SANE_Device **sd_list = NULL;
    Microtek2_Device *md;
    SANE_Status status;
    int index;

    DBG(30, "sane_get_devices: local_only=%d\n", local_only);

    /* this is hack to get the list freed with a call from sane_exit() */
    if ( device_list == NULL )
      {
        if ( sd_list )
          {
            DBG(100, "free sd_list at %p\n", (void *) sd_list);
            free(sd_list);
            sd_list=NULL;
          }
        DBG(30, "sane_get_devices: sd_list_freed\n");
        return SANE_STATUS_GOOD;
      }

    /* first free old list, if there is one; frontend wants a new list */
    if ( sd_list )
      {
        DBG(100, "free sd_list at %p\n", (void *) sd_list);
        free(sd_list);                            /* free array of pointers */
      }

    sd_list = (const SANE_Device **)
               malloc( (md_num_devices + 1) * sizeof(SANE_Device **));
    DBG(100, "sane_get_devices: sd_list=%p, malloc'd %lu bytes\n",
	(void *) sd_list, (u_long)  ((md_num_devices + 1) * sizeof(SANE_Device **)));

    if ( ! sd_list )
      {
        DBG(1, "sane_get_devices: malloc() for sd_list failed\n");
        return SANE_STATUS_NO_MEM;
      }

    *device_list = sd_list;
    index = 0;
    md = md_first_dev;
    while ( md )
      {
        status = attach(md);
        if ( status != SANE_STATUS_GOOD )
          {
            DBG(10, "sane_get_devices: attach status '%s'\n",
                     sane_strstatus(status));
            md = md->next;
            continue;
          }

        /* check whether unit is ready, if so add it to the list */
        status = scsi_test_unit_ready(md);
        if ( status != SANE_STATUS_GOOD )
          {
            DBG(10, "sane_get_devices: test_unit_ready status '%s'\n",
                     sane_strstatus(status));
            md = md->next;
            continue;
          }

        sd_list[index] = &md->sane;

        ++index;
        md = md->next;
      }

    sd_list[index] = NULL;
    return SANE_STATUS_GOOD;
}


/*---------- sane_get_parameters() -------------------------------------------*/

SANE_Status
sane_get_parameters(SANE_Handle handle, SANE_Parameters *params)
{
    Microtek2_Scanner *ms = handle;
    Microtek2_Device *md;
    Option_Value *val;
    Microtek2_Info *mi;
    int mode;
    int depth;
    int bits_pp_in;             /* bits per pixel from scanner */
    int bits_pp_out;            /* bits_per_pixel transferred to frontend */
    int bytes_per_line;
    double x_pixel_per_mm;
    double y_pixel_per_mm;
    double x1_pixel;
    double y1_pixel;
    double width_pixel;
    double height_pixel;


    DBG(40, "sane_get_parameters: handle=%p, params=%p\n", handle,
	(void *) params);


    md = ms->dev;
    mi = &md->info[md->scan_source];
    val= ms->val;

    if ( ! ms->scanning )            /* get an estimate for the params */
      {

        get_scan_mode_and_depth(ms, &mode, &depth, &bits_pp_in, &bits_pp_out);

        switch ( mode )
          {
            case MS_MODE_COLOR:
              if ( mi->onepass )
                {
                  ms->params.format = SANE_FRAME_RGB;
                  ms->params.last_frame = SANE_TRUE;
                }
              else
                {
                  ms->params.format = SANE_FRAME_RED;
                  ms->params.last_frame = SANE_FALSE;
                }
              break;
            case MS_MODE_GRAY:
            case MS_MODE_HALFTONE:
            case MS_MODE_LINEART:
            case MS_MODE_LINEARTFAKE:
              ms->params.format = SANE_FRAME_GRAY;
              ms->params.last_frame = SANE_TRUE;
              break;
            default:
              DBG(1, "sane_get_parameters: Unknown scan mode %d\n", mode);
              break;
          }

      ms->params.depth = (SANE_Int) bits_pp_out;

      /* calculate lines, pixels per line and bytes per line */
      if ( val[OPT_RESOLUTION_BIND].w == SANE_TRUE )
        {
          x_pixel_per_mm = y_pixel_per_mm =
                SANE_UNFIX(val[OPT_RESOLUTION].w) / MM_PER_INCH;
          DBG(30, "sane_get_parameters: x_res=y_res=%f\n",
                  SANE_UNFIX(val[OPT_RESOLUTION].w));
        }
      else
        {
          x_pixel_per_mm = SANE_UNFIX(val[OPT_RESOLUTION].w) / MM_PER_INCH;
          y_pixel_per_mm = SANE_UNFIX(val[OPT_Y_RESOLUTION].w) / MM_PER_INCH;
          DBG(30, "sane_get_parameters: x_res=%f, y_res=%f\n",
                  SANE_UNFIX(val[OPT_RESOLUTION].w),
                  SANE_UNFIX(val[OPT_Y_RESOLUTION].w));
        }

      DBG(30, "sane_get_parameters: x_ppm=%f, y_ppm=%f\n",
               x_pixel_per_mm, y_pixel_per_mm);

      y1_pixel = SANE_UNFIX(ms->val[OPT_TL_Y].w) * y_pixel_per_mm;
      height_pixel = fabs(SANE_UNFIX(ms->val[OPT_BR_Y].w) * y_pixel_per_mm
                          - y1_pixel) + 0.5;
      ms->params.lines = (SANE_Int) height_pixel;

      x1_pixel =  SANE_UNFIX(ms->val[OPT_TL_X].w) * x_pixel_per_mm;
      width_pixel = fabs(SANE_UNFIX(ms->val[OPT_BR_X].w) * x_pixel_per_mm
                         - x1_pixel) + 0.5;
      ms->params.pixels_per_line = (SANE_Int) width_pixel;


      if ( bits_pp_out == 1 )
          bytes_per_line =  (width_pixel + 7 ) / 8;
      else
        {
          bytes_per_line = ( width_pixel * bits_pp_out ) / 8 ;
          if ( mode == MS_MODE_COLOR && mi->onepass )
              bytes_per_line *= 3;
        }
      ms->params.bytes_per_line = (SANE_Int) bytes_per_line;
    }  /* if ms->scanning */

  if ( params )
     *params = ms->params;

  DBG(30,"sane_get_parameters: format=%d, last_frame=%d, lines=%d\n",
        ms->params.format,ms->params.last_frame, ms->params.lines);
  DBG(30,"sane_get_parameters: depth=%d, ppl=%d, bpl=%d\n",
        ms->params.depth,ms->params.pixels_per_line, ms->params.bytes_per_line);

  return SANE_STATUS_GOOD;
}


/*---------- sane_get_select_fd() --------------------------------------------*/

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int *fd)
{
    Microtek2_Scanner *ms = handle;


    DBG(30, "sane_get_select_fd: ms=%p\n", (void *) ms);

    if ( ! ms->scanning )
      {
        DBG(1, "sane_get_select_fd: Scanner not scanning\n");
        return SANE_STATUS_INVAL;
      }

    *fd = (SANE_Int) ms->fd[0];
    return SANE_STATUS_GOOD;
}


/*---------- sane_init() -----------------------------------------------------*/

SANE_Status
sane_init(SANE_Int *version_code, SANE_Auth_Callback authorize)
{
    Microtek2_Device *md;
    FILE *fp;
    int match;
    SANE_Auth_Callback trash;


    DBG_INIT();
    DBG(1, "sane_init: Microtek2 (v%d.%d build %s) says hello...\n",
           MICROTEK2_MAJOR, MICROTEK2_MINOR, MICROTEK2_BUILD);

    if ( version_code )
        *version_code = SANE_VERSION_CODE(SANE_CURRENT_MAJOR, V_MINOR, 0);

#ifdef HAVE_AUTHORIZATION
    auth_callback = authorize;
#else
    trash = authorize;     /* prevents compiler warning "unused variable" */
#endif

    sanei_thread_init();

    match = 0;
    fp = sanei_config_open(MICROTEK2_CONFIG_FILE);
    if ( fp == NULL )
        DBG(10, "sane_init: file not opened: '%s'\n", MICROTEK2_CONFIG_FILE);
    else
      {
        /* check config file for devices and associated options */
        parse_config_file(fp, &md_config_temp);

        while ( md_config_temp )
          {
            sanei_config_attach_matching_devices(md_config_temp->device,
                                                 attach_one);
            if ( md_config_temp->next )  /* go to next device, if existent */
                md_config_temp = md_config_temp->next;
            else
                break;
          }

        fclose(fp);
      }

    if ( md_first_dev == NULL )
      {
        /* config file not found or no valid entry; default to /dev/scanner */
        /* instead of insisting on config file */
	add_device_list("/dev/scanner", &md);
        if ( md )
	    attach(md);
      }
    return SANE_STATUS_GOOD;
}


/*---------- sane_open() -----------------------------------------------------*/

SANE_Status
sane_open(SANE_String_Const name, SANE_Handle *handle)
{
    SANE_Status status;
    Microtek2_Scanner *ms;
    Microtek2_Device *md;
#ifdef HAVE_AUTHORIZATION
    struct stat st;
    int rc;
#endif


    DBG(30, "sane_open: device='%s'\n", name);

    *handle = NULL;
    md = md_first_dev;

    if ( name )
      {
        /* add_device_list() returns a pointer to the device struct if */
        /* the device is known or newly added, else it returns NULL */

        status = add_device_list(name, &md);
        if ( status != SANE_STATUS_GOOD )
            return status;
      }

    if ( ! md )
      {
        DBG(10, "sane_open: invalid device name '%s'\n", name);
        return SANE_STATUS_INVAL;
      }

    /* attach calls INQUIRY and READ SCANNER ATTRIBUTES */
    status = attach(md);
    if ( status != SANE_STATUS_GOOD )
        return status;

    ms = malloc(sizeof(Microtek2_Scanner));
    DBG(100, "sane_open: ms=%p, malloc'd %lu bytes\n",
	(void *) ms, (u_long) sizeof(Microtek2_Scanner));
    if ( ms == NULL )
      {
        DBG(1, "sane_open: malloc() for ms failed\n");
        return SANE_STATUS_NO_MEM;
      }

    memset(ms, 0, sizeof(Microtek2_Scanner));
    ms->dev = md;
    ms->scanning = SANE_FALSE;
    ms->cancelled = SANE_FALSE;
    ms->current_pass = 0;
    ms->sfd = -1;
    ms->pid = -1;
    ms->fp = NULL;
    ms->gamma_table = NULL;
    ms->buf.src_buf = ms->buf.src_buffer[0] = ms->buf.src_buffer[1] = NULL;
    ms->control_bytes = NULL;
    ms->shading_image = NULL;
    ms->condensed_shading_w = NULL;
    ms->condensed_shading_d = NULL;
    ms->current_color = MS_COLOR_ALL;
    ms->current_read_color = MS_COLOR_RED;

    init_options(ms, MD_SOURCE_FLATBED);

    /* insert scanner into linked list */
    ms->next = ms_first_handle;
    ms_first_handle = ms;

    *handle = ms;

#ifdef HAVE_AUTHORIZATION
    /* check whether the file with the passwords exists. If it doesnt */
    /* exist, we dont use any authorization */

    rc = stat(PASSWD_FILE, &st);
    if ( rc == -1 && errno == ENOENT )
        return SANE_STATUS_GOOD;
    else
      {
        status = do_authorization(md->name);
        return status;
      }
#else
    return SANE_STATUS_GOOD;
#endif
}


/*---------- sane_read() -----------------------------------------------------*/

SANE_Status
sane_read(SANE_Handle handle, SANE_Byte *buf, SANE_Int maxlen, SANE_Int *len )
{
    Microtek2_Scanner *ms = handle;
    SANE_Status status;
    ssize_t nread;


    DBG(30, "sane_read: handle=%p, buf=%p, maxlen=%d\n", handle, buf, maxlen);

    *len = 0;

    if ( ! ms->scanning || ms->cancelled )
      {
        if ( ms->cancelled )
          {
            status = SANE_STATUS_CANCELLED;
          }
        else
          {
            DBG(15, "sane_read: Scanner %p not scanning\n", (void *) ms);
            status = SANE_STATUS_IO_ERROR;
          }
        DBG(15, "sane_read: scan cancelled or scanner not scanning->cleanup\n");
        cleanup_scanner(ms);
        return status;
      }


    nread = read(ms->fd[0], (void *) buf, (int) maxlen);
    if ( nread == -1 )
      {
        if ( errno == EAGAIN )
          {
            DBG(30, "sane_read: currently no data available\n");
            return SANE_STATUS_GOOD;
          }
        else
          {
            DBG(1, "sane_read: read() failed, errno=%d\n", errno);
            cleanup_scanner(ms);
            return SANE_STATUS_IO_ERROR;
          }
      }

    if ( nread == 0 )
      {
         DBG(15, "sane_read: read 0 bytes -> EOF\n");
         ms->scanning = SANE_FALSE;
         cleanup_scanner(ms);
         return SANE_STATUS_EOF;
      }


    *len = (SANE_Int) nread;
    DBG(30, "sane_read: *len=%d\n", *len);
    return SANE_STATUS_GOOD;
}


/*---------- sane_set_io_mode() ---------------------------------------------*/

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
    Microtek2_Scanner *ms = handle;
    int rc;


    DBG(30, "sane_set_io_mode: handle=%p, nonblocking=%d\n",
             handle, non_blocking);

    if ( ! ms->scanning )
      {
        DBG(1, "sane_set_io_mode: Scanner not scanning\n");
        return SANE_STATUS_INVAL;
      }

    rc = fcntl(ms->fd[0], F_SETFL, non_blocking ? O_NONBLOCK : 0);
    if ( rc == -1 )
      {
        DBG(1, "sane_set_io_mode: fcntl() failed\n");
        return SANE_STATUS_INVAL;
      }

  return SANE_STATUS_GOOD;
}

/*---------- add_device_list() -----------------------------------------------*/

static SANE_Status
add_device_list(SANE_String_Const dev_name, Microtek2_Device **mdev)
{
    Microtek2_Device *md;
    SANE_String hdev;
    size_t len;


    if ( (hdev = strdup(dev_name)) == NULL)
      { 
	DBG(5, "add_device_list: malloc() for hdev failed\n");
        return SANE_STATUS_NO_MEM;
      }

    len = strlen(hdev);
    if ( hdev[len - 1] == '\n' )
        hdev[--len] = '\0';

    DBG(30, "add_device_list: device='%s'\n", hdev);

    /* check, if device is already known */
    md = md_first_dev;
    while ( md )
      {
        if ( strcmp(hdev, md->name) == 0 )
          {
	    DBG(30, "add_device_list: device '%s' already in list\n", hdev);

            *mdev = md;
            return SANE_STATUS_GOOD;
          }
        md = md->next;
    }

    md = (Microtek2_Device *) malloc(sizeof(Microtek2_Device));
    DBG(100, "add_device_list: md=%p, malloc'd %lu bytes\n",
                         (void *) md, (u_long) sizeof(Microtek2_Device));
    if ( md == NULL )
      {
	DBG(1, "add_device_list: malloc() for md failed\n");
        return SANE_STATUS_NO_MEM;
      }

    /* initialize Device and add it at the beginning of the list */
    memset(md, 0, sizeof(Microtek2_Device));
    md->next = md_first_dev;
    md_first_dev = md;
    md->sane.name = NULL;
    md->sane.vendor = NULL;
    md->sane.model = NULL;
    md->sane.type = NULL;
    md->scan_source = MD_SOURCE_FLATBED;
    md->shading_table_w = NULL;
    md->shading_table_d = NULL;
    strncpy(md->name, hdev, PATH_MAX - 1);
    if ( md_config_temp )
        md->opts = md_config_temp->opts;
    else
        md->opts = md_options;
    ++md_num_devices;
    *mdev = md;
    DBG(100, "free hdev at %p\n", hdev);
    free(hdev);

    return SANE_STATUS_GOOD;
}

/*---------- attach() --------------------------------------------------------*/

static SANE_Status
attach(Microtek2_Device *md)
{
    /* This function is called from sane_init() to do the inquiry and to read */
    /* the scanner attributes. If one of these calls fails, or if a new */
    /* device is passed in sane_open() this function may also be called */
    /* from sane_open() or sane_get_devices(). */

    SANE_String model_string; 
    SANE_Status status;
    SANE_Byte source_info;


    DBG(30, "attach: device='%s'\n", md->name);

    status = scsi_inquiry( &md->info[MD_SOURCE_FLATBED], md->name );
    if ( status != SANE_STATUS_GOOD )
      {
	DBG(1, "attach: '%s'\n", sane_strstatus(status));
        return status;
      }

    /* We copy the inquiry info into the info structures for each scansource */
    /* like ADF, TMA, STRIPE and SLIDE */

    for ( source_info = 1; source_info < 5; ++source_info )
        memcpy( &md->info[source_info],
                &md->info[MD_SOURCE_FLATBED],
                sizeof( Microtek2_Info ) );

    /* Here we should insert a function, that stores all the relevant */
    /* information in the info structure in a more conveniant format */
    /* in the device structure, e.g. the model name with a trailing '\0'. */

    status = check_inquiry(md, &model_string);
    if ( status != SANE_STATUS_GOOD )
        return status;

    md->sane.name = md->name;
    md->sane.vendor = "Microtek";
    md->sane.model = strdup(model_string);
    if ( md->sane.model == NULL )
        DBG(1, "attach: strdup for model string failed\n");
    md->sane.type = "flatbed scanner";
    md->revision = strtod(md->info[MD_SOURCE_FLATBED].revision, NULL);

    status = scsi_read_attributes(&md->info[0],
                                  md->name, MD_SOURCE_FLATBED);
    if ( status != SANE_STATUS_GOOD )
      {
	DBG(1, "attach: '%s'\n", sane_strstatus(status));
        return status;
      }

    if ( MI_LUTCAP_NONE( md->info[MD_SOURCE_FLATBED].lut_cap) )
        /* no gamma tables */
        md->model_flags |= MD_NO_GAMMA;

    /* check whether the device supports transparency media adapters */
    if ( md->info[MD_SOURCE_FLATBED].option_device & MI_OPTDEV_TMA )
      {
        status = scsi_read_attributes(&md->info[0],
                                      md->name, MD_SOURCE_TMA);
        if ( status != SANE_STATUS_GOOD )
            return status;
      }

    /* check whether the device supports an ADF */
    if ( md->info[MD_SOURCE_FLATBED].option_device & MI_OPTDEV_ADF )
      {
        status = scsi_read_attributes(&md->info[0],
                                      md->name, MD_SOURCE_ADF);
        if ( status != SANE_STATUS_GOOD )
            return status;
      }

    /* check whether the device supports STRIPES */
    if ( md->info[MD_SOURCE_FLATBED].option_device & MI_OPTDEV_STRIPE )
      {
        status = scsi_read_attributes(&md->info[0],
                                      md->name, MD_SOURCE_STRIPE);
        if ( status != SANE_STATUS_GOOD )
            return status;
      }

    /* check whether the device supports SLIDES */
    if ( md->info[MD_SOURCE_FLATBED].option_device & MI_OPTDEV_SLIDE )
      {
        /* The Phantom 636cx indicates in its attributes that it supports */
        /* slides, but it doesn't. Thus this command would fail. */

        if ( ! (md->model_flags & MD_NO_SLIDE_MODE) )
          {
            status = scsi_read_attributes(&md->info[0],
                                          md->name, MD_SOURCE_SLIDE);
            if ( status != SANE_STATUS_GOOD )
                return status;
          }
      }

    status = scsi_read_system_status(md, -1);
    if ( status != SANE_STATUS_GOOD )
        return status;

    return SANE_STATUS_GOOD;
}


/*---------- attach_one() ----------------------------------------------------*/

static SANE_Status
attach_one (const char *name)
{
    Microtek2_Device *md;
    Microtek2_Device *md_tmp;


    DBG(30, "attach_one: name='%s'\n", name);

    md_tmp = md_first_dev;
    /* if add_device_list() adds an entry it does this at the beginning */
    /* of the list and thus changes md_first_dev */
    add_device_list(name, &md);
    if ( md_tmp != md_first_dev )
        attach(md);

    return SANE_STATUS_GOOD;
}

/*---------- cancel_scan() ---------------------------------------------------*/

static SANE_Status
cancel_scan(Microtek2_Scanner *ms)
{
    SANE_Status status;


    DBG(30, "cancel_scan: ms=%p\n", (void *) ms);

    /* READ IMAGE with a transferlength of 0 aborts a scan */
    ms->transfer_length = 0;
    status = scsi_read_image(ms, (uint8_t *) NULL, 1);
    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "cancel_scan: cancel failed: '%s'\n", sane_strstatus(status));
        status = SANE_STATUS_IO_ERROR;
      }
    else
        status = SANE_STATUS_CANCELLED;

    close(ms->fd[1]);

    /* if we are aborting a scan because, for example, we run out
       of material on a feeder, then pid may be already -1 and
       kill(-1, SIGTERM), i.e. killing all our processes, is not
       likely what we really want - --mj, 2001/Nov/19 */
    if (ms->pid != -1)
      {
       sanei_thread_kill(ms->pid);
       sanei_thread_waitpid(ms->pid, NULL);
      }

    return status;
}


/*---------- check_option() --------------------------------------------------*/

static void
check_option(const char *cp, Config_Options *co)
{
    /* This function analyses options in the config file */

    char *endptr;

    /* When this function is called, it is already made sure that this */
    /* is an option line, i.e. a line that starts with option */

    cp = sanei_config_skip_whitespace(cp);     /* skip blanks */
    cp = sanei_config_skip_whitespace(cp + 6); /* skip "option" */
    if ( strncmp(cp, "dump", 4) == 0 && isspace(cp[4]) )
      {
        cp = sanei_config_skip_whitespace(cp + 4);
        if ( *cp )
          {
            md_dump = (int) strtol(cp, &endptr, 10);
            if ( md_dump > 4 || md_dump < 0 )
              {
                md_dump = 1;
                DBG(30, "check_option: setting dump to %d\n", md_dump);
              }
            cp = sanei_config_skip_whitespace(endptr);
            if ( *cp )
              {
                /* something behind the option value or value wrong */
                md_dump = 1;
                DBG(30, "check_option: option value wrong\n");
              }
          }
        else
          {
            DBG(30, "check_option: missing option value\n");
            /* reasonable fallback */
            md_dump = 1;
          }
      }
    else if ( strncmp(cp, "strip-height", 12) == 0 && isspace(cp[12]) )
      {
        cp = sanei_config_skip_whitespace(cp + 12);
        if ( *cp )
          {
            co->strip_height = strtod(cp, &endptr);
            DBG(30, "check_option: setting strip_height to %f\n",
                     co->strip_height);
            if ( co->strip_height <= 0.0 )
                co->strip_height = 14.0;
            cp = sanei_config_skip_whitespace(endptr);
            if ( *cp )
              {
                /* something behind the option value or value wrong */
                co->strip_height = 14.0;
                DBG(30, "check_option: option value wrong: %f\n",
                         co->strip_height);
              }
          }
      }
    else if ( strncmp(cp, "no-backtrack-option", 19) == 0
              && isspace(cp[19]) )
      {
        cp = sanei_config_skip_whitespace(cp + 19);
        if ( strncmp(cp, "on", 2) == 0 )
          {
            cp = sanei_config_skip_whitespace(cp + 2);
            co->no_backtracking = "on";
          }
        else if ( strncmp(cp, "off", 3) == 0 )
          {
            cp = sanei_config_skip_whitespace(cp + 3);
            co->no_backtracking = "off";
          }
        else
            co->no_backtracking = "off";

        if ( *cp )
          {
            /* something behind the option value or value wrong */
            co->no_backtracking = "off";
            DBG(30, "check_option: option value wrong: %s\n", cp);
          }
      }
    else if ( strncmp(cp, "lightlid-35", 11) == 0
              && isspace(cp[11]) )
      {
        cp = sanei_config_skip_whitespace(cp + 11);
        if ( strncmp(cp, "on", 2) == 0 )
          {
            cp = sanei_config_skip_whitespace(cp + 2);
            co->lightlid35 = "on";
          }
        else if ( strncmp(cp, "off", 3) == 0 )
          {
            cp = sanei_config_skip_whitespace(cp + 3);
            co->lightlid35 = "off";
          }
        else
            co->lightlid35 = "off";

        if ( *cp )
          {
            /* something behind the option value or value wrong */
            co->lightlid35 = "off";
            DBG(30, "check_option: option value wrong: %s\n", cp);
          }
      }
    else if ( strncmp(cp, "toggle-lamp", 11) == 0
              && isspace(cp[11]) )
      {
        cp = sanei_config_skip_whitespace(cp + 11);
        if ( strncmp(cp, "on", 2) == 0 )
          {
            cp = sanei_config_skip_whitespace(cp + 2);
            co->toggle_lamp = "on";
          }
        else if ( strncmp(cp, "off", 3) == 0 )
          {
            cp = sanei_config_skip_whitespace(cp + 3);
            co->toggle_lamp = "off";
          }
        else
            co->toggle_lamp = "off";

        if ( *cp )
          {
            /* something behind the option value or value wrong */
            co->toggle_lamp = "off";
            DBG(30, "check_option: option value wrong: %s\n", cp);
          }
      }
    else if ( strncmp(cp, "lineart-autoadjust", 18) == 0
              && isspace(cp[18]) )
      {
        cp = sanei_config_skip_whitespace(cp + 18);
        if ( strncmp(cp, "on", 2) == 0 )
          {
            cp = sanei_config_skip_whitespace(cp + 2);
            co->auto_adjust = "on";
          }
        else if ( strncmp(cp, "off", 3) == 0 )
          {
            cp = sanei_config_skip_whitespace(cp + 3);
            co->auto_adjust = "off";
          }
        else
            co->auto_adjust = "off";

        if ( *cp )
          {
            /* something behind the option value or value wrong */
            co->auto_adjust = "off";
            DBG(30, "check_option: option value wrong: %s\n", cp);
          }
      }
    else if ( strncmp(cp, "backend-calibration", 19) == 0
              && isspace(cp[19]) )
      {
        cp = sanei_config_skip_whitespace(cp + 19);
        if ( strncmp(cp, "on", 2) == 0 )
          {
            cp = sanei_config_skip_whitespace(cp + 2);
            co->backend_calibration = "on";
          }
        else if ( strncmp(cp, "off", 3) == 0 )
          {
            cp = sanei_config_skip_whitespace(cp + 3);
            co->backend_calibration = "off";
          }
        else
            co->backend_calibration = "off";

        if ( *cp )
          {
            /* something behind the option value or value wrong */
            co->backend_calibration = "off";
            DBG(30, "check_option: option value wrong: %s\n", cp);
          }
      }
    else if ( strncmp(cp, "colorbalance-adjust", 19) == 0
              && isspace(cp[19]) )
      {
        cp = sanei_config_skip_whitespace(cp + 19);
        if ( strncmp(cp, "on", 2) == 0 )
          {
            cp = sanei_config_skip_whitespace(cp + 2);
            co->colorbalance_adjust = "on";
          }
        else if ( strncmp(cp, "off", 3) == 0 )
          {
            cp = sanei_config_skip_whitespace(cp + 3);
            co->colorbalance_adjust = "off";
          }
        else
            co->colorbalance_adjust = "off";

        if ( *cp )
          {
            /* something behind the option value or value wrong */
            co->colorbalance_adjust = "off";
            DBG(30, "check_option: option value wrong: %s\n", cp);
          }
      }
    else
        DBG(30, "check_option: invalid option in '%s'\n", cp);
}


/*---------- check_inquiry() -------------------------------------------------*/

static SANE_Status
check_inquiry(Microtek2_Device *md, SANE_String *model_string)
{
    Microtek2_Info *mi;

    DBG(30, "check_inquiry: md=%p\n", (void *) md);

    md->n_control_bytes = 0;
    md->shading_length = 0;
    md->shading_table_contents = 0;

    mi = &md->info[MD_SOURCE_FLATBED];
    if ( mi->scsi_version != MI_SCSI_II_VERSION )
      {
        DBG(1, "check_inquiry: Device is not a SCSI-II device, but 0x%02x\n",
                mi->scsi_version);
        return SANE_STATUS_IO_ERROR;
      }

    if ( mi->device_type != MI_DEVTYPE_SCANNER )
      {
        DBG(1, "check_inquiry: Device is not a scanner, but 0x%02x\n",
                mi->device_type);
        return SANE_STATUS_IO_ERROR;
      }

    if ( strncasecmp("MICROTEK", mi->vendor, INQ_VENDOR_L) != 0
         && strncmp("        ", mi->vendor, INQ_VENDOR_L) != 0
         && strncmp("AGFA    ", mi->vendor, INQ_VENDOR_L) != 0 )
      {
        DBG(1, "check_inquiry: Device is not a Microtek, but '%.*s'\n",
                INQ_VENDOR_L, mi->vendor);
        return SANE_STATUS_IO_ERROR;
      }

    if ( mi->depth & MI_HASDEPTH_16 )
        md->shading_depth = 16;
    else if ( mi->depth & MI_HASDEPTH_14 )
        md->shading_depth = 14;
    else if ( mi->depth & MI_HASDEPTH_12 )
        md->shading_depth = 12;
    else if ( mi->depth & MI_HASDEPTH_10 )
        md->shading_depth = 10;
    else 
        md->shading_depth = 8;

    switch (mi->model_code)
      {
        case 0x81:
        case 0xab:
          *model_string = "ScanMaker 4";
          break;
        case 0x85:
          *model_string = "ScanMaker V300 / ColorPage-EP";
          /* The ScanMaker V300 (FW < 2.70) returns some values for the */
          /* "read image info" command in only two bytes */
          /* and doesn't understand read_image_status */
          md->model_flags |= MD_NO_RIS_COMMAND;
          if ( md->revision < 2.70 )
              md->model_flags |= MD_RII_TWO_BYTES;
          break;
        case 0x87:
          *model_string = "ScanMaker 5";
          md->model_flags |= MD_NO_GAMMA;
          break;
        case 0x89:
          *model_string = "ScanMaker 6400XL";
          break;
        case 0x8a:
          *model_string = "ScanMaker 9600XL";
          break;
        case 0x8c:
          *model_string = "ScanMaker 630 / ScanMaker V600";
          break;
        case 0x8d:
          *model_string = "ScanMaker 336 / ScanMaker V310";
          break;
        case 0x90:
        case 0x92:
          *model_string = "E3+ / Vobis HighScan";
          break;
        case 0x91:
          *model_string = "ScanMaker X6 / Phantom 636";
          /* The X6 indicates a data format of segregated data in TMA mode */
          /* but actually transfers as chunky data */
          md->model_flags |= MD_DATA_FORMAT_WRONG;
          if ( md->revision == 1.00 )
              md->model_flags |= MD_OFFSET_2;
          break;
        case 0x93:
          *model_string = "ScanMaker 336 / ScanMaker V310";
          break;
        case 0x70:
        case 0x71:
        case 0x94:
        case 0xa0:
          *model_string = "Phantom 330cx / Phantom 336cx / SlimScan C3";
          /* These models do not accept gamma tables. Apparently they */
          /* read the control bits and do not accept shading tables */
          /* They also don't support enhancements (contrast, brightness...)*/
          md->model_flags |= MD_NO_SLIDE_MODE
                          | MD_NO_GAMMA
#ifndef NO_PHANTOMTYPE_SHADING
			  | MD_PHANTOM336CX_TYPE_SHADING
#endif
                          | MD_READ_CONTROL_BIT
                          | MD_NO_ENHANCEMENTS;
          md->opt_backend_calib_default = SANE_TRUE;
          md->opt_no_backtrack_default = SANE_TRUE;
          md->n_control_bytes = 320;
          md->shading_length = 18;
          md->shading_depth = 10;
          md->controlbit_offset = 7;
          break;
        case 0x95:
          *model_string = "ArtixScan 1010";
          break;
        case 0x97:
          *model_string = "ScanMaker 636";
          break;
        case 0x98:
          *model_string = "ScanMaker X6EL";
          if ( md->revision == 1.00 )
              md->model_flags |= MD_OFFSET_2;
          break;
        case 0x99:
          *model_string = "ScanMaker X6USB";
          if ( md->revision == 1.00 )
              md->model_flags |= MD_OFFSET_2;
          md->model_flags |= MD_X6_SHORT_TRANSFER;
          break;
        case 0x9a:
          *model_string = "Phantom 636cx / C6";
          /* The Phantom 636cx says it supports the SLIDE mode, but it */
          /* doesn't. Thus inquring the attributes for slide mode would */
          /* fail. Also it does not accept gamma tables. Apparently */
          /* it reads the control bits and does not accept shading tables */
          md->model_flags |= MD_NO_SLIDE_MODE
                          | MD_READ_CONTROL_BIT
                          | MD_NO_GAMMA
                          | MD_PHANTOM_C6;
          md->opt_backend_calib_default = SANE_TRUE;
          md->opt_no_backtrack_default = SANE_TRUE;
          md->n_control_bytes = 647;
          /* md->shading_length = 18; firmware values seem to work better */
          md->shading_depth = 12;
          md->controlbit_offset = 18;
          break;
        case 0x9d:
          *model_string = "AGFA Duoscan T1200";
          break;
        case 0xa3:
          *model_string = "ScanMaker V6USL";
          /* The V6USL does not accept gamma tables */
          md->model_flags |= MD_NO_GAMMA;
          break;
        case 0xa5:
          *model_string = "ArtixScan 4000t";
          break;
        case 0xac:
          *model_string = "ScanMaker V6UL";
          /* The V6USL does not accept gamma tables, perhaps the V6UL also */
          md->model_flags |= MD_NO_GAMMA;
          break;
        case 0xaf:
          *model_string = "SlimScan C3";
          md->model_flags |= MD_NO_SLIDE_MODE
                          | MD_NO_GAMMA
                          | MD_READ_CONTROL_BIT
                          | MD_NO_ENHANCEMENTS;
          md->opt_backend_calib_default = SANE_TRUE;
          md->opt_no_backtrack_default = SANE_TRUE;
          md->n_control_bytes = 320;
          md->controlbit_offset = 7;
          break;
        case 0xb0:
          *model_string = "ScanMaker X12USL";
          md->opt_backend_calib_default = SANE_TRUE;
          md->model_flags |= MD_16BIT_TRANSFER
                          | MD_CALIB_DIVISOR_600;
          break;
        case 0xb3:
           *model_string = "ScanMaker 3600";
           break;
        case 0xb4:
           *model_string = "ScanMaker 4700";
           break;
        case 0xb6:
          *model_string = "ScanMaker V6UPL";
          /* is like V6USL but with USB and Parport interface ?? */
          md->model_flags |= MD_NO_GAMMA;
          break;
        case 0xb8:
           *model_string = "ScanMaker 3700";
           break;
        case 0xde:
           *model_string = "ScanMaker 9800XL";
           md->model_flags |= MD_NO_GAMMA
			   | MD_16BIT_TRANSFER;
           md->opt_backend_calib_default = SANE_TRUE;
           md->opt_no_backtrack_default = SANE_TRUE;
           break;
        default:
          DBG(1, "check_inquiry: Model 0x%02x not supported\n", mi->model_code);
          return SANE_STATUS_IO_ERROR;
      }

    return SANE_STATUS_GOOD;
}


/*---------- cleanup_scanner() -----------------------------------------------*/

static void
cleanup_scanner(Microtek2_Scanner *ms)
{
    SANE_Status status;
    Microtek2_Device *md;
    md = ms->dev;

    DBG(30, "cleanup_scanner: ms=%p, ms->sfd=%d\n", (void *) ms, ms->sfd);

    if ( ms->scanning == SANE_TRUE )
      status=cancel_scan(ms);

    if ( ms->sfd != -1 )
      sanei_scsi_close(ms->sfd);
    ms->sfd = -1;
    ms->pid = -1;
    ms->fp = NULL;
    ms->current_pass = 0;
    ms->scanning = SANE_FALSE;
    ms->cancelled = SANE_FALSE;

    /* free buffers */
    if ( ms->buf.src_buffer[0] )
      {
        DBG(100, "free ms->buf.src_buffer[0] at %p\n", ms->buf.src_buffer[0]);
        free((void *) ms->buf.src_buffer[0]);
        ms->buf.src_buffer[0] = NULL;
        ms->buf.src_buf = NULL;
      }
    if ( ms->buf.src_buffer[1] )
      {
        DBG(100, "free ms->buf.src_buffer[1] at %p\n", ms->buf.src_buffer[1]);
        free((void *) ms->buf.src_buffer[1]);
        ms->buf.src_buffer[1] = NULL;
        ms->buf.src_buf = NULL;
      }
    if ( ms->buf.src_buf )
      {
        DBG(100, "free ms->buf.src_buf at %p\n", ms->buf.src_buf);
        free((void *) ms->buf.src_buf);
        ms->buf.src_buf = NULL;
      }
    if ( ms->temporary_buffer )
      {
        DBG(100, "free ms->temporary_buffer at %p\n", ms->temporary_buffer);
        free((void *) ms->temporary_buffer);
        ms->temporary_buffer = NULL;
      }
    if ( ms->gamma_table )
      {
        DBG(100, "free ms->gamma_table at %p\n", ms->gamma_table);
        free((void *) ms->gamma_table);
        ms->gamma_table = NULL;
      }
    if ( ms->control_bytes )
      {
        DBG(100, "free ms->control_bytes at %p\n", ms->control_bytes);
        free((void *) ms->control_bytes);
        ms->control_bytes = NULL;
      }
    if ( ms->condensed_shading_w )
      {
        DBG(100, "free ms->condensed_shading_w at %p\n",
                  ms->condensed_shading_w);
        free((void *) ms->condensed_shading_w);
        ms->condensed_shading_w = NULL;
      }
    if ( ms->condensed_shading_d )
      {
        DBG(100, "free ms->condensed_shading_d at %p\n",
                  ms->condensed_shading_d);
        free((void *) ms->condensed_shading_d);
        ms->condensed_shading_d = NULL;
      }

    return;
}

#ifdef HAVE_AUTHORIZATION
/*---------- do_authorization() ----------------------------------------------*/

static SANE_Status
do_authorization(char *ressource)
{
    /* This function implements a simple authorization function. It looks */
    /* up an entry in the file SANE_PATH_CONFIG_DIR/auth. Such an entry */
    /* must be of the form device:user:password where password is a crypt() */
    /* encrypted password. If several users are allowed to access a device */
    /* an entry must be created for each user. If no entry exists for device */
    /* or the file does not exist no authentication is neccessary. If the */
    /* file exists, but cant be opened the authentication fails */

    SANE_Status status;
    FILE *fp;
    int device_found;
    char username[SANE_MAX_USERNAME_LEN];
    char password[SANE_MAX_PASSWORD_LEN];
    char line[MAX_LINE_LEN];
    char *linep;
    char *device;
    char *user;
    char *passwd;
    char *p;


    DBG(30, "do_authorization: ressource=%s\n", ressource);

    if ( auth_callback == NULL )  /* frontend does not require authorization */
        return SANE_STATUS_GOOD;

    /* first check if an entry exists in for this device. If not, we dont */
    /* use authorization */

    fp = fopen(PASSWD_FILE, "r");
    if ( fp == NULL )
      {
        if ( errno == ENOENT )
          {
            DBG(1, "do_authorization: file not found: %s\n", PASSWD_FILE);
            return SANE_STATUS_GOOD;
          }
        else
          {
            DBG(1, "do_authorization: fopen() failed, errno=%d\n", errno);
            return SANE_STATUS_ACCESS_DENIED;
          }
      }
    
    linep = &line[0];
    device_found = 0;
    while ( fgets(line, MAX_LINE_LEN, fp) )
      {
        p = index(linep, SEPARATOR);
        if ( p )
          {
            *p = '\0';
            device = linep;
            if ( strcmp(device, ressource) == 0 )
              {
                DBG(2, "equal\n");
                device_found = 1;
                break; 
              }
          }
      }

    if ( ! device_found )
      {
        fclose(fp);
        return SANE_STATUS_GOOD;
      }

    fseek(fp, 0L, SEEK_SET);
    
    (*auth_callback) (ressource, username, password);
  
    status = SANE_STATUS_ACCESS_DENIED;
    do
      {
        fgets(line, MAX_LINE_LEN, fp);
        if ( ! ferror(fp) && ! feof(fp) )
          {
            /* neither strsep(3) nor strtok(3) seem to work on my system */
            p = index(linep, SEPARATOR);
            if ( p == NULL )
                continue;
            *p = '\0';
            device = linep;
            if ( strcmp( device, ressource) != 0 ) /* not a matching entry */
                continue;

            linep = ++p; 
            p = index(linep, SEPARATOR);
            if ( p == NULL )
                continue;

            *p = '\0';
            user = linep;
            if ( strncmp(user, username, SANE_MAX_USERNAME_LEN) != 0 )
                continue;                  /* username doesnt match */

            linep = ++p;
            /* rest of the line is considered to be the password */
            passwd = linep;
            /* remove newline */
            *(passwd + strlen(passwd) - 1) = '\0';
            p = crypt(password, SALT);
            if ( strcmp(p, passwd) == 0 )
              {
                /* authentication ok */
                status = SANE_STATUS_GOOD;
                break;
              }
            else
                continue;
          }
      } while ( ! ferror(fp) && ! feof(fp) );
    fclose(fp);

    return status;
}
#endif

/*---------- dump_area() -----------------------------------------------------*/

static SANE_Status
dump_area(uint8_t *area, int len, char *info)
{
    /* this function dumps control or information blocks */

#define BPL    16               /* bytes per line to print */

    int i;
    int o;
    int o_limit;
    char outputline[100];
    char *outbuf;

    if ( ! info[0] )
        info = "No additional info available";

    DBG(30, "dump_area: %s\n", info);

    outbuf = outputline;
    o_limit = (len + BPL - 1) / BPL;
    for ( o = 0; o < o_limit; o++)
      {
        sprintf(outbuf, "  %4d: ", o * BPL);
        outbuf += 8;
        for ( i=0; i < BPL && (o * BPL + i ) < len; i++)
          {
            if ( i == BPL / 2 )
              {
                sprintf(outbuf, " ");
                outbuf +=1;
              }
            sprintf(outbuf, "%02x", area[o * BPL + i]);
            outbuf += 2;
          }

        sprintf(outbuf, "%*s",  2 * ( 2 + BPL - i), " " );
        outbuf += (2 * ( 2 + BPL - i));
        sprintf(outbuf, "%s",  (i == BPL / 2) ? " " : "");
        outbuf += ((i == BPL / 2) ? 1 : 0);

        for ( i = 0; i < BPL && (o * BPL + i ) < len; i++)
          {
            if ( i == BPL / 2 )
              {
                sprintf(outbuf, " ");
                outbuf += 1;
              }
            sprintf(outbuf, "%c", isprint(area[o * BPL + i])
                                  ? area[o * BPL + i]
                                  : '.');
            outbuf += 1;
          }
        outbuf = outputline;
        DBG(1, "%s\n", outbuf);
      }

    return SANE_STATUS_GOOD;
}


/*---------- dump_area2() ----------------------------------------------------*/

static SANE_Status
dump_area2(uint8_t *area, int len, char *info)
{

#define BPL    16               /* bytes per line to print */

    int i, linelength;
    char outputline[100];
    char *outbuf;
    linelength = BPL * 3;

    if ( ! info[0] )
        info = "No additional info available";    

    DBG(1, "[%s]\n", info);

    outbuf = outputline;
    for ( i = 0; i < len; i++)
      {
        sprintf(outbuf, "%02x,", *(area + i));
        outbuf += 3;
        if ( ((i+1)%BPL == 0) || (i == len-1) )
           {
             outbuf = outputline;
             DBG(1, "%s\n", outbuf);
           }
      }

    return SANE_STATUS_GOOD;
}

/*---------- dump_to_file() --------------------------------------------------*/
/*---  only for debugging, currently not used -----*/
#if 0
static SANE_Status
dump_to_file(uint8_t *area, int len, char *filename, char *mode)
{
FILE *out;
int i;

    out = fopen(filename, mode);

    for ( i = 0; i < len; i++)
         fputc( *(area + i ), out);

    fclose(out);

    return SANE_STATUS_GOOD;
}
#endif

/*---------- dump_attributes() -----------------------------------------------*/

static SANE_Status
dump_attributes(Microtek2_Info *mi)
{
  /* dump all we know about the scanner */

  int i;

  DBG(30, "dump_attributes: mi=%p\n", (void *) mi);
  DBG(1, "\n");
  DBG(1, "Scanner attributes from device structure\n");
  DBG(1, "========================================\n");
  DBG(1, "Scanner ID...\n");
  DBG(1, "~~~~~~~~~~~~~\n");
  DBG(1, "  Vendor Name%15s: '%s'\n", " ", mi->vendor);
  DBG(1, "  Model Name%16s: '%s'\n", " ", mi->model);
  DBG(1, "  Revision%18s: '%s'\n", " ", mi->revision);
  DBG(1, "  Model Code%16s: 0x%02x\n"," ", mi->model_code);
  switch(mi->model_code)
    {
      case 0x80: DBG(1,  "Redondo 2000XL / ArtixScan 2020\n"); break;
      case 0x81: DBG(1,  "ScanMaker 4 / Aruba\n"); break;
      case 0x82: DBG(1,  "Bali\n"); break;
      case 0x83: DBG(1,  "Washington\n"); break;
      case 0x84: DBG(1,  "Manhattan\n"); break;
      case 0x85: DBG(1,  "ScanMaker V300 / Phantom parallel / TR3\n"); break;
      case 0x86: DBG(1,  "CCP\n"); break;
      case 0x87: DBG(1,  "Scanmaker V\n"); break;
      case 0x88: DBG(1,  "Scanmaker VI\n"); break;
      case 0x89: DBG(1,  "ScanMaker 6400XL / A3-400\n"); break;
      case 0x8a: DBG(1,  "ScanMaker 9600XL / A3-600\n"); break;
      case 0x8b: DBG(1,  "Watt\n"); break;
      case 0x8c: DBG(1,  "ScanMaker V600 / TR6\n"); break;
      case 0x8d: DBG(1,  "ScanMaker V310 / Tr3 10-bit\n"); break;
      case 0x8e: DBG(1,  "CCB\n"); break;
      case 0x8f: DBG(1,  "Sun Rise\n"); break;
      case 0x90: DBG(1,  "ScanMaker E3+ 10-bit\n"); break;
      case 0x91: DBG(1,  "ScanMaker X6 / Phantom 636\n"); break;
      case 0x92: DBG(1,  "ScanMaker E3+ / Vobis Highscan\n"); break;
      case 0x93: DBG(1,  "ScanMaker V310\n"); break;
      case 0x94: DBG(1,  "SlimScan C3 / Phantom 330cx / 336cx\n"); break;
      case 0x95: DBG(1,  "ArtixScan 1010\n"); break;
      case 0x97: DBG(1,  "ScanMaker V636\n"); break;
      case 0x98: DBG(1,  "ScanMaker X6EL\n"); break;
      case 0x99: DBG(1,  "ScanMaker X6 / X6USB\n"); break;
      case 0x9a: DBG(1,  "SlimScan C6 / Phantom 636cx\n"); break;
      case 0x9d: DBG(1,  "AGFA DuoScan T1200\n"); break;
      case 0xa0: DBG(1,  "SlimScan C3 / Phantom 336cx\n"); break;
      case 0xac: DBG(1,  "ScanMaker V6UL\n"); break;
      case 0xa3: DBG(1,  "ScanMaker V6USL\n"); break;
      case 0xaf: DBG(1,  "SlimScan C3 / Phantom 336cx\n"); break;
      case 0xb0: DBG(1,  "ScanMaker X12USL\n"); break;
      case 0xb3: DBG(1,  "ScanMaker 3600\n"); break;
      case 0xb4: DBG(1,  "ScanMaker 4700\n"); break;
      case 0xb6: DBG(1,  "ScanMaker V6UPL\n"); break;
      case 0xb8: DBG(1,  "ScanMaker 3700\n"); break;
      case 0xde: DBG(1,  "ScanMaker 9800XL\n"); break;
      default:   DBG(1,  "Unknown\n"); break;
    }
  DBG(1, "  Device Type Code%10s: 0x%02x (%s),\n", " ",
                  mi->device_type,
		  mi->device_type & MI_DEVTYPE_SCANNER ?
                  "Scanner" : "Unknown type");

  switch (mi->scanner_type)
    {
      case MI_TYPE_FLATBED:
          DBG(1, "  Scanner type%14s:%s", " ", " Flatbed scanner\n");
          break;
      case MI_TYPE_TRANSPARENCY:
          DBG(1, "  Scanner type%14s:%s", " ", " Transparency scanner\n");
          break;
      case MI_TYPE_SHEEDFEED:
          DBG(1, "  Scanner type%14s:%s", " ", " Sheet feed scanner\n");
          break;
      default:
          DBG(1, "  Scanner type%14s:%s", " ", " Unknown\n");
          break;
    }

  DBG(1, "  Supported options%9s: Automatic document feeder: %s\n",
		  " ", mi->option_device & MI_OPTDEV_ADF ? "Yes" : "No");
  DBG(1, "%30sTransparency media adapter: %s\n",
		  " ", mi->option_device & MI_OPTDEV_TMA ? "Yes" : "No");
  DBG(1, "%30sAuto paper detecting: %s\n",
		  " ", mi->option_device & MI_OPTDEV_ADP ? "Yes" : "No");
  DBG(1, "%30sAdvanced picture system: %s\n",
		  " ", mi->option_device & MI_OPTDEV_APS ? "Yes" : "No");
  DBG(1, "%30sStripes: %s\n",
		  " ", mi->option_device & MI_OPTDEV_STRIPE ? "Yes" : "No");
  DBG(1, "%30sSlides: %s\n",
		  " ", mi->option_device & MI_OPTDEV_SLIDE ? "Yes" : "No");
  DBG(1, "  Scan button%15s: %s\n", " ", mi->scnbuttn ? "Yes" : "No");

  DBG(1, "\n");
  DBG(1, "  Imaging Capabilities...\n");
  DBG(1, "  ~~~~~~~~~~~~~~~~~~~~~~~\n");
  DBG(1, "  Color scanner%6s: %s\n", " ", (mi->color) ? "Yes" : "No");
  DBG(1, "  Number passes%6s: %d pass%s\n", " ",
                  (mi->onepass) ? 1 : 3,
                  (mi->onepass) ? "" : "es");
  DBG(1, "  Resolution%9s: X-max: %5d dpi\n%35sY-max: %5d dpi\n",
                  " ", mi->max_xresolution, " ",mi->max_yresolution);
  DBG(1, "  Geometry%11s: Geometric width: %5d pts (%2.2f'')\n", " ",
          mi->geo_width, (float) mi->geo_width / (float) mi->opt_resolution);
  DBG(1, "%23sGeometric height:%5d pts (%2.2f'')\n", " ",
          mi->geo_height, (float) mi->geo_height / (float) mi->opt_resolution);
  DBG(1, "  Optical resolution%1s: %d\n", " ", mi->opt_resolution);

  DBG(1, "  Modes%14s: Lineart:     %s\n%35sHalftone:     %s\n", " ",
		  (mi->scanmode & MI_HASMODE_LINEART) ? " Yes" : " No", " ",
		  (mi->scanmode & MI_HASMODE_HALFTONE) ? "Yes" : "No");

  DBG(1, "%23sGray:     %s\n%35sColor:     %s\n", " ",
		  (mi->scanmode & MI_HASMODE_GRAY) ? "    Yes" : "    No", " ",
		  (mi->scanmode & MI_HASMODE_COLOR) ? "   Yes" : "   No");

  DBG(1, "  Depths%14s: Nibble Gray:  %s\n",
		  " ", (mi->depth & MI_HASDEPTH_NIBBLE) ? "Yes" : "No");
  DBG(1, "%23s10-bit-color: %s\n",
                  " ", (mi->depth & MI_HASDEPTH_10) ? "Yes" : "No");
  DBG(1, "%23s12-bit-color: %s\n", " ",
		  (mi->depth & MI_HASDEPTH_12) ? "Yes" : "No");
  DBG(1, "%23s14-bit-color: %s\n", " ",
		  (mi->depth & MI_HASDEPTH_14) ? "Yes" : "No");
  DBG(1, "%23s16-bit-color: %s\n", " ",
		  (mi->depth & MI_HASDEPTH_16) ? "Yes" : "No");
  DBG(1, "  d/l of HT pattern%2s: %s\n",
                  " ", (mi->has_dnldptrn) ? "Yes" : "No");
  DBG(1, "  Builtin HT pattern%1s: %d\n", " ", mi->grain_slct);

  if ( MI_LUTCAP_NONE(mi->lut_cap) )
      DBG(1, "  LUT capabilities   : None\n");
  if ( mi->lut_cap & MI_LUTCAP_256B )
      DBG(1, "  LUT capabilities   :  256 bytes\n");
  if ( mi->lut_cap & MI_LUTCAP_1024B )
      DBG(1, "  LUT capabilities   : 1024 bytes\n");
  if ( mi->lut_cap & MI_LUTCAP_1024W )
      DBG(1, "  LUT capabilities   : 1024 words\n");
  if ( mi->lut_cap & MI_LUTCAP_4096B )
      DBG(1, "  LUT capabilities   : 4096 bytes\n");
  if ( mi->lut_cap & MI_LUTCAP_4096W )
      DBG(1, "  LUT capabilities   : 4096 words\n");
  if ( mi->lut_cap & MI_LUTCAP_64k_W )
      DBG(1, "  LUT capabilities   :  64k words\n");
  if ( mi->lut_cap & MI_LUTCAP_16k_W )
      DBG(1, "  LUT capabilities   :  16k words\n");
  DBG(1, "\n");
  DBG(1, "  Miscellaneous capabilities...\n");
  DBG(1, "  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
  if ( mi->onepass)
    {
      switch(mi->data_format)
        {
	  case MI_DATAFMT_CHUNKY:
              DBG(1, "  Data format        :%s",
                     " Chunky data, R, G & B in one pixel\n");
              break;
	  case MI_DATAFMT_LPLCONCAT:
              DBG(1, "  Data format        :%s",
                     " Line by line in concatenated sequence,\n");
              DBG(1, "%23swithout color indicator\n", " ");
              break;
	  case MI_DATAFMT_LPLSEGREG:
              DBG(1, "  Data format        :%s",
                     " Line by line in segregated sequence,\n");
              DBG(1, "%23swith color indicator\n", " ");
              break;
	  case MI_DATAFMT_WORDCHUNKY:
              DBG(1, "  Data format        : Word chunky data\n");
              break;
          default:
              DBG(1, "  Data format        : Unknown\n");
          break;
        }
    }
  else
      DBG(1, "No information with 3-pass scanners\n");

  DBG(1, "  Color Sequence%17s: \n", " ");
  for ( i = 0; i < RSA_COLORSEQUENCE_L; i++)
    {
      switch(mi->color_sequence[i])
        {
	  case MI_COLSEQ_RED:   DBG(1,"%34s%s\n", " ","R"); break;
	  case MI_COLSEQ_GREEN: DBG(1,"%34s%s\n", " ","G"); break;
	  case MI_COLSEQ_BLUE:  DBG(1,"%34s%s\n", " ","B"); break;
        }
    }
  if ( mi->new_image_status == SANE_TRUE )
      DBG(1, "  Using new ReadImageStatus format\n");
  else
      DBG(1, "  Using old ReadImageStatus format\n");
  if ( mi->direction & MI_DATSEQ_RTOL )
      DBG(1, "  Scanning direction             : right to left\n");
  else
      DBG(1, "  Scanning direction             : left to right\n");
  DBG(1, "  CCD gap%24s: %d lines\n", " ", mi->ccd_gap);
  DBG(1, "  CCD pixels%21s: %d\n", " ", mi->ccd_pixels);
  DBG(1, "  Calib white stripe location%4s: %d\n",
                  " ",  mi->calib_white);
  DBG(1, "  Max calib space%16s: %d\n", " ", mi->calib_space);
  DBG(1, "  Number of lens%17s: %d\n", " ", mi->nlens);
  DBG(1, "  Max number of windows%10s: %d\n", " ", mi->nwindows);
  DBG(1, "  Shading transfer function%6s: 0x%02x\n", " ",mi->shtrnsferequ);
  DBG(1, "  Red balance%20s: %d\n", " ", mi->balance[0]);
  DBG(1, "  Green balance%18s: %d\n", " ", mi->balance[1]);
  DBG(1, "  Blue balance%19s: %d\n", " " , mi->balance[2]);
  DBG(1, "  Buffer type%20s: %s\n",
                  " ",  mi->buftype ? "Ping-Pong" : "Ring");
  DBG(1, "  FEPROM%25s: %s\n", " ", mi->feprom ? "Yes" : "No");

  md_dump_clear = 0;
  return SANE_STATUS_GOOD;
}

/*---------- max_string_size() -----------------------------------------------*/

static size_t
max_string_size (const SANE_String_Const strings[])
{
  size_t size;
  size_t max_size = 0;
  int i;

  for (i = 0; strings[i]; ++i) {
    size = strlen(strings[i]) + 1; /* +1 because NUL counts as part of string */
    if (size > max_size) max_size = size;
  }
  return max_size;
}

/*---------- parse_config_file() ---------------------------------------------*/

static void
parse_config_file(FILE *fp, Config_Temp **ct)
{
    /* builds a list of device names with associated options from the */
    /* config file for later use, when building the list of devices. */
    /* ct->device = NULL indicates global options (valid for all devices */

    char s[PATH_MAX];
    Config_Options global_opts;
    Config_Temp *hct1;
    Config_Temp *hct2;


    DBG(30, "parse_config_file: fp=%p\n", (void *) fp);

    *ct = hct1 = NULL;

    /* first read global options and store them in global_opts */
    /* initialize global_opts with default values */

    global_opts = md_options;

    while ( sanei_config_read(s, sizeof(s), fp) )
      {
        DBG(100, "parse_config_file: read line: %s\n", s);
        if ( *s == '#' || *s == '\0' )  /* ignore empty lines and comments */
            continue;

        if ( strncmp( sanei_config_skip_whitespace(s), "option ", 7) == 0
          || strncmp( sanei_config_skip_whitespace(s), "option\t", 7) == 0 )
          {
            DBG(100, "parse_config_file: found global option %s\n", s);
            check_option(s, &global_opts);
          }
        else                /* it is considered a new device */
            break;
      }

    if ( ferror(fp) || feof(fp) )
      {
        if ( ferror(fp) )
            DBG(1, "parse_config_file: fread failed: errno=%d\n", errno);

        return;
      }

    while ( ! feof(fp) && ! ferror(fp) )
      {
        if ( *s == '#' || *s == '\0' )  /* ignore empty lines and comments */
          {
            sanei_config_read(s, sizeof(s), fp);
            continue;
          }

        if ( strncmp( sanei_config_skip_whitespace(s), "option ", 7) == 0
          || strncmp( sanei_config_skip_whitespace(s), "option\t", 7) == 0 )
          {
            /* when we enter this loop for the first time we allocate */
            /* memory, because the line surely contains a device name, */
            /* so hct1 is always != NULL at this point */
            DBG(100, "parse_config_file: found device option %s\n", s);
            check_option(s, &hct1->opts);
          }


        else                /* it is considered a new device */
          {
            DBG(100, "parse_config_file: found device %s\n", s);
            hct2 = (Config_Temp *) malloc(sizeof(Config_Temp));
            if ( hct2 == NULL )
              {
                DBG(1, "parse_config_file: malloc() failed\n");
                return;
              }

            if ( *ct == NULL )   /* first element */
                *ct = hct1 = hct2;

            hct1->next = hct2;
            hct1 = hct2;

            hct1->device = strdup(s);
            hct1->opts = global_opts;
            hct1->next = NULL;
          }
        sanei_config_read(s, sizeof(s), fp);
      }
    /* set filepointer to the beginning of the file */
    fseek(fp, 0L, SEEK_SET);
    return;
}


/*---------- signal_handler() ------------------------------------------------*/

static RETSIGTYPE
signal_handler (int signal)
{
  if ( signal == SIGTERM )
    {
      sanei_scsi_req_flush_all ();
      _exit (SANE_STATUS_GOOD);
    }
}

/*---------- init_options() --------------------------------------------------*/

static SANE_Status
init_options(Microtek2_Scanner *ms, uint8_t current_scan_source)
{
    /* This function is called every time, when the scan source changes. */
    /* The option values, that possibly change, are then reinitialized,  */
    /* whereas the option descriptors and option values that never */
    /* change are not */

    SANE_Option_Descriptor *sod;
    SANE_Status status;
    Option_Value *val;
    Microtek2_Device *md;
    Microtek2_Info *mi;
    int tablesize;
    int option_size;
    int max_gamma_value;
    int color;
    int i;
    static int first_call = 1;     /* indicates, whether option */
                                   /* descriptors must be initialized */
       /* cannot be used as after a sane_close the sod's must be initialized */

    DBG(30, "init_options: handle=%p, source=%d\n", (void *) ms,
	current_scan_source);

    sod = ms->sod;
    val = ms->val;
    md = ms->dev;
    mi = &md->info[current_scan_source];

    /* needed for gamma calculation */
    get_lut_size(mi, &md->max_lut_size, &md->lut_entry_size);

    /* calculate new values, where possibly needed */

    /* Scan source */
    if ( val[OPT_SOURCE].s )
        free((void *) val[OPT_SOURCE].s);
    i = 0;
    md->scansource_list[i] = (SANE_String) MD_SOURCESTRING_FLATBED;
    if ( current_scan_source == MD_SOURCE_FLATBED )
        val[OPT_SOURCE].s = (SANE_String) strdup(md->scansource_list[i]);
    if ( md->status.adfcnt )
      {
        md->scansource_list[++i] = (SANE_String) MD_SOURCESTRING_ADF;
        if ( current_scan_source == MD_SOURCE_ADF )
            val[OPT_SOURCE].s = (SANE_String) strdup(md->scansource_list[i]);
      }
    if ( md->status.tmacnt )
      {
        md->scansource_list[++i] = (SANE_String) MD_SOURCESTRING_TMA;
        if ( current_scan_source == MD_SOURCE_TMA )
            val[OPT_SOURCE].s = (SANE_String) strdup(md->scansource_list[i]);
      }
    if ( mi->option_device & MI_OPTDEV_STRIPE )
      {
        md->scansource_list[++i] = (SANE_String) MD_SOURCESTRING_STRIPE;
        if ( current_scan_source == MD_SOURCE_STRIPE )
            val[OPT_SOURCE].s = (SANE_String) strdup(md->scansource_list[i]);
      }

    /* Comment this out as long as I do not know in which bit */
    /* it is indicated, whether a slide adapter is connected */
#if 0
    if ( mi->option_device & MI_OPTDEV_SLIDE )
      {
        md->scansource_list[++i] = (SANE_String) MD_SOURCESTRING_SLIDE;
        if ( current_scan_source == MD_SOURCE_SLIDE )
            val[OPT_SOURCE].s = (SANE_String) strdup(md->scansource_list[i]);
      }
#endif

    md->scansource_list[++i] = NULL;

    /* Scan mode */
    if ( val[OPT_MODE].s )
        free((void *) val[OPT_MODE].s);

    i = 0;
    if ( (mi->scanmode & MI_HASMODE_COLOR) )
      {
	md->scanmode_list[i] = (SANE_String) MD_MODESTRING_COLOR;
        val[OPT_MODE].s = strdup(md->scanmode_list[i]);
        ++i;
      }

    if ( mi->scanmode & MI_HASMODE_GRAY )
      {
	md->scanmode_list[i] = (SANE_String) MD_MODESTRING_GRAY;
        if ( ! (mi->scanmode & MI_HASMODE_COLOR ) )
            val[OPT_MODE].s = strdup(md->scanmode_list[i]);
        ++i;
      }

    if ( mi->scanmode & MI_HASMODE_HALFTONE )
      {
	md->scanmode_list[i] = (SANE_String) MD_MODESTRING_HALFTONE;
        if ( ! (mi->scanmode & MI_HASMODE_COLOR )
            && ! (mi->scanmode & MI_HASMODE_GRAY ) )
            val[OPT_MODE].s = strdup(md->scanmode_list[i]);
        ++i;
      }

    /* Always enable a lineart mode. Some models (X6, FW 1.40) say */
    /* that they have no lineart mode. In this case we will do a grayscale */
    /* scan and convert it to onebit data */
    md->scanmode_list[i] = (SANE_String) MD_MODESTRING_LINEART;
    if ( ! (mi->scanmode & MI_HASMODE_COLOR )
        && ! (mi->scanmode & MI_HASMODE_GRAY )
        && ! (mi->scanmode & MI_HASMODE_HALFTONE ) )
        val[OPT_MODE].s = strdup(md->scanmode_list[i]);
    ++i;
    md->scanmode_list[i] = NULL;

    /* bitdepth */
    i = 0;

#if 0
    if ( mi->depth & MI_HASDEPTH_NIBBLE )
        md->bitdepth_list[++i] = (SANE_Int) MD_DEPTHVAL_4;
#endif

    md->bitdepth_list[++i] = (SANE_Int) MD_DEPTHVAL_8;
    if ( mi->depth & MI_HASDEPTH_10 )
        md->bitdepth_list[++i] = (SANE_Int) MD_DEPTHVAL_10;
    if ( mi->depth & MI_HASDEPTH_12 )
        md->bitdepth_list[++i] = (SANE_Int) MD_DEPTHVAL_12;
    if ( mi->depth & MI_HASDEPTH_14 )
        md->bitdepth_list[++i] = (SANE_Int) MD_DEPTHVAL_14;
    if ( mi->depth & MI_HASDEPTH_16 )
        md->bitdepth_list[++i] = (SANE_Int) MD_DEPTHVAL_16;

    md->bitdepth_list[0] = i;
    if (  md->bitdepth_list[1] == (SANE_Int) MD_DEPTHVAL_8 )
        val[OPT_BITDEPTH].w = md->bitdepth_list[1];
    else
        val[OPT_BITDEPTH].w = md->bitdepth_list[2];

    /* Halftone */
    md->halftone_mode_list[0] = (SANE_String) MD_HALFTONE0;
    md->halftone_mode_list[1] = (SANE_String) MD_HALFTONE1;
    md->halftone_mode_list[2] = (SANE_String) MD_HALFTONE2;
    md->halftone_mode_list[3] = (SANE_String) MD_HALFTONE3;
    md->halftone_mode_list[4] = (SANE_String) MD_HALFTONE4;
    md->halftone_mode_list[5] = (SANE_String) MD_HALFTONE5;
    md->halftone_mode_list[6] = (SANE_String) MD_HALFTONE6;
    md->halftone_mode_list[7] = (SANE_String) MD_HALFTONE7;
    md->halftone_mode_list[8] = (SANE_String) MD_HALFTONE8;
    md->halftone_mode_list[9] = (SANE_String) MD_HALFTONE9;
    md->halftone_mode_list[10] = (SANE_String) MD_HALFTONE10;
    md->halftone_mode_list[11] = (SANE_String) MD_HALFTONE11;
    md->halftone_mode_list[12] = NULL;
    if ( val[OPT_HALFTONE].s )
        free((void *) val[OPT_HALFTONE].s);
    val[OPT_HALFTONE].s = strdup(md->halftone_mode_list[0]);

    /* Resolution */
    md->x_res_range_dpi.min = SANE_FIX(10.0);
    md->x_res_range_dpi.max = SANE_FIX(mi->max_xresolution);
    md->x_res_range_dpi.quant = SANE_FIX(1.0);
    val[OPT_RESOLUTION].w = MIN(MD_RESOLUTION_DEFAULT, md->x_res_range_dpi.max);

    md->y_res_range_dpi.min = SANE_FIX(10.0);
    md->y_res_range_dpi.max = SANE_FIX(mi->max_yresolution);
    md->y_res_range_dpi.quant = SANE_FIX(1.0);
    val[OPT_Y_RESOLUTION].w = val[OPT_RESOLUTION].w; /* bind is default */

    /* Preview mode */
    val[OPT_PREVIEW].w = SANE_FALSE;

    /* Geometry */
    md->x_range_mm.min = SANE_FIX(0.0);
    md->x_range_mm.max = SANE_FIX((double) mi->geo_width
                                  / (double) mi->opt_resolution
                                  * MM_PER_INCH);
    md->x_range_mm.quant = SANE_FIX(0.0);
    md->y_range_mm.min = SANE_FIX(0.0);
    md->y_range_mm.max = SANE_FIX((double) mi->geo_height
                                  / (double) mi->opt_resolution
                                  * MM_PER_INCH);
    md->y_range_mm.quant = SANE_FIX(0.0);
    val[OPT_TL_X].w = SANE_FIX(0.0);
    val[OPT_TL_Y].w = SANE_FIX(0.0);
    val[OPT_BR_X].w = md->x_range_mm.max;
    val[OPT_BR_Y].w = md->y_range_mm.max;

    /* Enhancement group */
    val[OPT_BRIGHTNESS].w = MD_BRIGHTNESS_DEFAULT;
    val[OPT_CONTRAST].w = MD_CONTRAST_DEFAULT;
    val[OPT_THRESHOLD].w = MD_THRESHOLD_DEFAULT;

    /* Gamma */
    /* linear gamma must come first */
    i = 0;
    md->gammamode_list[i++] = (SANE_String) MD_GAMMAMODE_LINEAR;
    md->gammamode_list[i++] = (SANE_String) MD_GAMMAMODE_SCALAR;
    md->gammamode_list[i++] = (SANE_String) MD_GAMMAMODE_CUSTOM;
    if ( val[OPT_GAMMA_MODE].s )
        free((void *) val[OPT_GAMMA_MODE].s);
    val[OPT_GAMMA_MODE].s = strdup(md->gammamode_list[0]);

    md->gammamode_list[i] = NULL;

    /* bind gamma */
    val[OPT_GAMMA_BIND].w = SANE_TRUE;
    val[OPT_GAMMA_SCALAR].w = MD_GAMMA_DEFAULT;
    val[OPT_GAMMA_SCALAR_R].w = MD_GAMMA_DEFAULT;
    val[OPT_GAMMA_SCALAR_G].w = MD_GAMMA_DEFAULT;
    val[OPT_GAMMA_SCALAR_B].w = MD_GAMMA_DEFAULT;

    /* If the device supports gamma tables, we allocate memory according */
    /* to lookup table capabilities, otherwise we allocate 4096 elements */
    /* which is sufficient for a color depth of 12. If the device */
    /* does not support gamma tables, we fill the table according to */
    /* the actual bit depth, i.e. 256 entries with a range of 0..255 */
    /* if the actual bit depth is 8, for example. This will hopefully*/
    /* make no trouble if the bit depth is 1. */
    if ( md->model_flags & MD_NO_GAMMA )
      {
        tablesize = 4096;
        option_size = (int) pow(2.0, (double) val[OPT_BITDEPTH].w );
        max_gamma_value = option_size - 1;
      }
    else
      {
        tablesize = md->max_lut_size;
        option_size = tablesize;
        max_gamma_value = md->max_lut_size - 1;
      }

    for ( color = 0; color < 4; color++ )
      {
        /* index 0 is used if bind gamma == true, index 1 to 3 */
        /* if bind gamma == false */
        if ( md->custom_gamma_table[color] )
            free((void *) md->custom_gamma_table[color]);
        md->custom_gamma_table[color] =
                              (SANE_Int *) malloc(tablesize * sizeof(SANE_Int));
        DBG(100, "init_options: md->custom_gamma_table[%d]=%p, malloc'd %lu bytes\n",
            color, (void *) md->custom_gamma_table[color], (u_long) (tablesize * sizeof(SANE_Int)));
        if ( md->custom_gamma_table[color] == NULL )
          {
            DBG(1, "init_options: malloc for custom gamma table failed\n");
            return SANE_STATUS_NO_MEM;
          }

        for ( i = 0; i < max_gamma_value; i++ )
            md->custom_gamma_table[color][i] = i;
      }

    md->custom_gamma_range.min = 0;
    md->custom_gamma_range.max =  max_gamma_value;
    md->custom_gamma_range.quant = 1;

    sod[OPT_GAMMA_CUSTOM].size = option_size * sizeof (SANE_Int);
    sod[OPT_GAMMA_CUSTOM_R].size = option_size * sizeof (SANE_Int);
    sod[OPT_GAMMA_CUSTOM_G].size = option_size * sizeof (SANE_Int);
    sod[OPT_GAMMA_CUSTOM_B].size = option_size * sizeof (SANE_Int);

    val[OPT_GAMMA_CUSTOM].wa = &md->custom_gamma_table[0][0];
    val[OPT_GAMMA_CUSTOM_R].wa = &md->custom_gamma_table[1][0];
    val[OPT_GAMMA_CUSTOM_G].wa = &md->custom_gamma_table[2][0];
    val[OPT_GAMMA_CUSTOM_B].wa = &md->custom_gamma_table[3][0];

    /* Shadow, midtone, highlight, exposure time */
    md->channel_list[0] = (SANE_String) MD_CHANNEL_MASTER;
    md->channel_list[1] = (SANE_String) MD_CHANNEL_RED;
    md->channel_list[2] = (SANE_String) MD_CHANNEL_GREEN;
    md->channel_list[3] = (SANE_String) MD_CHANNEL_BLUE;
    md->channel_list[4] = NULL;
    if ( val[OPT_CHANNEL].s )
        free((void *) val[OPT_CHANNEL].s);
    val[OPT_CHANNEL].s = strdup(md->channel_list[0]);
    val[OPT_SHADOW].w = MD_SHADOW_DEFAULT;
    val[OPT_SHADOW_R].w = MD_SHADOW_DEFAULT;
    val[OPT_SHADOW_G].w = MD_SHADOW_DEFAULT;
    val[OPT_SHADOW_B].w = MD_SHADOW_DEFAULT;
    val[OPT_MIDTONE].w = MD_MIDTONE_DEFAULT;
    val[OPT_MIDTONE_R].w = MD_MIDTONE_DEFAULT;
    val[OPT_MIDTONE_G].w = MD_MIDTONE_DEFAULT;
    val[OPT_MIDTONE_B].w = MD_MIDTONE_DEFAULT;
    val[OPT_HIGHLIGHT].w = MD_HIGHLIGHT_DEFAULT;
    val[OPT_HIGHLIGHT_R].w = MD_HIGHLIGHT_DEFAULT;
    val[OPT_HIGHLIGHT_G].w = MD_HIGHLIGHT_DEFAULT;
    val[OPT_HIGHLIGHT_B].w = MD_HIGHLIGHT_DEFAULT;
    val[OPT_EXPOSURE].w = MD_EXPOSURE_DEFAULT;
    val[OPT_EXPOSURE_R].w = MD_EXPOSURE_DEFAULT;
    val[OPT_EXPOSURE_G].w = MD_EXPOSURE_DEFAULT;
    val[OPT_EXPOSURE_B].w = MD_EXPOSURE_DEFAULT;

    /* special options */
    val[OPT_RESOLUTION_BIND].w = SANE_TRUE;

    /* enable/disable option for backtracking */
    val[OPT_DISABLE_BACKTRACK].w = md->opt_no_backtrack_default;

    /* enable/disable calibration by backend */
    val[OPT_CALIB_BACKEND].w = md->opt_backend_calib_default;

    /* turn off the lamp during a scan */
    val[OPT_LIGHTLID35].w = SANE_FALSE;

    /* auto adjustment of threshold during a lineart scan */
    val[OPT_AUTOADJUST].w = SANE_FALSE;

    /* color balance (100% means no correction) */
    val[OPT_BALANCE_R].w = SANE_FIX(100);
    val[OPT_BALANCE_G].w = SANE_FIX(100);
    val[OPT_BALANCE_B].w = SANE_FIX(100);

    if ( first_call )
      {
        /* initialize option descriptors and ranges */

        /* Percentage range for brightness, contrast */
        md->percentage_range.min = 0 << SANE_FIXED_SCALE_SHIFT;
        md->percentage_range.max = 200 << SANE_FIXED_SCALE_SHIFT;
        md->percentage_range.quant = 1 << SANE_FIXED_SCALE_SHIFT;

        md->threshold_range.min = 1;
        md->threshold_range.max = 255;
        md->threshold_range.quant = 1;

        md->scalar_gamma_range.min = SANE_FIX(0.1);
        md->scalar_gamma_range.max = SANE_FIX(4.0);
        md->scalar_gamma_range.quant = SANE_FIX(0.1);

        md->shadow_range.min = 0;
        md->shadow_range.max = 253;
        md->shadow_range.quant = 1;

        md->midtone_range.min = 1;
        md->midtone_range.max = 254;
        md->midtone_range.quant = 1;

        md->highlight_range.min = 2;
        md->highlight_range.max = 255;
        md->highlight_range.quant = 1;

        md->exposure_range.min = 0;
        md->exposure_range.max = 510;
        md->exposure_range.quant = 2;

        md->balance_range.min = 0;
        md->balance_range.max = 200 << SANE_FIXED_SCALE_SHIFT;
        md->balance_range.quant = 1 << SANE_FIXED_SCALE_SHIFT;

        /* default for most options */
        for ( i = 0; i < NUM_OPTIONS; i++ )
          {
            sod[i].type = SANE_TYPE_FIXED;
            sod[i].unit = SANE_UNIT_NONE;
            sod[i].size = sizeof(SANE_Fixed);
            sod[i].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
            sod[i].constraint_type = SANE_CONSTRAINT_RANGE;
          }

        sod[OPT_NUM_OPTS].name = SANE_NAME_NUM_OPTIONS;
        sod[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
        sod[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
        sod[OPT_NUM_OPTS].type = SANE_TYPE_INT;
        sod[OPT_NUM_OPTS].size = sizeof (SANE_Int);
        sod[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
        sod[OPT_NUM_OPTS].constraint_type = SANE_CONSTRAINT_NONE;
        val[OPT_NUM_OPTS].w = NUM_OPTIONS;      /* NUM_OPTIONS is no option */
        DBG(255, "sod=%p\n", (void *) sod);
        DBG(255, "OPT_NUM_OPTS=%d\n", OPT_NUM_OPTS);
        DBG(255, "SANE_CAP_SOFT_DETECT=%d\n", SANE_CAP_SOFT_DETECT);
        DBG(255, "OPT_NUM_OPTS.cap=%d\n", sod[0].cap);

        /* The Scan Mode Group */
        sod[OPT_MODE_GROUP].title = M_TITLE_SCANMODEGRP;
        sod[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
        sod[OPT_MODE_GROUP].size = 0;
        sod[OPT_MODE_GROUP].desc = "";
        sod[OPT_MODE_GROUP].cap = 0;
        sod[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

        /* Scan source */
        sod[OPT_SOURCE].name = SANE_NAME_SCAN_SOURCE;
        sod[OPT_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
        sod[OPT_SOURCE].desc = SANE_DESC_SCAN_SOURCE;
        sod[OPT_SOURCE].type = SANE_TYPE_STRING;
        sod[OPT_SOURCE].size = max_string_size(md->scansource_list);
        /* if there is only one scan source, deactivate option */
        if ( md->scansource_list[1] == NULL )
            sod[OPT_SOURCE].cap |= SANE_CAP_INACTIVE;
        sod[OPT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
        sod[OPT_SOURCE].constraint.string_list = md->scansource_list;

        /* Scan mode */
        sod[OPT_MODE].name = SANE_NAME_SCAN_MODE;
        sod[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
        sod[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
        sod[OPT_MODE].type = SANE_TYPE_STRING;
        sod[OPT_MODE].size = max_string_size(md->scanmode_list);
        sod[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
        sod[OPT_MODE].constraint.string_list = md->scanmode_list;

        /* Bit depth */
        sod[OPT_BITDEPTH].name = SANE_NAME_BIT_DEPTH;
        sod[OPT_BITDEPTH].title = SANE_TITLE_BIT_DEPTH;
        sod[OPT_BITDEPTH].desc = SANE_DESC_BIT_DEPTH;
        sod[OPT_BITDEPTH].type = SANE_TYPE_INT;
        sod[OPT_BITDEPTH].unit = SANE_UNIT_BIT;
        sod[OPT_BITDEPTH].size = sizeof(SANE_Int);
        /* if we have only 8 bit color deactivate this option */
        if ( md->bitdepth_list[0] == 1 )
            sod[OPT_BITDEPTH].cap |= SANE_CAP_INACTIVE;
        sod[OPT_BITDEPTH].constraint_type = SANE_CONSTRAINT_WORD_LIST;
        sod[OPT_BITDEPTH].constraint.word_list = md->bitdepth_list;

        /* Halftone */
        sod[OPT_HALFTONE].name = SANE_NAME_HALFTONE;
        sod[OPT_HALFTONE].title = SANE_TITLE_HALFTONE;
        sod[OPT_HALFTONE].desc = SANE_DESC_HALFTONE;
        sod[OPT_HALFTONE].type = SANE_TYPE_STRING;
        sod[OPT_HALFTONE].size = max_string_size(md->halftone_mode_list);
        sod[OPT_HALFTONE].cap  |= SANE_CAP_INACTIVE;
        sod[OPT_HALFTONE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
        sod[OPT_HALFTONE].constraint.string_list = md->halftone_mode_list;

        /* Resolution */
        sod[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
        sod[OPT_RESOLUTION].title = SANE_TITLE_SCAN_X_RESOLUTION;
        sod[OPT_RESOLUTION].desc = SANE_DESC_SCAN_X_RESOLUTION;
        sod[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
        sod[OPT_RESOLUTION].constraint.range = &md->x_res_range_dpi;

        sod[OPT_Y_RESOLUTION].name = SANE_NAME_SCAN_Y_RESOLUTION;
        sod[OPT_Y_RESOLUTION].title = SANE_TITLE_SCAN_Y_RESOLUTION;
        sod[OPT_Y_RESOLUTION].desc = SANE_DESC_SCAN_Y_RESOLUTION;
        sod[OPT_Y_RESOLUTION].unit = SANE_UNIT_DPI;
        sod[OPT_Y_RESOLUTION].cap |= SANE_CAP_INACTIVE;
        sod[OPT_Y_RESOLUTION].constraint.range = &md->y_res_range_dpi;

        /* Preview */
        sod[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
        sod[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
        sod[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
        sod[OPT_PREVIEW].type = SANE_TYPE_BOOL;
        sod[OPT_PREVIEW].size = sizeof(SANE_Bool);
        sod[OPT_PREVIEW].constraint_type = SANE_CONSTRAINT_NONE;

        /* Geometry group, for scan area selection */
        sod[OPT_GEOMETRY_GROUP].title = M_TITLE_GEOMGRP;
        sod[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
        sod[OPT_GEOMETRY_GROUP].size = 0;
        sod[OPT_GEOMETRY_GROUP].desc = "";
        sod[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;
        sod[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

        sod[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
        sod[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
        sod[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
        sod[OPT_TL_X].unit = SANE_UNIT_MM;
        sod[OPT_TL_X].constraint.range = &md->x_range_mm;

        sod[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
        sod[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
        sod[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
        sod[OPT_TL_Y].unit = SANE_UNIT_MM;
        sod[OPT_TL_Y].constraint.range = &md->y_range_mm;

        sod[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
        sod[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
        sod[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
        sod[OPT_BR_X].unit = SANE_UNIT_MM;
        sod[OPT_BR_X].constraint.range = &md->x_range_mm;

        sod[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
        sod[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
        sod[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
        sod[OPT_BR_Y].unit = SANE_UNIT_MM;
        sod[OPT_BR_Y].constraint.range = &md->y_range_mm;

        /* Enhancement group */
        sod[OPT_ENHANCEMENT_GROUP].title = M_TITLE_ENHANCEGRP;
        sod[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
        sod[OPT_ENHANCEMENT_GROUP].desc = "";
        sod[OPT_ENHANCEMENT_GROUP].size = 0;
        sod[OPT_ENHANCEMENT_GROUP].cap = 0;
        sod[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

        sod[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
        sod[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
        sod[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
        sod[OPT_BRIGHTNESS].unit = SANE_UNIT_PERCENT;
        sod[OPT_BRIGHTNESS].constraint.range = &md->percentage_range;

        sod[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
        sod[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
        sod[OPT_CONTRAST].desc = SANE_DESC_CONTRAST;
        sod[OPT_CONTRAST].unit = SANE_UNIT_PERCENT;
        sod[OPT_CONTRAST].constraint.range = &md->percentage_range;

        sod[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
        sod[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
        sod[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
        sod[OPT_THRESHOLD].type = SANE_TYPE_INT;
        sod[OPT_THRESHOLD].size = sizeof(SANE_Int);
        sod[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
        sod[OPT_THRESHOLD].constraint.range = &md->threshold_range;

        /* automatically adjust threshold for a lineart scan */
        sod[OPT_AUTOADJUST].name = M_NAME_AUTOADJUST;
        sod[OPT_AUTOADJUST].title = M_TITLE_AUTOADJUST;
        sod[OPT_AUTOADJUST].desc = M_DESC_AUTOADJUST;
        sod[OPT_AUTOADJUST].type = SANE_TYPE_BOOL;
        sod[OPT_AUTOADJUST].size = sizeof(SANE_Bool);
        sod[OPT_AUTOADJUST].constraint_type = SANE_CONSTRAINT_NONE;
        if ( strncmp(md->opts.auto_adjust, "off", 3) == 0 )
            sod[OPT_AUTOADJUST].cap |= SANE_CAP_INACTIVE;

        /* Gamma */
        sod[OPT_GAMMA_GROUP].title = "Gamma";
        sod[OPT_GAMMA_GROUP].desc = "";
        sod[OPT_GAMMA_GROUP].type = SANE_TYPE_GROUP;
        sod[OPT_GAMMA_GROUP].size = 0;
        sod[OPT_GAMMA_GROUP].cap = 0;
        sod[OPT_GAMMA_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

        sod[OPT_GAMMA_MODE].name = M_NAME_GAMMA_MODE;
        sod[OPT_GAMMA_MODE].title = M_TITLE_GAMMA_MODE;
        sod[OPT_GAMMA_MODE].desc = M_DESC_GAMMA_MODE;
        sod[OPT_GAMMA_MODE].type = SANE_TYPE_STRING;
        sod[OPT_GAMMA_MODE].size = max_string_size(md->gammamode_list);
        sod[OPT_GAMMA_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
        sod[OPT_GAMMA_MODE].constraint.string_list = md->gammamode_list;

        sod[OPT_GAMMA_BIND].name = M_NAME_GAMMA_BIND;
        sod[OPT_GAMMA_BIND].title = M_TITLE_GAMMA_BIND;
        sod[OPT_GAMMA_BIND].desc = M_DESC_GAMMA_BIND;
        sod[OPT_GAMMA_BIND].type = SANE_TYPE_BOOL;
        sod[OPT_GAMMA_BIND].size = sizeof(SANE_Bool);
        sod[OPT_GAMMA_BIND].constraint_type = SANE_CONSTRAINT_NONE;

        /* this is active if gamma_bind == true and gammamode == scalar */
        sod[OPT_GAMMA_SCALAR].name = M_NAME_GAMMA_SCALAR;
        sod[OPT_GAMMA_SCALAR].title = M_TITLE_GAMMA_SCALAR;
        sod[OPT_GAMMA_SCALAR].desc = M_DESC_GAMMA_SCALAR;
        sod[OPT_GAMMA_SCALAR].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_SCALAR].constraint.range = &md->scalar_gamma_range;

        sod[OPT_GAMMA_SCALAR_R].name = M_NAME_GAMMA_SCALAR_R;
        sod[OPT_GAMMA_SCALAR_R].title = M_TITLE_GAMMA_SCALAR_R;
        sod[OPT_GAMMA_SCALAR_R].desc = M_DESC_GAMMA_SCALAR_R;
        sod[OPT_GAMMA_SCALAR_R].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_SCALAR_R].constraint.range = &md->scalar_gamma_range;

        sod[OPT_GAMMA_SCALAR_G].name = M_NAME_GAMMA_SCALAR_G;
        sod[OPT_GAMMA_SCALAR_G].title = M_TITLE_GAMMA_SCALAR_G;
        sod[OPT_GAMMA_SCALAR_G].desc = M_DESC_GAMMA_SCALAR_G;
        sod[OPT_GAMMA_SCALAR_G].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_SCALAR_G].constraint.range = &md->scalar_gamma_range;

        sod[OPT_GAMMA_SCALAR_B].name = M_NAME_GAMMA_SCALAR_B;
        sod[OPT_GAMMA_SCALAR_B].title = M_TITLE_GAMMA_SCALAR_B;
        sod[OPT_GAMMA_SCALAR_B].desc = M_DESC_GAMMA_SCALAR_B;
        sod[OPT_GAMMA_SCALAR_B].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_SCALAR_B].constraint.range = &md->scalar_gamma_range;

        sod[OPT_GAMMA_CUSTOM].name = SANE_NAME_GAMMA_VECTOR;
        sod[OPT_GAMMA_CUSTOM].title = SANE_TITLE_GAMMA_VECTOR;
        sod[OPT_GAMMA_CUSTOM].desc = SANE_DESC_GAMMA_VECTOR;
        sod[OPT_GAMMA_CUSTOM].type = SANE_TYPE_INT;
        sod[OPT_GAMMA_CUSTOM].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM].size = option_size * sizeof (SANE_Int);
        sod[OPT_GAMMA_CUSTOM].constraint.range = &md->custom_gamma_range;

        sod[OPT_GAMMA_CUSTOM_R].name = SANE_NAME_GAMMA_VECTOR_R;
        sod[OPT_GAMMA_CUSTOM_R].title = SANE_TITLE_GAMMA_VECTOR_R;
        sod[OPT_GAMMA_CUSTOM_R].desc = SANE_DESC_GAMMA_VECTOR_R;
        sod[OPT_GAMMA_CUSTOM_R].type = SANE_TYPE_INT;
        sod[OPT_GAMMA_CUSTOM_R].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM_R].size = option_size * sizeof (SANE_Int);
        sod[OPT_GAMMA_CUSTOM_R].constraint.range = &md->custom_gamma_range;

        sod[OPT_GAMMA_CUSTOM_G].name = SANE_NAME_GAMMA_VECTOR_G;
        sod[OPT_GAMMA_CUSTOM_G].title = SANE_TITLE_GAMMA_VECTOR_G;
        sod[OPT_GAMMA_CUSTOM_G].desc = SANE_DESC_GAMMA_VECTOR_G;
        sod[OPT_GAMMA_CUSTOM_G].type = SANE_TYPE_INT;
        sod[OPT_GAMMA_CUSTOM_G].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM_G].size = option_size * sizeof (SANE_Int);
        sod[OPT_GAMMA_CUSTOM_G].constraint.range = &md->custom_gamma_range;

        sod[OPT_GAMMA_CUSTOM_B].name = SANE_NAME_GAMMA_VECTOR_B;
        sod[OPT_GAMMA_CUSTOM_B].title = SANE_TITLE_GAMMA_VECTOR_B;
        sod[OPT_GAMMA_CUSTOM_B].desc = SANE_DESC_GAMMA_VECTOR_B;
        sod[OPT_GAMMA_CUSTOM_B].type = SANE_TYPE_INT;
        sod[OPT_GAMMA_CUSTOM_B].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM_B].size = option_size * sizeof (SANE_Int);
        sod[OPT_GAMMA_CUSTOM_B].constraint.range = &md->custom_gamma_range;

        /* Shadow, midtone, highlight */
        sod[OPT_SMH_GROUP].title = M_TITLE_SMHGRP;
        sod[OPT_SMH_GROUP].desc = "";
        sod[OPT_SMH_GROUP].type = SANE_TYPE_GROUP;
        sod[OPT_SMH_GROUP].size = 0;
        sod[OPT_SMH_GROUP].cap = 0;
        sod[OPT_SMH_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

        sod[OPT_CHANNEL].name = M_NAME_CHANNEL;
        sod[OPT_CHANNEL].title = M_TITLE_CHANNEL;
        sod[OPT_CHANNEL].desc = M_DESC_CHANNEL;
        sod[OPT_CHANNEL].type = SANE_TYPE_STRING;
        sod[OPT_CHANNEL].size = max_string_size(md->channel_list);
        sod[OPT_CHANNEL].constraint_type = SANE_CONSTRAINT_STRING_LIST;
        sod[OPT_CHANNEL].constraint.string_list = md->channel_list;

        sod[OPT_SHADOW].name = SANE_NAME_SHADOW;
        sod[OPT_SHADOW].title = SANE_TITLE_SHADOW;
        sod[OPT_SHADOW].desc = SANE_DESC_SHADOW;
        sod[OPT_SHADOW].type = SANE_TYPE_INT;
        sod[OPT_SHADOW].size = sizeof(SANE_Int);
        sod[OPT_SHADOW].constraint.range = &md->shadow_range;

        sod[OPT_SHADOW_R].name = SANE_NAME_SHADOW_R;
        sod[OPT_SHADOW_R].title = SANE_TITLE_SHADOW_R;
        sod[OPT_SHADOW_R].desc = SANE_DESC_SHADOW_R;
        sod[OPT_SHADOW_R].type = SANE_TYPE_INT;
        sod[OPT_SHADOW_R].size = sizeof(SANE_Int);
        sod[OPT_SHADOW_R].constraint.range = &md->shadow_range;

        sod[OPT_SHADOW_G].name = SANE_NAME_SHADOW_G;
        sod[OPT_SHADOW_G].title = SANE_TITLE_SHADOW_G;
        sod[OPT_SHADOW_G].desc = SANE_DESC_SHADOW_G;
        sod[OPT_SHADOW_G].type = SANE_TYPE_INT;
        sod[OPT_SHADOW_G].size = sizeof(SANE_Int);
        sod[OPT_SHADOW_G].constraint.range = &md->shadow_range;

        sod[OPT_SHADOW_B].name = SANE_NAME_SHADOW_B;
        sod[OPT_SHADOW_B].title = SANE_TITLE_SHADOW_B;
        sod[OPT_SHADOW_B].desc = SANE_DESC_SHADOW_B;
        sod[OPT_SHADOW_B].type = SANE_TYPE_INT;
        sod[OPT_SHADOW_B].size = sizeof(SANE_Int);
        sod[OPT_SHADOW_B].constraint.range = &md->shadow_range;

        sod[OPT_MIDTONE].name = M_NAME_MIDTONE;
        sod[OPT_MIDTONE].title = M_TITLE_MIDTONE;
        sod[OPT_MIDTONE].desc = M_DESC_MIDTONE;
        sod[OPT_MIDTONE].type = SANE_TYPE_INT;
        sod[OPT_MIDTONE].size = sizeof(SANE_Int);
        sod[OPT_MIDTONE].constraint.range = &md->midtone_range;

        sod[OPT_MIDTONE_R].name = M_NAME_MIDTONE_R;
        sod[OPT_MIDTONE_R].title = M_TITLE_MIDTONE_R;
        sod[OPT_MIDTONE_R].desc = M_DESC_MIDTONE_R;
        sod[OPT_MIDTONE_R].type = SANE_TYPE_INT;
        sod[OPT_MIDTONE_R].size = sizeof(SANE_Int);
        sod[OPT_MIDTONE_R].constraint.range = &md->midtone_range;

        sod[OPT_MIDTONE_G].name = M_NAME_MIDTONE_G;
        sod[OPT_MIDTONE_G].title = M_TITLE_MIDTONE_G;
        sod[OPT_MIDTONE_G].desc = M_DESC_MIDTONE_G;
        sod[OPT_MIDTONE_G].type = SANE_TYPE_INT;
        sod[OPT_MIDTONE_G].size = sizeof(SANE_Int);
        sod[OPT_MIDTONE_G].constraint.range = &md->midtone_range;

        sod[OPT_MIDTONE_B].name = M_NAME_MIDTONE_B;
        sod[OPT_MIDTONE_B].title = M_TITLE_MIDTONE_B;
        sod[OPT_MIDTONE_B].desc = M_DESC_MIDTONE_B;
        sod[OPT_MIDTONE_B].type = SANE_TYPE_INT;
        sod[OPT_MIDTONE_B].size = sizeof(SANE_Int);
        sod[OPT_MIDTONE_B].constraint.range = &md->midtone_range;

        sod[OPT_HIGHLIGHT].name = SANE_NAME_HIGHLIGHT;
        sod[OPT_HIGHLIGHT].title = SANE_TITLE_HIGHLIGHT;
        sod[OPT_HIGHLIGHT].desc = SANE_DESC_HIGHLIGHT;
        sod[OPT_HIGHLIGHT].type = SANE_TYPE_INT;
        sod[OPT_HIGHLIGHT].size = sizeof(SANE_Int);
        sod[OPT_HIGHLIGHT].constraint.range = &md->highlight_range;

        sod[OPT_HIGHLIGHT_R].name = SANE_NAME_HIGHLIGHT_R;
        sod[OPT_HIGHLIGHT_R].title = SANE_TITLE_HIGHLIGHT_R;
        sod[OPT_HIGHLIGHT_R].desc = SANE_DESC_HIGHLIGHT_R;
        sod[OPT_HIGHLIGHT_R].type = SANE_TYPE_INT;
        sod[OPT_HIGHLIGHT_R].size = sizeof(SANE_Int);
        sod[OPT_HIGHLIGHT_R].constraint.range = &md->highlight_range;

        sod[OPT_HIGHLIGHT_G].name = SANE_NAME_HIGHLIGHT_G;
        sod[OPT_HIGHLIGHT_G].title = SANE_TITLE_HIGHLIGHT_G;
        sod[OPT_HIGHLIGHT_G].desc = SANE_DESC_HIGHLIGHT_G;
        sod[OPT_HIGHLIGHT_G].type = SANE_TYPE_INT;
        sod[OPT_HIGHLIGHT_G].size = sizeof(SANE_Int);
        sod[OPT_HIGHLIGHT_G].constraint.range = &md->highlight_range;

        sod[OPT_HIGHLIGHT_B].name = SANE_NAME_HIGHLIGHT_B;
        sod[OPT_HIGHLIGHT_B].title = SANE_TITLE_HIGHLIGHT_B;
        sod[OPT_HIGHLIGHT_B].desc = SANE_DESC_HIGHLIGHT_B;
        sod[OPT_HIGHLIGHT_B].type = SANE_TYPE_INT;
        sod[OPT_HIGHLIGHT_B].size = sizeof(SANE_Int);
        sod[OPT_HIGHLIGHT_B].constraint.range = &md->highlight_range;

        sod[OPT_EXPOSURE].name = SANE_NAME_SCAN_EXPOS_TIME;
        sod[OPT_EXPOSURE].title = SANE_TITLE_SCAN_EXPOS_TIME;
        sod[OPT_EXPOSURE].desc = SANE_DESC_SCAN_EXPOS_TIME;
        sod[OPT_EXPOSURE].type = SANE_TYPE_INT;
        sod[OPT_EXPOSURE].unit = SANE_UNIT_PERCENT;
        sod[OPT_EXPOSURE].size = sizeof(SANE_Int);
        sod[OPT_EXPOSURE].constraint.range = &md->exposure_range;

        sod[OPT_EXPOSURE_R].name = SANE_NAME_SCAN_EXPOS_TIME_R;
        sod[OPT_EXPOSURE_R].title = SANE_TITLE_SCAN_EXPOS_TIME_R;
        sod[OPT_EXPOSURE_R].desc = SANE_DESC_SCAN_EXPOS_TIME_R;
        sod[OPT_EXPOSURE_R].type = SANE_TYPE_INT;
        sod[OPT_EXPOSURE_R].unit = SANE_UNIT_PERCENT;
        sod[OPT_EXPOSURE_R].size = sizeof(SANE_Int);
        sod[OPT_EXPOSURE_R].constraint.range = &md->exposure_range;

        sod[OPT_EXPOSURE_G].name = SANE_NAME_SCAN_EXPOS_TIME_G;
        sod[OPT_EXPOSURE_G].title = SANE_TITLE_SCAN_EXPOS_TIME_G;
        sod[OPT_EXPOSURE_G].desc = SANE_DESC_SCAN_EXPOS_TIME_G;
        sod[OPT_EXPOSURE_G].type = SANE_TYPE_INT;
        sod[OPT_EXPOSURE_G].unit = SANE_UNIT_PERCENT;
        sod[OPT_EXPOSURE_G].size = sizeof(SANE_Int);
        sod[OPT_EXPOSURE_G].constraint.range = &md->exposure_range;

        sod[OPT_EXPOSURE_B].name = SANE_NAME_SCAN_EXPOS_TIME_B;
        sod[OPT_EXPOSURE_B].title = SANE_TITLE_SCAN_EXPOS_TIME_B;
        sod[OPT_EXPOSURE_B].desc = SANE_DESC_SCAN_EXPOS_TIME_B;
        sod[OPT_EXPOSURE_B].type = SANE_TYPE_INT;
        sod[OPT_EXPOSURE_B].unit = SANE_UNIT_PERCENT;
        sod[OPT_EXPOSURE_B].size = sizeof(SANE_Int);
        sod[OPT_EXPOSURE_B].constraint.range = &md->exposure_range;

        /* The Special Options Group */
        sod[OPT_SPECIAL].title = M_TITLE_SPECIALGRP;
        sod[OPT_SPECIAL].type = SANE_TYPE_GROUP;
        sod[OPT_SPECIAL].size = 0;
        sod[OPT_SPECIAL].desc = "";
        sod[OPT_SPECIAL].cap = SANE_CAP_ADVANCED;
        sod[OPT_SPECIAL].constraint_type = SANE_CONSTRAINT_NONE;

        sod[OPT_RESOLUTION_BIND].name = SANE_NAME_RESOLUTION_BIND;
        sod[OPT_RESOLUTION_BIND].title = SANE_TITLE_RESOLUTION_BIND;
        sod[OPT_RESOLUTION_BIND].desc = SANE_DESC_RESOLUTION_BIND;
        sod[OPT_RESOLUTION_BIND].type = SANE_TYPE_BOOL;
        sod[OPT_RESOLUTION_BIND].size = sizeof(SANE_Bool);
        sod[OPT_RESOLUTION_BIND].cap |= SANE_CAP_ADVANCED;
        sod[OPT_RESOLUTION_BIND].constraint_type = SANE_CONSTRAINT_NONE;

        /* enable/disable option for backtracking */
        sod[OPT_DISABLE_BACKTRACK].name = M_NAME_NOBACKTRACK;
        sod[OPT_DISABLE_BACKTRACK].title = M_TITLE_NOBACKTRACK;
        sod[OPT_DISABLE_BACKTRACK].desc = M_DESC_NOBACKTRACK;
        sod[OPT_DISABLE_BACKTRACK].type = SANE_TYPE_BOOL;
        sod[OPT_DISABLE_BACKTRACK].size = sizeof(SANE_Bool);
        sod[OPT_DISABLE_BACKTRACK].cap |= SANE_CAP_ADVANCED;
        sod[OPT_DISABLE_BACKTRACK].constraint_type = SANE_CONSTRAINT_NONE;
        if ( strncmp(md->opts.no_backtracking, "off", 3) == 0 )
            sod[OPT_DISABLE_BACKTRACK].cap |= SANE_CAP_INACTIVE;

        /* calibration by driver */
        sod[OPT_CALIB_BACKEND].name = M_NAME_CALIBBACKEND;
        sod[OPT_CALIB_BACKEND].title = M_TITLE_CALIBBACKEND;
        sod[OPT_CALIB_BACKEND].desc = M_DESC_CALIBBACKEND;
        sod[OPT_CALIB_BACKEND].type = SANE_TYPE_BOOL;
        sod[OPT_CALIB_BACKEND].size = sizeof(SANE_Bool);
        sod[OPT_CALIB_BACKEND].cap |= SANE_CAP_ADVANCED;
        sod[OPT_CALIB_BACKEND].constraint_type = SANE_CONSTRAINT_NONE;
        if ( strncmp(md->opts.backend_calibration, "off", 3) == 0 )
            sod[OPT_CALIB_BACKEND].cap |= SANE_CAP_INACTIVE;

        /* turn off the lamp of the flatbed during a scan */
        sod[OPT_LIGHTLID35].name = M_NAME_LIGHTLID35;
        sod[OPT_LIGHTLID35].title = M_TITLE_LIGHTLID35;
        sod[OPT_LIGHTLID35].desc = M_DESC_LIGHTLID35;
        sod[OPT_LIGHTLID35].type = SANE_TYPE_BOOL;
        sod[OPT_LIGHTLID35].size = sizeof(SANE_Bool);
        sod[OPT_LIGHTLID35].cap |= SANE_CAP_ADVANCED;
        sod[OPT_LIGHTLID35].constraint_type = SANE_CONSTRAINT_NONE;
        if ( strncmp(md->opts.lightlid35, "off", 3) == 0 )
            sod[OPT_LIGHTLID35].cap |= SANE_CAP_INACTIVE;

        /* toggle the lamp of the flatbed */
        sod[OPT_TOGGLELAMP].name = M_NAME_TOGGLELAMP;
        sod[OPT_TOGGLELAMP].title = M_TITLE_TOGGLELAMP;
        sod[OPT_TOGGLELAMP].desc = M_DESC_TOGGLELAMP;
        sod[OPT_TOGGLELAMP].type = SANE_TYPE_BUTTON;
        sod[OPT_TOGGLELAMP].size = 0;
        sod[OPT_TOGGLELAMP].cap |= SANE_CAP_ADVANCED;
        sod[OPT_TOGGLELAMP].constraint_type = SANE_CONSTRAINT_NONE;
        if ( strncmp(md->opts.toggle_lamp, "off", 3) == 0 )
            sod[OPT_TOGGLELAMP].cap |= SANE_CAP_INACTIVE;

        /* color balance */
        sod[OPT_COLORBALANCE].title = M_TITLE_COLBALANCEGRP;
        sod[OPT_COLORBALANCE].type = SANE_TYPE_GROUP;
        sod[OPT_COLORBALANCE].size = 0;
        sod[OPT_COLORBALANCE].desc = "";
        sod[OPT_COLORBALANCE].cap = SANE_CAP_ADVANCED;
        sod[OPT_COLORBALANCE].constraint_type = SANE_CONSTRAINT_NONE;

        sod[OPT_BALANCE_R].name = M_NAME_BALANCE_R;
        sod[OPT_BALANCE_R].title = M_TITLE_BALANCE_R;
        sod[OPT_BALANCE_R].desc = M_DESC_BALANCE_R;
        sod[OPT_BALANCE_R].unit = SANE_UNIT_PERCENT;
        sod[OPT_BALANCE_R].cap |= SANE_CAP_ADVANCED;
        sod[OPT_BALANCE_R].constraint.range = &md->balance_range;
        if ( strncmp(md->opts.colorbalance_adjust, "off", 3) == 0 )
             sod[OPT_BALANCE_R].cap |= SANE_CAP_INACTIVE;

        sod[OPT_BALANCE_G].name = M_NAME_BALANCE_G;
        sod[OPT_BALANCE_G].title = M_TITLE_BALANCE_G;
        sod[OPT_BALANCE_G].desc = M_DESC_BALANCE_G;
        sod[OPT_BALANCE_G].unit = SANE_UNIT_PERCENT;
        sod[OPT_BALANCE_G].cap |= SANE_CAP_ADVANCED;
        sod[OPT_BALANCE_G].constraint.range = &md->balance_range;
        if ( strncmp(md->opts.colorbalance_adjust, "off", 3) == 0 )
             sod[OPT_BALANCE_G].cap |= SANE_CAP_INACTIVE;

        sod[OPT_BALANCE_B].name = M_NAME_BALANCE_B;
        sod[OPT_BALANCE_B].title = M_TITLE_BALANCE_B;
        sod[OPT_BALANCE_B].desc = M_DESC_BALANCE_B;
        sod[OPT_BALANCE_B].unit = SANE_UNIT_PERCENT;
        sod[OPT_BALANCE_B].cap |= SANE_CAP_ADVANCED;
        sod[OPT_BALANCE_B].constraint.range = &md->balance_range;
        if ( strncmp(md->opts.colorbalance_adjust, "off", 3) == 0 )
             sod[OPT_BALANCE_B].cap |= SANE_CAP_INACTIVE;

        sod[OPT_BALANCE_FW].name = M_NAME_BALANCE_FW;
        sod[OPT_BALANCE_FW].title = M_TITLE_BALANCE_FW;
        sod[OPT_BALANCE_FW].desc = M_DESC_BALANCE_FW;
        sod[OPT_BALANCE_FW].type = SANE_TYPE_BUTTON;
        sod[OPT_BALANCE_FW].size = 0;
        sod[OPT_BALANCE_FW].cap |= SANE_CAP_ADVANCED;
        sod[OPT_BALANCE_FW].constraint_type = SANE_CONSTRAINT_NONE;
        if ( strncmp(md->opts.colorbalance_adjust, "off", 3) == 0 )
             sod[OPT_BALANCE_FW].cap |= SANE_CAP_INACTIVE;
      }

    status = set_option_dependencies(ms, sod, val);
    if ( status != SANE_STATUS_GOOD )
        return status;

    return SANE_STATUS_GOOD;
}

/*---------- set_option_dependencies() ---------------------------------------*/

static SANE_Status
set_option_dependencies(Microtek2_Scanner *ms, SANE_Option_Descriptor *sod,
                        Option_Value *val)
{

    Microtek2_Device *md;
    md = ms->dev;

    DBG(40, "set_option_dependencies: val=%p, sod=%p, mode=%s\n",
             (void *) val, (void *) sod, val[OPT_MODE].s);

    if ( strcmp(val[OPT_MODE].s, MD_MODESTRING_COLOR) == 0 )
      {
        /* activate brightness,..., deactivate halftone pattern */
        /* and threshold */
        sod[OPT_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_CONTRAST].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_CHANNEL].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_SHADOW].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_MIDTONE].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_HIGHLIGHT].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_EXPOSURE].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_HALFTONE].cap |= SANE_CAP_INACTIVE;
        sod[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
        if ( md->bitdepth_list[0] != 1 )
            sod[OPT_BITDEPTH].cap &= ~SANE_CAP_INACTIVE;
        else
            sod[OPT_BITDEPTH].cap |= SANE_CAP_INACTIVE;
        sod[OPT_AUTOADJUST].cap |= SANE_CAP_INACTIVE;
        if ( ! ( strncmp(md->opts.colorbalance_adjust, "off", 3) == 0 ) )
          {
            sod[OPT_BALANCE_R].cap &= ~SANE_CAP_INACTIVE;
            sod[OPT_BALANCE_G].cap &= ~SANE_CAP_INACTIVE;
            sod[OPT_BALANCE_B].cap &= ~SANE_CAP_INACTIVE;
            sod[OPT_BALANCE_FW].cap &= ~SANE_CAP_INACTIVE;
          }
        /* reset options values that are inactive to their default */
        val[OPT_THRESHOLD].w = MD_THRESHOLD_DEFAULT;
      }

    else if ( strcmp(val[OPT_MODE].s, MD_MODESTRING_GRAY) == 0 )
      {
        sod[OPT_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_CONTRAST].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_CHANNEL].cap |= SANE_CAP_INACTIVE;
        sod[OPT_SHADOW].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_MIDTONE].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_HIGHLIGHT].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_EXPOSURE].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_HALFTONE].cap |= SANE_CAP_INACTIVE;
        sod[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
        if ( md->bitdepth_list[0] != 1 )
            sod[OPT_BITDEPTH].cap &= ~SANE_CAP_INACTIVE;
        else
            sod[OPT_BITDEPTH].cap |= SANE_CAP_INACTIVE;
        sod[OPT_AUTOADJUST].cap |= SANE_CAP_INACTIVE;
        sod[OPT_BALANCE_R].cap |= SANE_CAP_INACTIVE;
        sod[OPT_BALANCE_G].cap |= SANE_CAP_INACTIVE;
        sod[OPT_BALANCE_B].cap |= SANE_CAP_INACTIVE;
        sod[OPT_BALANCE_FW].cap |= SANE_CAP_INACTIVE;

        /* reset options values that are inactive to their default */
        if ( val[OPT_CHANNEL].s )
            free((void *) val[OPT_CHANNEL].s);
        val[OPT_CHANNEL].s = strdup((SANE_String) MD_CHANNEL_MASTER);
      }

    else if ( strcmp(val[OPT_MODE].s, MD_MODESTRING_HALFTONE) == 0 )
      {
        sod[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
        sod[OPT_CONTRAST].cap |= SANE_CAP_INACTIVE;
        sod[OPT_CHANNEL].cap |= SANE_CAP_INACTIVE;
        sod[OPT_SHADOW].cap |= SANE_CAP_INACTIVE;
        sod[OPT_MIDTONE].cap |= SANE_CAP_INACTIVE;
        sod[OPT_HIGHLIGHT].cap |= SANE_CAP_INACTIVE;
        sod[OPT_EXPOSURE].cap |= SANE_CAP_INACTIVE;
        sod[OPT_HALFTONE].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
        sod[OPT_BITDEPTH].cap |= SANE_CAP_INACTIVE;
        sod[OPT_AUTOADJUST].cap |= SANE_CAP_INACTIVE;
        sod[OPT_BALANCE_R].cap |= SANE_CAP_INACTIVE;
        sod[OPT_BALANCE_G].cap |= SANE_CAP_INACTIVE;
        sod[OPT_BALANCE_B].cap |= SANE_CAP_INACTIVE;
        sod[OPT_BALANCE_FW].cap |= SANE_CAP_INACTIVE;

        /* reset options values that are inactive to their default */
        val[OPT_BRIGHTNESS].w = MD_BRIGHTNESS_DEFAULT;
        val[OPT_CONTRAST].w = MD_CONTRAST_DEFAULT;
        if ( val[OPT_CHANNEL].s )
            free((void *) val[OPT_CHANNEL].s);
        val[OPT_CHANNEL].s = strdup((SANE_String) MD_CHANNEL_MASTER);
        val[OPT_SHADOW].w = MD_SHADOW_DEFAULT;
        val[OPT_MIDTONE].w = MD_MIDTONE_DEFAULT;
        val[OPT_HIGHLIGHT].w = MD_HIGHLIGHT_DEFAULT;
        val[OPT_EXPOSURE].w = MD_EXPOSURE_DEFAULT;
        val[OPT_THRESHOLD].w = MD_THRESHOLD_DEFAULT;
      }

    else if ( strcmp(val[OPT_MODE].s, MD_MODESTRING_LINEART) == 0 )
      {
        sod[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
        sod[OPT_CONTRAST].cap |= SANE_CAP_INACTIVE;
        sod[OPT_CHANNEL].cap |= SANE_CAP_INACTIVE;
        sod[OPT_SHADOW].cap |= SANE_CAP_INACTIVE;
        sod[OPT_MIDTONE].cap |= SANE_CAP_INACTIVE;
        sod[OPT_HIGHLIGHT].cap |= SANE_CAP_INACTIVE;
        sod[OPT_EXPOSURE].cap |= SANE_CAP_INACTIVE;
        sod[OPT_HALFTONE].cap |= SANE_CAP_INACTIVE;
        if ( val[OPT_AUTOADJUST].w == SANE_FALSE )
            sod[OPT_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;
        else
            sod[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
        sod[OPT_BITDEPTH].cap |= SANE_CAP_INACTIVE;
        sod[OPT_AUTOADJUST].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_BALANCE_R].cap |= SANE_CAP_INACTIVE;
        sod[OPT_BALANCE_G].cap |= SANE_CAP_INACTIVE;
        sod[OPT_BALANCE_B].cap |= SANE_CAP_INACTIVE;
        sod[OPT_BALANCE_FW].cap |= SANE_CAP_INACTIVE;

        /* reset options values that are inactive to their default */
        val[OPT_BRIGHTNESS].w = MD_BRIGHTNESS_DEFAULT;
        val[OPT_CONTRAST].w = MD_CONTRAST_DEFAULT;
        if ( val[OPT_CHANNEL].s )
            free((void *) val[OPT_CHANNEL].s);
        val[OPT_CHANNEL].s = strdup((SANE_String) MD_CHANNEL_MASTER);
        val[OPT_SHADOW].w = MD_SHADOW_DEFAULT;
        val[OPT_MIDTONE].w = MD_MIDTONE_DEFAULT;
        val[OPT_HIGHLIGHT].w = MD_HIGHLIGHT_DEFAULT;
        val[OPT_EXPOSURE].w = MD_EXPOSURE_DEFAULT;
      }

    else
      {
        DBG(1, "set_option_dependencies: unknown mode '%s'\n",
                val[OPT_MODE].s );
        return SANE_STATUS_INVAL;
      }

    /* these ones are always inactive if the mode changes */
    sod[OPT_SHADOW_R].cap |= SANE_CAP_INACTIVE;
    sod[OPT_SHADOW_G].cap |= SANE_CAP_INACTIVE;
    sod[OPT_SHADOW_B].cap |= SANE_CAP_INACTIVE;
    sod[OPT_MIDTONE_R].cap |= SANE_CAP_INACTIVE;
    sod[OPT_MIDTONE_G].cap |= SANE_CAP_INACTIVE;
    sod[OPT_MIDTONE_B].cap |= SANE_CAP_INACTIVE;
    sod[OPT_HIGHLIGHT_R].cap |= SANE_CAP_INACTIVE;
    sod[OPT_HIGHLIGHT_G].cap |= SANE_CAP_INACTIVE;
    sod[OPT_HIGHLIGHT_B].cap |= SANE_CAP_INACTIVE;
    sod[OPT_EXPOSURE_R].cap |= SANE_CAP_INACTIVE;
    sod[OPT_EXPOSURE_G].cap |= SANE_CAP_INACTIVE;
    sod[OPT_EXPOSURE_B].cap |= SANE_CAP_INACTIVE;

    /* reset options values that are inactive to their default */
    val[OPT_SHADOW_R].w = val[OPT_SHADOW_G].w = val[OPT_SHADOW_B].w
            = MD_SHADOW_DEFAULT;
    val[OPT_MIDTONE_R].w = val[OPT_MIDTONE_G].w = val[OPT_MIDTONE_B].w
            = MD_MIDTONE_DEFAULT;
    val[OPT_HIGHLIGHT_R].w = val[OPT_HIGHLIGHT_G].w = val[OPT_HIGHLIGHT_B].w
            = MD_HIGHLIGHT_DEFAULT;
    val[OPT_EXPOSURE_R].w = val[OPT_EXPOSURE_G].w = val[OPT_EXPOSURE_B].w
            = MD_EXPOSURE_DEFAULT;

    if ( SANE_OPTION_IS_SETTABLE(sod[OPT_GAMMA_MODE].cap) )
      {
        restore_gamma_options(sod, val);
      }

    return SANE_STATUS_GOOD;
}

/*---------- sane_control_option() -------------------------------------------*/

SANE_Status
sane_control_option(SANE_Handle handle, SANE_Int option,
                    SANE_Action action, void *value, SANE_Int *info)
{
    Microtek2_Scanner *ms = handle;
    Microtek2_Device *md;
    Microtek2_Info *mi;
    Option_Value *val;
    SANE_Option_Descriptor *sod;
    SANE_Status status;

    md = ms->dev;
    val = &ms->val[0];
    sod = &ms->sod[0];
    mi = &md->info[md->scan_source];

    if ( ms->scanning )
        return SANE_STATUS_DEVICE_BUSY;

    if ( option < 0 || option >= NUM_OPTIONS )
      {
        DBG(100, "sane_control_option: option %d; action %d \n", option, action);
        DBG(10, "sane_control_option: option %d invalid\n", option);
        return SANE_STATUS_INVAL;
      }

    if ( ! SANE_OPTION_IS_ACTIVE(ms->sod[option].cap) )
      {
        DBG(100, "sane_control_option: option %d; action %d \n", option, action);
        DBG(10, "sane_control_option: option %d not active\n", option);
        return SANE_STATUS_INVAL;
      }

    if ( info )
        *info = 0;

    switch ( action )
      {
        case SANE_ACTION_GET_VALUE:   /* read out option values */
          switch ( option )
            {
              /* word options */
              case OPT_BITDEPTH:
              case OPT_RESOLUTION:
              case OPT_Y_RESOLUTION:
              case OPT_THRESHOLD:
              case OPT_TL_X:
              case OPT_TL_Y:
              case OPT_BR_X:
              case OPT_BR_Y:
              case OPT_PREVIEW:
              case OPT_BRIGHTNESS:
              case OPT_CONTRAST:
              case OPT_SHADOW:
              case OPT_SHADOW_R:
              case OPT_SHADOW_G:
              case OPT_SHADOW_B:
              case OPT_MIDTONE:
              case OPT_MIDTONE_R:
              case OPT_MIDTONE_G:
              case OPT_MIDTONE_B:
              case OPT_HIGHLIGHT:
              case OPT_HIGHLIGHT_R:
              case OPT_HIGHLIGHT_G:
              case OPT_HIGHLIGHT_B:
              case OPT_EXPOSURE:
              case OPT_EXPOSURE_R:
              case OPT_EXPOSURE_G:
              case OPT_EXPOSURE_B:
              case OPT_GAMMA_SCALAR:
              case OPT_GAMMA_SCALAR_R:
              case OPT_GAMMA_SCALAR_G:
              case OPT_GAMMA_SCALAR_B:
              case OPT_BALANCE_R:
              case OPT_BALANCE_G:
              case OPT_BALANCE_B:

                *(SANE_Word *) value = val[option].w;

                if (sod[option].type == SANE_TYPE_FIXED )
                    DBG(50, "sane_control_option: opt=%d, act=%d, val=%f\n",
                             option, action, SANE_UNFIX(val[option].w));
                else
                    DBG(50, "sane_control_option: opt=%d, act=%d, val=%d\n",
                             option, action, val[option].w);

                return SANE_STATUS_GOOD;

              /* boolean options */
              case OPT_RESOLUTION_BIND:
              case OPT_DISABLE_BACKTRACK:
              case OPT_CALIB_BACKEND:
              case OPT_LIGHTLID35:
              case OPT_GAMMA_BIND:
              case OPT_AUTOADJUST:
                *(SANE_Bool *) value = val[option].w;
                DBG(50, "sane_control_option: opt=%d, act=%d, val=%d\n",
                         option, action, val[option].w);
                return SANE_STATUS_GOOD;

              /* string options */
              case OPT_SOURCE:
              case OPT_MODE:
              case OPT_HALFTONE:
              case OPT_CHANNEL:
              case OPT_GAMMA_MODE:
                strcpy(value, val[option].s);
                DBG(50, "sane_control_option: opt=%d, act=%d, val=%s\n",
                         option, action, val[option].s);
                return SANE_STATUS_GOOD;

              /* word array options */
              case OPT_GAMMA_CUSTOM:
              case OPT_GAMMA_CUSTOM_R:
              case OPT_GAMMA_CUSTOM_G:
              case OPT_GAMMA_CUSTOM_B:
                memcpy(value, val[option].wa, sod[option].size);
                return SANE_STATUS_GOOD;

              /* button options */
              case OPT_TOGGLELAMP:
              case OPT_BALANCE_FW:
                return SANE_STATUS_GOOD;

              /* others */
              case OPT_NUM_OPTS:
                *(SANE_Word *) value = NUM_OPTIONS;
                return SANE_STATUS_GOOD;

              default:
                return SANE_STATUS_UNSUPPORTED;
            }
          /* NOTREACHED */
          /* break; */

        case SANE_ACTION_SET_VALUE:     /* set option values */
          if ( ! SANE_OPTION_IS_SETTABLE(sod[option].cap) )
            {
              DBG(100, "sane_control_option: option %d; action %d \n",
                        option, action);
              DBG(10, "sane_control_option: trying to set unsettable option\n");
              return SANE_STATUS_INVAL;
            }

          /* do not check OPT_BR_Y, xscanimage sometimes tries to set */
          /* it to a too large value; bug in xscanimage ? */
          /* if ( option != OPT_BR_Y )
            { */
              status = sanei_constrain_value(ms->sod + option, value, info);
              if (status != SANE_STATUS_GOOD)
                {
                  DBG(10, "sane_control_option: invalid option value\n");
                  return status;
                }
          /*  } */

          switch ( sod[option].type )
            {
              case SANE_TYPE_BOOL:
                DBG(50, "sane_control_option: option=%d, action=%d, value=%d\n",
                         option, action, *(SANE_Int *) value);
                if ( ! ( ( *(SANE_Bool *) value == SANE_TRUE )
                         || ( *(SANE_Bool *) value == SANE_FALSE ) ) )
                    {
                      DBG(10, "sane_control_option: invalid BOOL option value\n");
                      return SANE_STATUS_INVAL;
                    }
                if ( val[option].w == *(SANE_Bool *) value ) /* no change */
                    return SANE_STATUS_GOOD;
                val[option].w = *(SANE_Bool *) value;
                break;

              case SANE_TYPE_INT:
                if ( sod[option].size == sizeof(SANE_Int) )
                  {
                    /* word option */
                    DBG(50, "sane_control_option: option=%d, action=%d, "
                            "value=%d\n", option, action, *(SANE_Int *) value);
                    if ( val[option].w == *(SANE_Int *) value ) /* no change */
                        return SANE_STATUS_GOOD;
                    val[option].w = *(SANE_Int *) value;
                  }
                else
                  {
                    /* word array option */
                    memcpy(val[option].wa, value, sod[option].size);
                  }
                break;

              case SANE_TYPE_FIXED:
                DBG(50, "sane_control_option: option=%d, action=%d, value=%f\n",
                         option, action, SANE_UNFIX( *(SANE_Fixed *) value));
                if ( val[option].w == *(SANE_Fixed *) value ) /* no change */
                    return SANE_STATUS_GOOD;
                val[option].w = *(SANE_Fixed *) value;
                break;

              case SANE_TYPE_STRING:
                DBG(50, "sane_control_option: option=%d, action=%d, value=%s\n",
                         option, action, (SANE_String) value);
                if ( strcmp(val[option].s, (SANE_String) value) == 0 )
                    return SANE_STATUS_GOOD;         /* no change */
                if ( val[option].s )
                    free((void *) val[option].s);
                val[option].s = strdup(value);
                if ( val[option].s == NULL )
                  {
                    DBG(1, "sane_control_option: strdup failed\n");
                    return SANE_STATUS_NO_MEM;
                  }
                break;

              case SANE_TYPE_BUTTON:
                break;

              default:
                DBG(1, "sane_control_option: unknown type %d\n",
                        sod[option].type);
                break;
            }

          switch ( option )
            {
              case OPT_RESOLUTION:
              case OPT_Y_RESOLUTION:
              case OPT_TL_X:
              case OPT_TL_Y:
              case OPT_BR_X:
              case OPT_BR_Y:
                if ( info )
                    *info |= SANE_INFO_RELOAD_PARAMS;
                    return SANE_STATUS_GOOD;
              case OPT_DISABLE_BACKTRACK:
              case OPT_CALIB_BACKEND:
              case OPT_LIGHTLID35:
              case OPT_PREVIEW:
              case OPT_BRIGHTNESS:
              case OPT_THRESHOLD:
              case OPT_CONTRAST:
              case OPT_EXPOSURE:
              case OPT_EXPOSURE_R:
              case OPT_EXPOSURE_G:
              case OPT_EXPOSURE_B:
              case OPT_GAMMA_SCALAR:
              case OPT_GAMMA_SCALAR_R:
              case OPT_GAMMA_SCALAR_G:
              case OPT_GAMMA_SCALAR_B:
              case OPT_GAMMA_CUSTOM:
              case OPT_GAMMA_CUSTOM_R:
              case OPT_GAMMA_CUSTOM_G:
              case OPT_GAMMA_CUSTOM_B:
              case OPT_HALFTONE:
              case OPT_BALANCE_R:
              case OPT_BALANCE_G:
              case OPT_BALANCE_B:
               return SANE_STATUS_GOOD;

              case OPT_BITDEPTH:
                /* If the bitdepth has changed we must change the size of */
                /* the gamma table if the device does not support gamma */
                /* tables. This will hopefully cause no trouble if the */
                /* mode is one bit */

                if ( md->model_flags & MD_NO_GAMMA )
                  {
                    int max_gamma_value;
                    int size;
                    int color;
                    int i;

                    size = (int) pow(2.0, (double) val[OPT_BITDEPTH].w) - 1;
                    max_gamma_value = size - 1;
                    for ( color = 0; color < 4; color++ )
                      {
                        for ( i = 0; i < max_gamma_value; i++ )
                            md->custom_gamma_table[color][i] = (SANE_Int) i;
                      }
                    md->custom_gamma_range.max = (SANE_Int) max_gamma_value;
                    sod[OPT_GAMMA_CUSTOM].size = size * sizeof (SANE_Int);
                    sod[OPT_GAMMA_CUSTOM_R].size = size * sizeof (SANE_Int);
                    sod[OPT_GAMMA_CUSTOM_G].size = size * sizeof (SANE_Int);
                    sod[OPT_GAMMA_CUSTOM_B].size = size * sizeof (SANE_Int);

                    if ( info )
                        *info |= SANE_INFO_RELOAD_OPTIONS;

                  }

                if ( info )
                    *info |= SANE_INFO_RELOAD_PARAMS;
                    return SANE_STATUS_GOOD;

              case OPT_SOURCE:
                if ( info )
                    *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
                if ( strcmp(val[option].s, MD_SOURCESTRING_FLATBED) == 0 )
                    md->scan_source = MD_SOURCE_FLATBED;
                else if ( strcmp(val[option].s, MD_SOURCESTRING_TMA) == 0 )
                    md->scan_source = MD_SOURCE_TMA;
                else if ( strcmp(val[option].s, MD_SOURCESTRING_ADF) == 0 )
                    md->scan_source = MD_SOURCE_ADF;
                else if ( strcmp(val[option].s, MD_SOURCESTRING_STRIPE) == 0 )
                    md->scan_source = MD_SOURCE_STRIPE;
                else if ( strcmp(val[option].s, MD_SOURCESTRING_SLIDE) == 0 )
                    md->scan_source = MD_SOURCE_SLIDE;
                else
                  {
                    DBG(1, "sane_control_option: unsupported option %s\n",
                            val[option].s);
                    return SANE_STATUS_UNSUPPORTED;
                  }

                init_options(ms, md->scan_source);
                return SANE_STATUS_GOOD;

              case OPT_MODE:
                if ( info )
                    *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;

                status = set_option_dependencies(ms, sod, val);

                /* Options with side effects need special treatment. They are */
                /* reset, even if they were set by set_option_dependencies(): */
                /* if we have more than one color depth activate this option */

                if ( md->bitdepth_list[0] == 1 )
                    sod[OPT_BITDEPTH].cap |= SANE_CAP_INACTIVE;
                if ( strncmp(md->opts.auto_adjust, "off", 3) == 0 )
                    sod[OPT_AUTOADJUST].cap |= SANE_CAP_INACTIVE;

                if ( status != SANE_STATUS_GOOD )
                    return status;
                return SANE_STATUS_GOOD;

              case OPT_CHANNEL:
                if ( info )
                    *info |= SANE_INFO_RELOAD_OPTIONS;
                if ( strcmp(val[option].s, MD_CHANNEL_MASTER) == 0 )
                  {
                    sod[OPT_SHADOW].cap &= ~SANE_CAP_INACTIVE;
                    sod[OPT_MIDTONE].cap &= ~SANE_CAP_INACTIVE;
                    sod[OPT_HIGHLIGHT].cap &= ~SANE_CAP_INACTIVE;
                    sod[OPT_EXPOSURE].cap &= ~SANE_CAP_INACTIVE;
                    sod[OPT_SHADOW_R].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_MIDTONE_R].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_HIGHLIGHT_R].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_EXPOSURE_R].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_SHADOW_G].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_MIDTONE_G].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_HIGHLIGHT_G].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_EXPOSURE_G].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_SHADOW_B].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_MIDTONE_B].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_HIGHLIGHT_B].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_EXPOSURE_B].cap |= SANE_CAP_INACTIVE;
                  }
                else if ( strcmp(val[option].s, MD_CHANNEL_RED) == 0 )
                  {
                    sod[OPT_SHADOW].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_MIDTONE].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_HIGHLIGHT].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_EXPOSURE].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_SHADOW_R].cap &= ~SANE_CAP_INACTIVE;
                    sod[OPT_MIDTONE_R].cap &= ~SANE_CAP_INACTIVE;
                    sod[OPT_HIGHLIGHT_R].cap &= ~SANE_CAP_INACTIVE;
                    sod[OPT_EXPOSURE_R].cap &= ~SANE_CAP_INACTIVE;
                    sod[OPT_SHADOW_G].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_MIDTONE_G].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_HIGHLIGHT_G].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_EXPOSURE_G].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_SHADOW_B].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_MIDTONE_B].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_HIGHLIGHT_B].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_EXPOSURE_B].cap |= SANE_CAP_INACTIVE;
                  }
                else if ( strcmp(val[option].s, MD_CHANNEL_GREEN) == 0 )
                  {
                    sod[OPT_SHADOW].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_MIDTONE].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_HIGHLIGHT].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_EXPOSURE].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_SHADOW_R].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_MIDTONE_R].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_HIGHLIGHT_R].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_EXPOSURE_R].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_SHADOW_G].cap &= ~SANE_CAP_INACTIVE;
                    sod[OPT_MIDTONE_G].cap &= ~SANE_CAP_INACTIVE;
                    sod[OPT_HIGHLIGHT_G].cap &= ~SANE_CAP_INACTIVE;
                    sod[OPT_EXPOSURE_G].cap &= ~SANE_CAP_INACTIVE;
                    sod[OPT_SHADOW_B].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_MIDTONE_B].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_HIGHLIGHT_B].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_EXPOSURE_B].cap |= SANE_CAP_INACTIVE;
                  }
                else if ( strcmp(val[option].s, MD_CHANNEL_BLUE) == 0 )
                  {
                    sod[OPT_SHADOW].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_MIDTONE].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_HIGHLIGHT].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_EXPOSURE].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_SHADOW_R].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_MIDTONE_R].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_HIGHLIGHT_R].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_EXPOSURE_R].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_SHADOW_G].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_MIDTONE_G].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_HIGHLIGHT_G].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_EXPOSURE_G].cap |= SANE_CAP_INACTIVE;
                    sod[OPT_SHADOW_B].cap &= ~SANE_CAP_INACTIVE;
                    sod[OPT_MIDTONE_B].cap &= ~SANE_CAP_INACTIVE;
                    sod[OPT_HIGHLIGHT_B].cap &= ~SANE_CAP_INACTIVE;
                    sod[OPT_EXPOSURE_B].cap &= ~SANE_CAP_INACTIVE;
                  }
                return SANE_STATUS_GOOD;

              case OPT_GAMMA_MODE:
                restore_gamma_options(sod, val);
                if ( info )
                    *info |= SANE_INFO_RELOAD_OPTIONS;
                return SANE_STATUS_GOOD;

              case OPT_GAMMA_BIND:
                restore_gamma_options(sod, val);
                if ( info )
                    *info |= SANE_INFO_RELOAD_OPTIONS;

                return SANE_STATUS_GOOD;

              case OPT_SHADOW:
              case OPT_SHADOW_R:
              case OPT_SHADOW_G:
              case OPT_SHADOW_B:
                if ( val[option].w >= val[option + 1].w )
                  {
                    val[option + 1].w = val[option].w + 1;
                    if ( info )
                        *info |= SANE_INFO_RELOAD_OPTIONS;
                  }
                if ( val[option + 1].w >= val[option + 2].w )
                    val[option + 2].w = val[option + 1].w + 1;

                return SANE_STATUS_GOOD;

              case OPT_MIDTONE:
              case OPT_MIDTONE_R:
              case OPT_MIDTONE_G:
              case OPT_MIDTONE_B:
                if ( val[option].w <= val[option - 1].w )
                  {
                    val[option - 1].w = val[option].w - 1;
                    if ( info )
                        *info |= SANE_INFO_RELOAD_OPTIONS;
                  }
                if ( val[option].w >= val[option + 1].w )
                  {
                    val[option + 1].w = val[option].w + 1;
                    if ( info )
                        *info |= SANE_INFO_RELOAD_OPTIONS;
                  }

                return SANE_STATUS_GOOD;

              case OPT_HIGHLIGHT:
              case OPT_HIGHLIGHT_R:
              case OPT_HIGHLIGHT_G:
              case OPT_HIGHLIGHT_B:
                if ( val[option].w <= val[option - 1].w )
                  {
                    val[option - 1].w = val[option].w - 1;
                    if ( info )
                        *info |= SANE_INFO_RELOAD_OPTIONS;
                  }
                if ( val[option - 1].w <= val[option - 2].w )
                    val[option - 2].w = val[option - 1].w - 1;

                return SANE_STATUS_GOOD;

              case OPT_RESOLUTION_BIND:
                if ( ms->val[option].w == SANE_FALSE )
                  {
                    ms->sod[OPT_Y_RESOLUTION].cap &= ~SANE_CAP_INACTIVE;
                  }
                else
                  {
                    ms->sod[OPT_Y_RESOLUTION].cap |= SANE_CAP_INACTIVE;
                  }
                if ( info )
                    *info |= SANE_INFO_RELOAD_OPTIONS;
                return SANE_STATUS_GOOD;

              case OPT_TOGGLELAMP:
                status = scsi_read_system_status(md, -1);
                if ( status != SANE_STATUS_GOOD )
                    return SANE_STATUS_IO_ERROR;

                md->status.flamp ^= 1;
                status = scsi_send_system_status(md, -1);
                if ( status != SANE_STATUS_GOOD )
                    return SANE_STATUS_IO_ERROR;
                return SANE_STATUS_GOOD;

              case OPT_AUTOADJUST:
                if ( info )
                    *info |= SANE_INFO_RELOAD_OPTIONS;

                if ( ms->val[option].w == SANE_FALSE )
                    ms->sod[OPT_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;
                else
                    ms->sod[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;

                return SANE_STATUS_GOOD;

              case OPT_BALANCE_FW:
                   val[OPT_BALANCE_R].w =
                        SANE_FIX((uint8_t)( (float)mi->balance[0] / 2.55 ) );
                   val[OPT_BALANCE_G].w =
                        SANE_FIX((uint8_t)( (float)mi->balance[1] / 2.55 ) );
                   val[OPT_BALANCE_B].w =
                        SANE_FIX((uint8_t)( (float)mi->balance[2] / 2.55 ) );
                   if ( info )
                       *info |= SANE_INFO_RELOAD_OPTIONS;

                return SANE_STATUS_GOOD;


              default:
                return SANE_STATUS_UNSUPPORTED;
            }
#if 0
          break;
#endif
        default:
          DBG(1, "sane_control_option: Unsupported action %d\n", action);
          return SANE_STATUS_UNSUPPORTED;
      }
}

/*---------- sane_get_option_descriptor() ------------------------------------*/

const SANE_Option_Descriptor *
sane_get_option_descriptor(SANE_Handle handle, SANE_Int n)
{
    Microtek2_Scanner *ms = handle;

    DBG(255, "sane_get_option_descriptor: handle=%p, sod=%p, opt=%d\n",
              (void *) handle, (void *) ms->sod, n);

    if ( n < 0 || n >= NUM_OPTIONS )
      {
        DBG(30, "sane_get_option_descriptor: invalid option %d\n", n);
        return NULL;
      }

    return &ms->sod[n];
}

/*---------- restore_gamma_options() -----------------------------------------*/

static SANE_Status
restore_gamma_options(SANE_Option_Descriptor *sod, Option_Value *val)
{

    DBG(40, "restore_gamma_options: val=%p, sod=%p\n", (void *) val, (void *) sod);
    /* if we dont have a gamma table return immediately */
    if ( ! val[OPT_GAMMA_MODE].s )
       return SANE_STATUS_GOOD;

    if ( strcmp(val[OPT_MODE].s, MD_MODESTRING_COLOR) == 0 )
      {
        sod[OPT_GAMMA_MODE].cap &= ~SANE_CAP_INACTIVE;
        if ( strcmp(val[OPT_GAMMA_MODE].s, MD_GAMMAMODE_LINEAR) == 0 )
          {
            sod[OPT_GAMMA_BIND].cap |= SANE_CAP_INACTIVE;
            sod[OPT_GAMMA_SCALAR].cap |= SANE_CAP_INACTIVE;
            sod[OPT_GAMMA_SCALAR_R].cap |= SANE_CAP_INACTIVE;
            sod[OPT_GAMMA_SCALAR_G].cap |= SANE_CAP_INACTIVE;
            sod[OPT_GAMMA_SCALAR_B].cap |= SANE_CAP_INACTIVE;
            sod[OPT_GAMMA_CUSTOM].cap |= SANE_CAP_INACTIVE;
            sod[OPT_GAMMA_CUSTOM_R].cap |= SANE_CAP_INACTIVE;
            sod[OPT_GAMMA_CUSTOM_G].cap |= SANE_CAP_INACTIVE;
            sod[OPT_GAMMA_CUSTOM_B].cap |= SANE_CAP_INACTIVE;
          }
        else if ( strcmp(val[OPT_GAMMA_MODE].s, MD_GAMMAMODE_SCALAR) == 0 )
          {
            sod[OPT_GAMMA_BIND].cap &= ~SANE_CAP_INACTIVE;
            if ( val[OPT_GAMMA_BIND].w == SANE_TRUE )
              {
                sod[OPT_GAMMA_SCALAR].cap &= ~SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR_R].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR_G].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR_B].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM_R].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM_G].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM_B].cap |= SANE_CAP_INACTIVE;
              }
            else
              {
                sod[OPT_GAMMA_SCALAR].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR_R].cap &= ~SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR_G].cap &= ~SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR_B].cap &= ~SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM_R].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM_G].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM_B].cap |= SANE_CAP_INACTIVE;
              }
          }
        else if ( strcmp(val[OPT_GAMMA_MODE].s, MD_GAMMAMODE_CUSTOM) == 0 )
          {
            sod[OPT_GAMMA_BIND].cap &= ~SANE_CAP_INACTIVE;
            if ( val[OPT_GAMMA_BIND].w == SANE_TRUE )
              {
                sod[OPT_GAMMA_CUSTOM].cap &= ~SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM_R].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM_G].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM_B].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR_R].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR_G].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR_B].cap |= SANE_CAP_INACTIVE;
              }
            else
              {
                sod[OPT_GAMMA_CUSTOM].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM_R].cap &= ~SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM_G].cap &= ~SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM_B].cap &= ~SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR_R].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR_G].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR_B].cap |= SANE_CAP_INACTIVE;
              }
          }
      }
    else if ( strcmp(val[OPT_MODE].s, MD_MODESTRING_GRAY) == 0 )
      {
        sod[OPT_GAMMA_MODE].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_BIND].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_SCALAR_R].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_SCALAR_G].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_SCALAR_B].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM_R].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM_G].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM_B].cap |= SANE_CAP_INACTIVE;
        if ( strcmp(val[OPT_GAMMA_MODE].s, MD_GAMMAMODE_LINEAR) == 0 )
          {
            sod[OPT_GAMMA_SCALAR].cap |= SANE_CAP_INACTIVE;
            sod[OPT_GAMMA_CUSTOM].cap |= SANE_CAP_INACTIVE;
          }
        else if ( strcmp(val[OPT_GAMMA_MODE].s, MD_GAMMAMODE_SCALAR) == 0 )
          {
            sod[OPT_GAMMA_SCALAR].cap &= ~SANE_CAP_INACTIVE;
            sod[OPT_GAMMA_CUSTOM].cap |= SANE_CAP_INACTIVE;
          }
        else if ( strcmp(val[OPT_GAMMA_MODE].s, MD_GAMMAMODE_CUSTOM) == 0 )
          {
            sod[OPT_GAMMA_CUSTOM].cap &= ~SANE_CAP_INACTIVE;
            sod[OPT_GAMMA_SCALAR].cap |= SANE_CAP_INACTIVE;
          }
      }
    else if ( strcmp(val[OPT_MODE].s, MD_MODESTRING_HALFTONE) == 0
              || strcmp(val[OPT_MODE].s, MD_MODESTRING_LINEART) == 0 )
      {
        /* reset gamma to default */
        if ( val[OPT_GAMMA_MODE].s )
            free((void *) val[OPT_GAMMA_MODE].s);
        val[OPT_GAMMA_MODE].s = strdup(MD_GAMMAMODE_LINEAR);
        sod[OPT_GAMMA_MODE].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_BIND].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_SCALAR].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_SCALAR_R].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_SCALAR_G].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_SCALAR_B].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM_R].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM_G].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM_B].cap |= SANE_CAP_INACTIVE;
      }
    else
        DBG(1, "restore_gamma_options: unknown mode %s\n", val[OPT_MODE].s);

    return SANE_STATUS_GOOD;
}


/*---------- calculate_sane_params() -----------------------------------------*/

static SANE_Status
calculate_sane_params(Microtek2_Scanner *ms)
{
    Microtek2_Device *md;
    Microtek2_Info *mi;


    DBG(30, "calculate_sane_params: ms=%p\n", (void *) ms);

    md = ms->dev;
    mi = &md->info[md->scan_source];

    if ( ! mi->onepass && ms->mode == MS_MODE_COLOR )
      {
        if ( ms->current_pass == 1 )
            ms->params.format = SANE_FRAME_RED;
        else if ( ms->current_pass == 2 )
            ms->params.format = SANE_FRAME_GREEN;
        else if ( ms->current_pass == 3 )
            ms->params.format = SANE_FRAME_BLUE;
        else
          {
            DBG(1, "calculate_sane_params: invalid pass number %d\n",
                    ms->current_pass);
            return SANE_STATUS_IO_ERROR;
          }
      }
    else if ( mi->onepass && ms->mode == MS_MODE_COLOR )
        ms->params.format = SANE_FRAME_RGB;
    else
        ms->params.format = SANE_FRAME_GRAY;

    if ( ! mi->onepass && ms->mode == MS_MODE_COLOR && ms->current_pass < 3 )
        ms->params.last_frame = SANE_FALSE;
    else
        ms->params.last_frame = SANE_TRUE;
    ms->params.lines = ms->src_remaining_lines;
    ms->params.pixels_per_line = ms->ppl;
    ms->params.bytes_per_line = ms->real_bpl;
    ms->params.depth = ms->bits_per_pixel_out;

    return SANE_STATUS_GOOD;

}

/*---------- get_calib_params() ----------------------------------------------*/

static void
get_calib_params(Microtek2_Scanner *ms)
{
    Microtek2_Device *md;
    Microtek2_Info *mi;

    
    DBG(30, "get_calib_params: handle=%p\n", (void *) ms);

    md = ms->dev;
    mi = &md->info[md->scan_source];
    
    if ( md->model_flags & MD_CALIB_DIVISOR_600 )
      {
        if ( ms->x_resolution_dpi <= 600 )
            mi->calib_divisor = 2;
        else
            mi->calib_divisor = 1;
      }
    DBG(30, "Calib Divisor: %d\n", mi->calib_divisor);


    ms->x_resolution_dpi = mi->opt_resolution / mi->calib_divisor;
    ms->y_resolution_dpi = mi->opt_resolution / 5; /* ignore dust particles */
    ms->x1_dots = 0;
    ms->y1_dots = mi->calib_white;
    ms->width_dots = mi->geo_width;
    if ( md->shading_length != 0 )
       ms->height_dots = md->shading_length;
    else
       ms->height_dots = mi->calib_space;

    ms->mode = MS_MODE_COLOR;

    if ( mi->depth & MI_HASDEPTH_16 )
        ms->depth = 16;
    else if ( mi->depth & MI_HASDEPTH_14 )
        ms->depth = 14;
    else if ( mi->depth & MI_HASDEPTH_12 )
        ms->depth = 12;
    else if ( mi->depth & MI_HASDEPTH_10 )
        ms->depth = 10;
    else 
        ms->depth = 8;

    ms->stay = 0;
    if ( mi->calib_space < 10 )
        ms->stay = 1;
    ms->rawdat = 1;
    ms->quality = 1;
    ms->fastscan = 0;
/*    ms->scan_source = md->scan_source; */
    ms->scan_source = 0;
    ms->brightness_m = ms->brightness_r = ms->brightness_g =
                       ms->brightness_b = 128;
    ms->exposure_m = ms->exposure_r = ms->exposure_g = ms->exposure_b = 0;
    ms->contrast_m = ms->contrast_r = ms->contrast_g = ms->contrast_b = 128;
    ms->shadow_m = ms->shadow_r = ms->shadow_g = ms->shadow_b = 0;
    ms->midtone_m = ms->midtone_r = ms->midtone_g = ms->midtone_b = 128;
    ms->highlight_m = ms->highlight_r = ms->highlight_g = ms->highlight_b = 255;

    return;
}


/*---------- get_scan_parameters () ------------------------------------------*/

static SANE_Status
get_scan_parameters(Microtek2_Scanner *ms)
{
    Microtek2_Device *md;
    Microtek2_Info *mi;
    double dpm;                   /* dots per millimeter */
    int x2_dots;
    int y2_dots;
    int i;


    DBG(30, "get_scan_parameters: handle=%p\n", (void *) ms);

    md = ms->dev;
    mi = &md->info[md->scan_source];

    get_scan_mode_and_depth(ms, &ms->mode, &ms->depth,
                            &ms->bits_per_pixel_in, &ms->bits_per_pixel_out);
    
    /* get the scan_source */
    if ( strcmp(ms->val[OPT_SOURCE].s, MD_SOURCESTRING_FLATBED) == 0 )
        ms->scan_source = MS_SOURCE_FLATBED;
    else if ( strcmp(ms->val[OPT_SOURCE].s, MD_SOURCESTRING_ADF) == 0 )
        ms->scan_source = MS_SOURCE_ADF;
    else if ( strcmp(ms->val[OPT_SOURCE].s, MD_SOURCESTRING_TMA) == 0 )
        ms->scan_source = MS_SOURCE_TMA;
    else if ( strcmp(ms->val[OPT_SOURCE].s, MD_SOURCESTRING_STRIPE) == 0 )
        ms->scan_source = MS_SOURCE_STRIPE;
    else if ( strcmp(ms->val[OPT_SOURCE].s, MD_SOURCESTRING_SLIDE) == 0 )
        ms->scan_source = MS_SOURCE_SLIDE;

    /* enable/disable backtracking */
    if ( ms->val[OPT_DISABLE_BACKTRACK].w == SANE_TRUE )
        ms->no_backtracking = 1;
    else
        ms->no_backtracking = 0;

    /* turn off the lamp during a scan */
    if ( ms->val[OPT_LIGHTLID35].w == SANE_TRUE )
        ms->lightlid35 = 1;
    else
        ms->lightlid35 = 0;

    /* automatic adjustment of threshold */
    if ( ms->val[OPT_AUTOADJUST].w == SANE_TRUE)
        ms->auto_adjust = 1;
    else
        ms->auto_adjust = 0;

    /* color calibration by backend */
    if ( ms->val[OPT_CALIB_BACKEND].w == SANE_TRUE )
        ms->calib_backend = 1;
    else
        ms->calib_backend = 0;

    /* if halftone mode select halftone pattern */
    if ( ms->mode == MS_MODE_HALFTONE )
      {
        i = 0;
        while ( strcmp(md->halftone_mode_list[i], ms->val[OPT_HALFTONE].s) ) 
            ++i;
        ms->internal_ht_index = i;
      }

    /* if lineart get the value for threshold */ 
    if ( ms->mode == MS_MODE_LINEART || ms->mode == MS_MODE_LINEARTFAKE)   
        ms->threshold = (uint8_t) ms->val[OPT_THRESHOLD].w;
    else
        ms->threshold = (uint8_t) M_THRESHOLD_DEFAULT;

    DBG(30, "get_scan_parameters: mode=%d, depth=%d, bpp_in=%d, bpp_out=%d\n",
             ms->mode, ms->depth, ms->bits_per_pixel_in, 
             ms->bits_per_pixel_out);

    /* calculate positions, width and height in dots */
    /* check for impossible values */
    /* ensure a minimum scan area of 10 x 10 pixels */
    dpm = (double) mi->opt_resolution / MM_PER_INCH;
    ms->x1_dots = (SANE_Int) ( SANE_UNFIX(ms->val[OPT_TL_X].w) * dpm + 0.5 );
    if ( ms->x1_dots > ( mi->geo_width - 10 ) )
        ms->x1_dots = ( mi->geo_width - 10 );
    ms->y1_dots = (SANE_Int) ( SANE_UNFIX(ms->val[OPT_TL_Y].w) * dpm + 0.5 );
    if ( ms->y1_dots > ( mi->geo_height - 10 ) )
        ms->y1_dots = ( mi->geo_height - 10 );
    x2_dots = (int) ( SANE_UNFIX(ms->val[OPT_BR_X].w) * dpm + 0.5 );
    if ( x2_dots >= mi->geo_width )
        x2_dots = mi->geo_width - 1;
    y2_dots = (int) ( SANE_UNFIX(ms->val[OPT_BR_Y].w) * dpm + 0.5 );
    if ( y2_dots >= mi->geo_height )
        y2_dots = mi->geo_height - 1;
    ms->width_dots = x2_dots - ms->x1_dots;
    if ( md->model_flags && MD_OFFSET_2 ) /* this firmware has problems with */
      if ( ( ms->width_dots % 2 ) == 1 )  /* odd pixel numbers */
        ms->width_dots -= 1;
    if ( ms->width_dots < 10 )
        ms->width_dots = 10;
    ms->height_dots = y2_dots - ms->y1_dots;
    if ( ms->height_dots < 10 )
        ms->height_dots = 10;

/*test!!!*/
/*    ms->y1_dots -= 50;*/

    /* take scanning direction into account */
    if ((mi->direction & MI_DATSEQ_RTOL) == 1)
        ms->x1_dots = mi->geo_width - ms->x1_dots - ms->width_dots;

    if ( ms->val[OPT_RESOLUTION_BIND].w == SANE_TRUE )
      {
        ms->x_resolution_dpi =
                    (SANE_Int) (SANE_UNFIX(ms->val[OPT_RESOLUTION].w) + 0.5);
        ms->y_resolution_dpi =
                    (SANE_Int) (SANE_UNFIX(ms->val[OPT_RESOLUTION].w) + 0.5);
      }
    else
      {
        ms->x_resolution_dpi =
                    (SANE_Int) (SANE_UNFIX(ms->val[OPT_RESOLUTION].w) + 0.5);
        ms->y_resolution_dpi =
                    (SANE_Int) (SANE_UNFIX(ms->val[OPT_Y_RESOLUTION].w) + 0.5);
      }

    if ( ms->x_resolution_dpi < 10 )
        ms->x_resolution_dpi = 10;
    if ( ms->y_resolution_dpi < 10 )
        ms->y_resolution_dpi = 10;

    DBG(30, "get_scan_parameters: yres=%d, x1=%d, width=%d, y1=%d, height=%d\n",
             ms->y_resolution_dpi, ms->x1_dots, ms->width_dots,
             ms->y1_dots, ms->height_dots);

    /* Preview mode */
    if ( ms->val[OPT_PREVIEW].w == SANE_TRUE )
      {
        ms->fastscan = SANE_TRUE;
        ms->quality = SANE_FALSE;
      }
    else
      {
        ms->fastscan = SANE_FALSE;
        ms->quality = SANE_TRUE;
      }

    ms->rawdat = 0;

    /* brightness, contrast, values 1,..,255 */
    ms->brightness_m = (uint8_t) (SANE_UNFIX(ms->val[OPT_BRIGHTNESS].w)
                      / SANE_UNFIX(md->percentage_range.max) * 254.0) + 1;
    ms->brightness_r = ms->brightness_g = ms->brightness_b = ms->brightness_m;

    ms->contrast_m = (uint8_t) (SANE_UNFIX(ms->val[OPT_CONTRAST].w)
                    / SANE_UNFIX(md->percentage_range.max) * 254.0) + 1;
    ms->contrast_r = ms->contrast_g = ms->contrast_b = ms->contrast_m;

    /* shadow, midtone, highlight, exposure */
    ms->shadow_m = (uint8_t) ms->val[OPT_SHADOW].w;
    ms->shadow_r = (uint8_t) ms->val[OPT_SHADOW_R].w;
    ms->shadow_g = (uint8_t) ms->val[OPT_SHADOW_G].w;
    ms->shadow_b = (uint8_t) ms->val[OPT_SHADOW_B].w;
    ms->midtone_m = (uint8_t) ms->val[OPT_MIDTONE].w;
    ms->midtone_r = (uint8_t) ms->val[OPT_MIDTONE_R].w;
    ms->midtone_g = (uint8_t) ms->val[OPT_MIDTONE_G].w;
    ms->midtone_b = (uint8_t) ms->val[OPT_MIDTONE_B].w;
    ms->highlight_m = (uint8_t) ms->val[OPT_HIGHLIGHT].w;
    ms->highlight_r = (uint8_t) ms->val[OPT_HIGHLIGHT_R].w;
    ms->highlight_g = (uint8_t) ms->val[OPT_HIGHLIGHT_G].w;
    ms->highlight_b = (uint8_t) ms->val[OPT_HIGHLIGHT_B].w;
    ms->exposure_m = (uint8_t) (ms->val[OPT_EXPOSURE].w / 2);
    ms->exposure_r = (uint8_t) (ms->val[OPT_EXPOSURE_R].w / 2);
    ms->exposure_g = (uint8_t) (ms->val[OPT_EXPOSURE_G].w / 2);
    ms->exposure_b = (uint8_t) (ms->val[OPT_EXPOSURE_B].w / 2);

    ms->gamma_mode = strdup( (char *) ms->val[OPT_GAMMA_MODE].s);

    ms->balance[0] = (uint8_t) (SANE_UNFIX(ms->val[OPT_BALANCE_R].w));
    ms->balance[1] = (uint8_t) (SANE_UNFIX(ms->val[OPT_BALANCE_G].w));
    ms->balance[2] = (uint8_t) (SANE_UNFIX(ms->val[OPT_BALANCE_B].w));
    DBG(255, "get_scan_parameters:ms->balance[0]=%d,[1]=%d,[2]=%d\n",
               ms->balance[0], ms->balance[1], ms->balance[2]);

    return SANE_STATUS_GOOD;
}

/*---------- get_scan_mode_and_depth() ---------------------------------------*/

static SANE_Status
get_scan_mode_and_depth(Microtek2_Scanner *ms,
                        int *mode,
                        int *depth,
                        int *bits_per_pixel_in,
                        int *bits_per_pixel_out)
{
    /* This function translates the strings for the possible modes and */
    /* bitdepth into a more conveniant format as needed for SET WINDOW. */
    /* bits_per_pixel is the number of bits per color one pixel needs */
    /* when transferred from the scanner, bits_perpixel_out is the */
    /* number of bits per color one pixel uses when transferred to the */
    /* frontend. These may be different. For example, with a depth of 4 */
    /* two pixels per byte are transferred from the scanner, but only one */
    /* pixel per byte is transferred to the frontend. */
    /* If lineart_fake is set to !=0, we need the parameters for a */
    /* grayscale scan, because the scanner has no lineart mode */

    Microtek2_Device *md;
    Microtek2_Info *mi;

    DBG(30, "get_scan_mode_and_depth: handle=%p\n", (void *) ms);

    md = ms->dev;
    mi = &md->info[md->scan_source];

    if ( strcmp(ms->val[OPT_MODE].s, MD_MODESTRING_COLOR) == 0 )
	*mode = MS_MODE_COLOR;
    else if ( strcmp(ms->val[OPT_MODE].s, MD_MODESTRING_GRAY) == 0 )
	*mode = MS_MODE_GRAY;
    else if ( strcmp(ms->val[OPT_MODE].s, MD_MODESTRING_HALFTONE) == 0)
	*mode = MS_MODE_HALFTONE;
    else if ( strcmp(ms->val[OPT_MODE].s, MD_MODESTRING_LINEART) == 0 )
      {
        if ( MI_LINEART_NONE(mi->scanmode)
             || ms->val[OPT_AUTOADJUST].w == SANE_TRUE
             || md->model_flags & MD_READ_CONTROL_BIT)
            *mode = MS_MODE_LINEARTFAKE;
        else
	    *mode = MS_MODE_LINEART;
      }
    else
      {
        DBG(1, "get_scan_mode_and_depth: Unknown mode %s\n",
                ms->val[OPT_MODE].s);
        return SANE_STATUS_INVAL;
      }

    if ( strcmp(ms->val[OPT_MODE].s, MD_MODESTRING_COLOR) == 0
         || strcmp(ms->val[OPT_MODE].s, MD_MODESTRING_GRAY) == 0 )
      {
        if ( ms->val[OPT_BITDEPTH].w == MD_DEPTHVAL_16 )
          {
            *depth = 16;
            *bits_per_pixel_in = *bits_per_pixel_out = 16;
          }
        else if ( ms->val[OPT_BITDEPTH].w == MD_DEPTHVAL_14 )
          {
            *depth = 14;
            *bits_per_pixel_in = *bits_per_pixel_out = 16;
          }
        else if ( ms->val[OPT_BITDEPTH].w == MD_DEPTHVAL_12 )
          {
            *depth = 12;
            *bits_per_pixel_in = *bits_per_pixel_out = 16;
          }
        else if ( ms->val[OPT_BITDEPTH].w == MD_DEPTHVAL_10 )
          {
            *depth = 10;
            *bits_per_pixel_in = *bits_per_pixel_out = 16;
          }
        else if ( ms->val[OPT_BITDEPTH].w ==  MD_DEPTHVAL_8 )
          {
            *depth = 8;
            *bits_per_pixel_in = *bits_per_pixel_out = 8;
          }
        else if ( ms->val[OPT_MODE].w == MD_DEPTHVAL_4 )
          {
            *depth = 4;
            *bits_per_pixel_in = 4;
            *bits_per_pixel_out = 8;
          }
      }
    else if ( strcmp(ms->val[OPT_MODE].s, MD_MODESTRING_HALFTONE) == 0  )
      {
        *depth = 1;
        *bits_per_pixel_in = *bits_per_pixel_out = 1;
      }
    else                   /* lineart */
      {
        *bits_per_pixel_out = 1;
        if ( *mode == MS_MODE_LINEARTFAKE )
          {
            *depth = 8;
            *bits_per_pixel_in = 8;
          }
        else
          {
            *depth = 1;
            *bits_per_pixel_in = 1;
          }
      }

#if 0
    if ( ms->val[OPT_PREVIEW].w == SANE_TRUE )
      {
        if ( *depth > 8 )
          {
            *depth = 8;
            *bits_per_pixel_in = *bits_per_pixel_out = 8;
          }
      }
#endif

    DBG(30, "get_scan_mode_and_depth: mode=%d, depth=%d,"
            " bits_pp_in=%d, bits_pp_out=%d, preview=%d\n",
             *mode, *depth, *bits_per_pixel_in, *bits_per_pixel_out,
             ms->val[OPT_PREVIEW].w);
             
    return SANE_STATUS_GOOD;
}


/*---------- scsi_wait_for_image() -------------------------------------------*/

static SANE_Status
scsi_wait_for_image(Microtek2_Scanner *ms)
{
    int retry = 60;
    SANE_Status status;


    DBG(30, "scsi_wait_for_image: ms=%p\n", (void *) ms);

    while ( retry-- > 0 )
      {
        status = scsi_read_image_status(ms);
        if  (status == SANE_STATUS_DEVICE_BUSY )
          {
            sleep(1);
            continue;
          }
        if ( status == SANE_STATUS_GOOD )
            return status;

        /* status != GOOD && != BUSY */
        DBG(1, "scsi_wait_for_image: '%s'\n", sane_strstatus(status));
        return status;
      }

    /* BUSY after n retries */
    DBG(1, "scsi_wait_for_image: '%s'\n", sane_strstatus(status));
    return status;
}


/*---------- scsi_read_gamma() -----------------------------------------------*/

/* currently not used */
/*
static SANE_Status
scsi_read_gamma(Microtek2_Scanner *ms, int color)
{
    uint8_t readgamma[RG_CMD_L];
    uint8_t result[3072];
    size_t size;
    SANE_Bool endiantype;
    SANE_Status status;

    RG_CMD(readgamma);
    ENDIAN_TYPE(endiantype);
    RG_PCORMAC(readgamma, endiantype);
    RG_COLOR(readgamma, color);
    RG_WORD(readgamma, ( ms->dev->lut_entry_size == 1 ) ? 0 : 1);
    RG_TRANSFERLENGTH(readgamma, (color == 3 ) ? 3072 : 1024);

    dump_area(readgamma, 10, "ReadGamma");

    size = sizeof(result);
    status = sanei_scsi_cmd(ms->sfd, readgamma, sizeof(readgamma),
                            result, &size);
    if ( status != SANE_STATUS_GOOD ) {
        DBG(1, "scsi_read_gamma: (L,R) read_gamma failed: status '%s'\n",
                sane_strstatus(status));
        return status;
    }

    dump_area(result, 3072, "Result");

    return SANE_STATUS_GOOD;
}
*/


/*---------- scsi_send_gamma() -----------------------------------------------*/

static SANE_Status
scsi_send_gamma(Microtek2_Scanner *ms)
{
    SANE_Bool endiantype;
    SANE_Status status;
    size_t size;
    uint8_t *cmd, color;


    DBG(30, "scsi_send_gamma: pos=%p, size=%d, word=%d, color=%d\n",
             ms->gamma_table, ms->lut_size_bytes, ms->word, ms->current_color);

    if ( ( 3 * ms->lut_size_bytes ) <= 0xffff ) /*send Gamma with one command*/
      {
        cmd = (uint8_t *) alloca(SG_CMD_L + 3 * ms->lut_size_bytes);
        if ( cmd == NULL )
          {
            DBG(1, "scsi_send_gamma: Couldn't get buffer for gamma table\n");
            return SANE_STATUS_IO_ERROR;
          }

        SG_SET_CMD(cmd);
        ENDIAN_TYPE(endiantype)
        SG_SET_PCORMAC(cmd, endiantype);
        SG_SET_COLOR(cmd, ms->current_color);
        SG_SET_WORD(cmd, ms->word);
        SG_SET_TRANSFERLENGTH(cmd, 3 * ms->lut_size_bytes);
        memcpy(cmd + SG_CMD_L, ms->gamma_table, 3 * ms->lut_size_bytes);
        size = 3 * ms->lut_size_bytes;
        if ( md_dump >= 2 )
                dump_area2(cmd, SG_CMD_L, "sendgammacmd");
        if ( md_dump >= 3 )
                dump_area2(cmd + SG_CMD_L, size, "sendgammadata");

        status = sanei_scsi_cmd(ms->sfd, cmd, size + SG_CMD_L, NULL, 0);
        if ( status != SANE_STATUS_GOOD )
                DBG(1, "scsi_send_gamma: '%s'\n", sane_strstatus(status));
      }

    else  /* send gamma with 3 commands, one for each color */
      {
        for ( color = 0; color < 3; color++ )
          {
            cmd = (uint8_t *) alloca(SG_CMD_L + ms->lut_size_bytes);
            if ( cmd == NULL )
              {
                DBG(1, "scsi_send_gamma: Couldn't get buffer for gamma table\n");
                return SANE_STATUS_IO_ERROR;
              }
            SG_SET_CMD(cmd);
            ENDIAN_TYPE(endiantype)
            SG_SET_PCORMAC(cmd, endiantype);
            SG_SET_COLOR(cmd, color);
            SG_SET_WORD(cmd, ms->word);
            SG_SET_TRANSFERLENGTH(cmd, ms->lut_size_bytes);
            memcpy(cmd + SG_CMD_L,
                   ms->gamma_table + color * ms->lut_size_bytes,
                   ms->lut_size_bytes);
            size = ms->lut_size_bytes;
            if ( md_dump >= 2 )
                    dump_area2(cmd, SG_CMD_L, "sendgammacmd");
            if ( md_dump >= 3 )
                    dump_area2(cmd + SG_CMD_L, size, "sendgammadata");

            status = sanei_scsi_cmd(ms->sfd, cmd, size + SG_CMD_L, NULL, 0);
            if ( status != SANE_STATUS_GOOD )
                    DBG(1, "scsi_send_gamma: '%s'\n", sane_strstatus(status));
          }

      }

    return status;
}


/*---------- scsi_inquiry() --------------------------------------------------*/

static SANE_Status
scsi_inquiry(Microtek2_Info *mi, char *device)
{
    SANE_Status status;
    uint8_t cmd[INQ_CMD_L];
    uint8_t *result;
    uint8_t inqlen;
    size_t size;
    int sfd;


    DBG(30, "scsi_inquiry: mi=%p, device='%s'\n", (void *) mi, device);

    status = sanei_scsi_open(device, &sfd, scsi_sense_handler, 0);
    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "scsi_inquiry: '%s'\n", sane_strstatus(status));
        return status;
      }

    INQ_CMD(cmd);
    INQ_SET_ALLOC(cmd, INQ_ALLOC_L);
    result = (uint8_t *) alloca(INQ_ALLOC_L);
    if ( result == NULL )
      {
        DBG(1, "scsi_inquiry: malloc failed\n");
        sanei_scsi_close(sfd);
        return SANE_STATUS_NO_MEM;
      }

    size = INQ_ALLOC_L;
    status = sanei_scsi_cmd(sfd, cmd, sizeof(cmd), result, &size);
    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "scsi_inquiry: '%s'\n", sane_strstatus(status));
        sanei_scsi_close(sfd);
        return status;
      }

    INQ_GET_INQLEN(inqlen, result);
    INQ_SET_ALLOC(cmd, inqlen + INQ_ALLOC_L);
    result = alloca(inqlen + INQ_ALLOC_L);
    if ( result == NULL )
      {
        DBG(1, "scsi_inquiry: malloc failed\n");
        sanei_scsi_close(sfd);
        return SANE_STATUS_NO_MEM;
      }
    size = inqlen + INQ_ALLOC_L;
    if (md_dump >= 2 )
        dump_area2(cmd, sizeof(cmd), "inquiry");

    status = sanei_scsi_cmd(sfd, cmd, sizeof(cmd), result, &size);
    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "scsi_inquiry: cmd '%s'\n", sane_strstatus(status));
        sanei_scsi_close(sfd);
        return status;
      }
    sanei_scsi_close(sfd);

    if (md_dump >= 2 )
      {
        dump_area2((uint8_t *) result, size, "inquiryresult");
        dump_area((uint8_t *) result, size, "inquiryresult");
      }

    /* copy results */
    INQ_GET_QUAL(mi->device_qualifier, result);
    INQ_GET_DEVT(mi->device_type, result);
    INQ_GET_VERSION(mi->scsi_version, result);
    INQ_GET_VENDOR(mi->vendor, (char *)result);
    INQ_GET_MODEL(mi->model, (char *)result);
    INQ_GET_REV(mi->revision, (char *)result);
    INQ_GET_MODELCODE(mi->model_code, result);


    return SANE_STATUS_GOOD;
}


/*---------- scsi_read_attributes() ------------------------------------------*/

static SANE_Status
scsi_read_attributes(Microtek2_Info *pmi, char *device, uint8_t scan_source)
{
    SANE_Status status;
    Microtek2_Info *mi;
    uint8_t readattributes[RSA_CMD_L];
    uint8_t result[RSA_TRANSFERLENGTH];
    size_t size;
    int sfd;


    mi = &pmi[scan_source];

    DBG(30, "scsi_read_attributes: mi=%p, device='%s', source=%d\n",
             (void *) mi, device, scan_source);

    RSA_CMD(readattributes);
    RSA_SETMEDIA(readattributes, scan_source);
    status = sanei_scsi_open(device, &sfd, scsi_sense_handler, 0);
    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "scsi_read_attributes: open '%s'\n", sane_strstatus(status));
        return status;
      }

    if (md_dump >= 2 )
        dump_area2(readattributes, sizeof(readattributes), "scannerattributes");

    size = sizeof(result);
    status = sanei_scsi_cmd(sfd, readattributes,
                            sizeof(readattributes), result, &size);
    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "scsi_read_attributes: cmd '%s'\n", sane_strstatus(status));
        sanei_scsi_close(sfd);
        return status;
      }

    sanei_scsi_close(sfd);

    /* The X6 appears to lie about the data format for a TMA */
    if ( (&pmi[0])->model_code == 0x91 )
        result[0] &= 0xfd;
    /* default value for calib_divisor ... bit49?? */
    mi->calib_divisor = 1;
    /* 9600XL */
    if ( (&pmi[0])->model_code == 0xde )
        mi->calib_divisor = 2;
    /* 6400XL has problems in lineart mode*/
    if ( (&pmi[0])->model_code == 0x89 )
        result[13] &= 0xfe; /* simulate no lineart */
#if 0
    result[13] &= 0xfe; /* simulate no lineart */
#endif

    /* copy all the stuff into the info structure */
    RSA_COLOR(mi->color, result);
    RSA_ONEPASS(mi->onepass, result);
    RSA_SCANNERTYPE(mi->scanner_type, result);
    RSA_FEPROM(mi->feprom, result);
    RSA_DATAFORMAT(mi->data_format, result);
    RSA_COLORSEQUENCE(mi->color_sequence, result);
    RSA_NIS(mi->new_image_status, result);
    RSA_DATSEQ(mi->direction, result);
    RSA_CCDGAP(mi->ccd_gap, result);
    RSA_MAX_XRESOLUTION(mi->max_xresolution, result);
    RSA_MAX_YRESOLUTION(mi->max_yresolution, result);
    RSA_GEOWIDTH(mi->geo_width, result);
    RSA_GEOHEIGHT(mi->geo_height, result);
    RSA_OPTRESOLUTION(mi->opt_resolution, result);
    RSA_DEPTH(mi->depth, result);
    /* The X12USL doesn't say that it has 14bit */
    if ( (&pmi[0])->model_code == 0xb0 )
        mi->depth |= MI_HASDEPTH_14;
    RSA_SCANMODE(mi->scanmode, result);
    RSA_CCDPIXELS(mi->ccd_pixels, result);
    RSA_LUTCAP(mi->lut_cap, result);
    RSA_DNLDPTRN(mi->has_dnldptrn, result);
    RSA_GRAINSLCT(mi->grain_slct, result);
    RSA_SUPPOPT(mi->option_device, result);
    RSA_CALIBWHITE(mi->calib_white, result);
    RSA_CALIBSPACE(mi->calib_space, result);
    RSA_NLENS(mi->nlens, result);
    RSA_NWINDOWS(mi->nwindows, result);
    RSA_SHTRNSFEREQU(mi->shtrnsferequ, result);
    RSA_SCNBTTN(mi->scnbuttn, result);
    RSA_BUFTYPE(mi->buftype, result);
    RSA_REDBALANCE(mi->balance[0], result);
    RSA_GREENBALANCE(mi->balance[1], result);
    RSA_BLUEBALANCE(mi->balance[2], result);
    RSA_APSMAXFRAMES(mi->aps_maxframes, result);

    if (md_dump >= 2 )
        dump_area2((uint8_t *) result, sizeof(result),
                   "scannerattributesresults");
    if ( md_dump >= 1 && md_dump_clear )
        dump_attributes(mi);

    return SANE_STATUS_GOOD;
}


/*---------- scsi_read_control_bits() ----------------------------------------*/

static SANE_Status
scsi_read_control_bits(Microtek2_Scanner *ms)
{
    Microtek2_Device *md;
    SANE_Status status;
    uint8_t cmd[RCB_CMD_L];
    uint32_t byte;
    int bit;
    int count_1s;

    md = ms->dev;

    DBG(30, "scsi_read_control_bits: ms=%p, fd=%d\n", (void *) ms, ms->sfd);
    DBG(30, "ms->control_bytes = %p\n", ms->control_bytes);

    RCB_SET_CMD(cmd);
    RCB_SET_LENGTH(cmd, ms->n_control_bytes);

    if ( md_dump >= 2)
        dump_area2(cmd, RCB_CMD_L, "readcontrolbits");

    status = sanei_scsi_cmd(ms->sfd,
                            cmd,
                            sizeof(cmd),
                            ms->control_bytes,
                            &ms->n_control_bytes);

    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "scsi_read_control_bits: cmd '%s'\n", sane_strstatus(status));
        return status;
      }

    if ( md_dump >= 2)
        dump_area2(ms->control_bytes,
                   ms->n_control_bytes,
                   "readcontrolbitsresult");

    count_1s = 0;
    for ( byte = 0; byte < ms->n_control_bytes; byte++ )
      {
        for ( bit = 0; bit < 8; bit++ )
          {
            if ( (ms->control_bytes[byte] >> bit) & 0x01 )
                ++count_1s;
          }
      }
    DBG(20, "read_control_bits: number of 1's in controlbytes: %d\n", count_1s);

    return SANE_STATUS_GOOD;
}


/*---------- scsi_set_window() -----------------------------------------------*/

static SANE_Status
scsi_set_window(Microtek2_Scanner *ms, int n) {   /* n windows, not yet */
                                                  /* implemented */
    SANE_Status status;
    uint8_t *setwindow;
    int size;


    DBG(30, "scsi_set_window: ms=%p, wnd=%d\n", (void *) ms, n);

    size = SW_CMD_L + SW_HEADER_L + n * SW_BODY_L;
    setwindow = (uint8_t *) malloc(size);
    DBG(100, "scsi_set_window: setwindow= %p, malloc'd %d Bytes\n",
              setwindow, size);
    if ( setwindow == NULL )
      {
        DBG(1, "scsi_set_window: malloc for setwindow failed\n");
        return SANE_STATUS_NO_MEM;
      }
    memset(setwindow, 0, size);

    SW_CMD(setwindow);
    SW_PARAM_LENGTH(setwindow, SW_HEADER_L + n * SW_BODY_L);
    SW_WNDDESCLEN(setwindow + SW_HEADER_P, SW_WNDDESCVAL);

#define POS  (setwindow + SW_BODY_P(n-1))

    SW_WNDID(POS, n-1);
    SW_XRESDPI(POS, ms->x_resolution_dpi);
    SW_YRESDPI(POS, ms->y_resolution_dpi);
    SW_XPOSTL(POS, ms->x1_dots);
    SW_YPOSTL(POS, ms->y1_dots);
    SW_WNDWIDTH(POS, ms->width_dots);
    SW_WNDHEIGHT(POS, ms->height_dots);
    SW_THRESHOLD(POS, ms->threshold);
    SW_IMGCOMP(POS, ms->mode);
    SW_BITSPERPIXEL(POS, ms->depth);
    SW_EXTHT(POS, ms->use_external_ht);
    SW_INTHTINDEX(POS, ms->internal_ht_index);
    SW_RIF(POS, 1);
    SW_LENS(POS, 0);                                  /* ???? */
    SW_INFINITE(POS, 0);
    SW_STAY(POS, ms->stay);
    SW_RAWDAT(POS, ms->rawdat);
    SW_QUALITY(POS, ms->quality);
    SW_FASTSCAN(POS, ms->fastscan);
    SW_MEDIA(POS, ms->scan_source);
    SW_BRIGHTNESS_M(POS, ms->brightness_m);
    SW_CONTRAST_M(POS, ms->contrast_m);
    SW_EXPOSURE_M(POS, ms->exposure_m);
    SW_SHADOW_M(POS, ms->shadow_m);
    SW_MIDTONE_M(POS, ms->midtone_m);
    SW_HIGHLIGHT_M(POS, ms->highlight_m);
    /* the following properties are only referenced if it's a color scan */
    /* but I guess they don't matter at a gray scan */
    SW_BRIGHTNESS_R(POS, ms->brightness_r);
    SW_CONTRAST_R(POS, ms->contrast_r);
    SW_EXPOSURE_R(POS, ms->exposure_r);
    SW_SHADOW_R(POS, ms->shadow_r);
    SW_MIDTONE_R(POS, ms->midtone_r);
    SW_HIGHLIGHT_R(POS, ms->highlight_r);
    SW_BRIGHTNESS_G(POS, ms->brightness_g);
    SW_CONTRAST_G(POS, ms->contrast_g);
    SW_EXPOSURE_G(POS, ms->exposure_g);
    SW_SHADOW_G(POS, ms->shadow_g);
    SW_MIDTONE_G(POS, ms->midtone_g);
    SW_HIGHLIGHT_G(POS, ms->highlight_g);
    SW_BRIGHTNESS_B(POS, ms->brightness_b);
    SW_CONTRAST_B(POS, ms->contrast_b);
    SW_EXPOSURE_B(POS, ms->exposure_b);
    SW_SHADOW_B(POS, ms->shadow_b);
    SW_MIDTONE_B(POS, ms->midtone_b);
    SW_HIGHLIGHT_B(POS, ms->highlight_b);

    if ( md_dump >= 2 )
      {
        dump_area2(setwindow, 10, "setwindowcmd");
        dump_area2(setwindow + 10 ,8 , "setwindowheader");
        dump_area2(setwindow + 18 ,61 , "setwindowbody");
      }

    status = sanei_scsi_cmd(ms->sfd, setwindow, size, NULL, 0);
    if ( status != SANE_STATUS_GOOD )
        DBG(1, "scsi_set_window: '%s'\n", sane_strstatus(status));

    DBG(100, "scsi_set_window: free setwindow at %p\n", setwindow);
    free((void *) setwindow);
    return status;
}


/*---------- scsi_read_image_info() ------------------------------------------*/

static SANE_Status
scsi_read_image_info(Microtek2_Scanner *ms)
{
    uint8_t cmd[RII_CMD_L];
    uint8_t result[RII_RESULT_L];
    size_t size;
    SANE_Status status;
    Microtek2_Device *md;

    md = ms->dev;

    DBG(30, "scsi_read_image_info: ms=%p\n", (void *) ms);

    RII_SET_CMD(cmd);

    if ( md_dump >= 2)
        dump_area2(cmd, RII_CMD_L, "readimageinfo");

    size = sizeof(result);
    status = sanei_scsi_cmd(ms->sfd, cmd, sizeof(cmd), result, &size);
    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "scsi_read_image_info: '%s'\n", sane_strstatus(status));
        return status;
      }

    if ( md_dump >= 2)
        dump_area2(result, size, "readimageinforesult");

    /* The V300 returns some values in only two bytes */
    if ( !(md->revision==2.70) && (md->model_flags & MD_RII_TWO_BYTES) )
      {
        RII_GET_V300_WIDTHPIXEL(ms->ppl, result);
        RII_GET_V300_WIDTHBYTES(ms->bpl, result);
        RII_GET_V300_HEIGHTLINES(ms->src_remaining_lines, result);
        RII_GET_V300_REMAINBYTES(ms->remaining_bytes, result);
      }
    else
      {
        RII_GET_WIDTHPIXEL(ms->ppl, result);
        RII_GET_WIDTHBYTES(ms->bpl, result);
        RII_GET_HEIGHTLINES(ms->src_remaining_lines, result);
        RII_GET_REMAINBYTES(ms->remaining_bytes, result);
      }

    DBG(30, "scsi_read_image_info: ppl=%d, bpl=%d, lines=%d, remain=%d\n",
             ms->ppl, ms->bpl, ms->src_remaining_lines, ms->remaining_bytes);

    return SANE_STATUS_GOOD;
}


/*---------- scsi_read_image() -----------------------------------------------*/

static SANE_Status
scsi_read_image(Microtek2_Scanner *ms, uint8_t *buffer, int bytes_per_pixel)
{
    uint8_t cmd[RI_CMD_L];
    SANE_Bool endiantype;
    SANE_Status status;
    size_t size;
    size_t i;
    uint8_t tmp;


    DBG(30, "scsi_read_image:  ms=%p, buffer=%p\n", (void *) ms, buffer);

    ENDIAN_TYPE(endiantype)
    RI_SET_CMD(cmd);
    RI_SET_PCORMAC(cmd, endiantype);
    RI_SET_COLOR(cmd, ms->current_read_color);
    RI_SET_TRANSFERLENGTH(cmd, ms->transfer_length);

    DBG(30, "scsi_read_image: transferlength=%d\n", ms->transfer_length);

    if ( md_dump >= 2 )
        dump_area2(cmd, RI_CMD_L, "readimagecmd");

    size = ms->transfer_length;
    status = sanei_scsi_cmd(ms->sfd, cmd, sizeof(cmd), buffer, &size);

    if ( buffer && ( ms->dev->model_flags & MD_PHANTOM_C6 ) && endiantype )
      {
	switch(bytes_per_pixel)
	  {
	    case 1: break;
	    case 2:
		    for ( i = 1; i < size; i += 2 )
		      {
			tmp = buffer[i-1];
			buffer[i-1] = buffer[i];
			buffer[i] = tmp;
		      }
		    break;
	    default:
		    DBG(1, "scsi_read_image: Unexpected bytes_per_pixel=%d\n", bytes_per_pixel);
	  }
      }

    if ( status != SANE_STATUS_GOOD )
        DBG(1, "scsi_read_image: '%s'\n", sane_strstatus(status));

    if ( md_dump > 3 )
        dump_area2(buffer, ms->transfer_length, "readimageresult");

    return status;
}


/*---------- scsi_read_image_status() ----------------------------------------*/

static SANE_Status
scsi_read_image_status(Microtek2_Scanner *ms)
{
    Microtek2_Device *md;
    Microtek2_Info *mi;
    uint8_t cmd[RIS_CMD_L];
    uint8_t dummy;
    size_t dummy_length;
    SANE_Status status;
    SANE_Bool endian_type;

    md = ms->dev;
    mi = &md->info[md->scan_source];

    DBG(30, "scsi_read_image_status: ms=%p\n", (void *) ms);

    ENDIAN_TYPE(endian_type)
    RIS_SET_CMD(cmd);
    RIS_SET_PCORMAC(cmd, endian_type);
    RIS_SET_COLOR(cmd, ms->current_read_color);

/*    mi->new_image_status = SANE_TRUE;  */  /* for testing*/

    if ( mi->new_image_status == SANE_TRUE )
      {
        DBG(30, "scsi_read_image_status: use new image status \n");
        dummy_length = 1;
        cmd[8] = 1;
      }
    else
      {
        DBG(30, "scsi_read_image_status: use old image status \n");
        dummy_length = 0;
        cmd[8] = 0;
      }

    if ( md_dump >= 2 )
        dump_area2(cmd, sizeof(cmd), "readimagestatus");

    status = sanei_scsi_cmd(ms->sfd, cmd, sizeof(cmd), &dummy, &dummy_length);

    if ( mi->new_image_status == SANE_TRUE )
      {
        if ( dummy == 0 )
            status = SANE_STATUS_GOOD;
        else
            status = SANE_STATUS_DEVICE_BUSY;
      }

        /* For some (X6USB) scanner
        We say we are going to try to read 1 byte of data (as recommended
        in the Microtek SCSI command documentation under "New Image Status")
        so that dubious SCSI host adapters (like the one in at least some
        Microtek X6 USB scanners) don't get wedged trying to do a zero
        length read. However, we do not actually try to read this byte of
        data, as that wedges the USB scanner as well.
        IOW the SCSI command says we are going to read 1 byte, but in fact
        we don't: */
        /*cmd[8] = 1;
        status = sanei_scsi_cmd(ms->sfd, cmd, sizeof(cmd), &dummy, 0); */


    if ( status != SANE_STATUS_GOOD )
        DBG(1, "scsi_read_image_status: '%s'\n", sane_strstatus(status));

    return status;
}

/*---------- scsi_read_shading () --------------------------------------------*/

static SANE_Status
scsi_read_shading(Microtek2_Scanner *ms, uint8_t *buffer, uint32_t length)
{
    Microtek2_Device *md;
    uint8_t cmd[RSI_CMD_L];
    SANE_Bool endiantype;
    SANE_Status status = SANE_STATUS_GOOD;
    size_t size;

    DBG(30, "scsi_read_shading: pos=%p, size=%d, word=%d, color=%d, dark=%d\n",
             buffer, length, ms->word, ms->current_color, ms->dark);

    md = ms->dev;

    size = length;

    RSI_SET_CMD(cmd);
    ENDIAN_TYPE(endiantype)
    RSI_SET_PCORMAC(cmd, endiantype);
    RSI_SET_COLOR(cmd, ms->current_color);
    RSI_SET_DARK(cmd, ms->dark);
    RSI_SET_WORD(cmd, ms->word);
    RSI_SET_TRANSFERLENGTH(cmd, size);

    if ( md_dump >= 2 )
        dump_area2(cmd, RSI_CMD_L, "readshading");

    DBG(100, "scsi_read_shading: sfd=%d, cmd=%p, sizeofcmd=%lu,"
             "dest=%p, destsize=%lu\n",
              ms->sfd, cmd, (u_long) sizeof(cmd), buffer, (u_long) size);

    status = sanei_scsi_cmd(ms->sfd, cmd, sizeof(cmd), buffer, &size);
    if ( status != SANE_STATUS_GOOD )
        DBG(1, "scsi_read_shading: '%s'\n", sane_strstatus(status));

    if ( md_dump > 3)
        dump_area2(buffer,
                   size,
                   "readshadingresult");

    return status;
}


/*---------- scsi_send_shading () --------------------------------------------*/

static SANE_Status
scsi_send_shading(Microtek2_Scanner *ms,
                  uint8_t *shading_data,
                  uint32_t length,
                  uint8_t dark)
{
    SANE_Bool endiantype;
    SANE_Status status;
    size_t size;
    uint8_t *cmd;


    DBG(30, "scsi_send_shading: pos=%p, size=%d, word=%d, color=%d, dark=%d\n",
             shading_data, length, ms->word, ms->current_color,
             dark);

    cmd = (uint8_t *) malloc(SSI_CMD_L + length);
    DBG(100, "scsi_send_shading: cmd=%p, malloc'd %d bytes\n",
              cmd, SSI_CMD_L + length);
    if ( cmd == NULL )
      {
        DBG(1, "scsi_send_shading: Couldn't get buffer for shading table\n");
        return SANE_STATUS_NO_MEM;
      }

    SSI_SET_CMD(cmd);
    ENDIAN_TYPE(endiantype)
    SSI_SET_PCORMAC(cmd, endiantype);
    SSI_SET_COLOR(cmd, ms->current_color);
    SSI_SET_DARK(cmd, dark);
    SSI_SET_WORD(cmd, ms->word);
    SSI_SET_TRANSFERLENGTH(cmd, length);
    memcpy(cmd + SSI_CMD_L, shading_data, length);
    size = length;

    if ( md_dump >= 2 )
        dump_area2(cmd, SSI_CMD_L, "sendshading");
    if ( md_dump >= 3 )
        dump_area2(cmd + SSI_CMD_L, size, "sendshadingdata");

    status = sanei_scsi_cmd(ms->sfd, cmd, size + SSI_CMD_L, NULL, 0);
    if ( status != SANE_STATUS_GOOD )
        DBG(1, "scsi_send_shading: '%s'\n", sane_strstatus(status));

    DBG(100, "free cmd at %p\n", cmd);
    free((void *) cmd);

    return status;

}


/*---------- scsi_read_system_status() ---------------------------------------*/

static SANE_Status
scsi_read_system_status(Microtek2_Device *md, int fd)
{
    uint8_t cmd[RSS_CMD_L];
    uint8_t result[RSS_RESULT_L];
    int sfd;
    size_t size;
    SANE_Status status;

    DBG(30, "scsi_read_system_status: md=%p, fd=%d\n", (void *) md, fd);

    if ( fd == -1 )
      {
        status = sanei_scsi_open(md->name, &sfd, scsi_sense_handler, 0);
        if ( status != SANE_STATUS_GOOD )
          {
            DBG(1, "scsi_read_system_status: open '%s'\n",
                    sane_strstatus(status));
            return status;
          }
      }
    else
      sfd = fd;

    RSS_CMD(cmd);

    if ( md_dump >= 2)
        dump_area2(cmd, RSS_CMD_L, "readsystemstatus");

    size = sizeof(result);
    status = sanei_scsi_cmd(sfd, cmd, sizeof(cmd), result, &size);

    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "scsi_read_system_status: cmd '%s'\n", sane_strstatus(status));
        sanei_scsi_close(sfd);
        return status;
      }

    if ( fd == -1 )
        sanei_scsi_close(sfd);

    if ( md_dump >= 2)
        dump_area2(result, size, "readsystemstatusresult");

    md->status.sskip = RSS_SSKIP(result);
    md->status.ntrack = RSS_NTRACK(result);
    md->status.ncalib = RSS_NCALIB(result);
    md->status.tlamp = RSS_TLAMP(result);
    md->status.flamp = RSS_FLAMP(result);
    md->status.rdyman= RSS_RDYMAN(result);
    md->status.trdy = RSS_TRDY(result);
    md->status.frdy = RSS_FRDY(result);
    md->status.adp = RSS_RDYMAN(result);
    md->status.detect = RSS_DETECT(result);
    md->status.adptime = RSS_ADPTIME(result);
    md->status.lensstatus = RSS_LENSSTATUS(result);
    md->status.aloff = RSS_ALOFF(result);
    md->status.timeremain = RSS_TIMEREMAIN(result);
    md->status.tmacnt = RSS_TMACNT(result);
    md->status.paper = RSS_PAPER(result);
    md->status.adfcnt = RSS_ADFCNT(result);
    md->status.currentmode = RSS_CURRENTMODE(result);
    md->status.buttoncount = RSS_BUTTONCOUNT(result);

    return SANE_STATUS_GOOD;
}


/*---------- scsi_request_sense() --------------------------------------------*/

/* currently not used */

#if 0

static SANE_Status
scsi_request_sense(Microtek2_Scanner *ms)
{
    uint8_t requestsense[RQS_CMD_L];
    uint8_t buffer[100];
    SANE_Status status;
    int size;
    int asl;
    int as_info_length;

    DBG(30, "scsi_request_sense: ms=%p\n", (void *) ms);

    RQS_CMD(requestsense);
    RQS_ALLOCLENGTH(requestsense, 100);

    size = sizeof(buffer);
    status = sanei_scsi_cmd(ms->sfd,  requestsense, sizeof(requestsense),
                            buffer, &size);

    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "scsi_request_sense: '%s'\n", sane_strstatus(status));
        return status;
      }

    if ( md_dump >= 2 )
        dump_area2(buffer, size, "requestsenseresult");

    dump_area(buffer, RQS_LENGTH(buffer), "RequestSense");
    asl = RQS_ASL(buffer);
    if ( (as_info_length = RQS_ASINFOLENGTH(buffer)) > 0 )
        DBG(25, "scsi_request_sense: info '%.*s'\n",
                as_info_length, RQS_ASINFO(buffer));

    return SANE_STATUS_GOOD;
}
#endif


/*---------- scsi_send_system_status() ---------------------------------------*/

static SANE_Status
scsi_send_system_status(Microtek2_Device *md, int fd)
{
    uint8_t cmd[SSS_CMD_L + SSS_DATA_L];
    uint8_t *pos;
    int sfd;
    SANE_Status status;


    DBG(30, "scsi_send_system_status: md=%p, fd=%d\n", (void *) md, fd);

    memset(cmd, 0, SSS_CMD_L + SSS_DATA_L);
    if ( fd == -1 )
      {
        status = sanei_scsi_open(md->name, &sfd, scsi_sense_handler, 0);
        if ( status != SANE_STATUS_GOOD )
          {
            DBG(1, "scsi_send_system_status: open '%s'\n",
                    sane_strstatus(status));
            return status;
          }
      }
    else
      sfd = fd;

    SSS_CMD(cmd);
    pos = cmd + SSS_CMD_L;
    SSS_STICK(pos, md->status.stick);
    SSS_NTRACK(pos, md->status.ntrack);
    SSS_NCALIB(pos, md->status.ncalib);
    SSS_TLAMP(pos, md->status.tlamp);
    SSS_FLAMP(pos, md->status.flamp);
    SSS_RESERVED17(pos, md->status.reserved17);
    SSS_RDYMAN(pos, md->status.rdyman);
    SSS_TRDY(pos, md->status.trdy);
    SSS_FRDY(pos, md->status.frdy);
    SSS_ADP(pos, md->status.adp);
    SSS_DETECT(pos, md->status.detect);
    SSS_ADPTIME(pos, md->status.adptime);
    SSS_LENSSTATUS(pos, md->status.lensstatus);
    SSS_ALOFF(pos, md->status.aloff);
    SSS_TIMEREMAIN(pos, md->status.timeremain);
    SSS_TMACNT(pos, md->status.tmacnt);
    SSS_PAPER(pos, md->status.paper);
    SSS_ADFCNT(pos, md->status.adfcnt);
    SSS_CURRENTMODE(pos, md->status.currentmode);
    SSS_BUTTONCOUNT(pos, md->status.buttoncount);

    if ( md_dump >= 2)
      {
        dump_area2(cmd, SSS_CMD_L, "sendsystemstatus");
        dump_area2(cmd + SSS_CMD_L, SSS_DATA_L, "sendsystemstatusdata");
      }

    status = sanei_scsi_cmd(sfd, cmd, sizeof(cmd), NULL, 0);
    if ( status != SANE_STATUS_GOOD )
        DBG(1, "scsi_send_system_status: '%s'\n", sane_strstatus(status));

    if ( fd == -1 )
        sanei_scsi_close(sfd);
    return status;
}


/*---------- scsi_sense_handler() --------------------------------------------*/
/* rewritten 19.12.2001 for better SANE_STATUS return codes */

static SANE_Status
scsi_sense_handler (int fd, u_char *sense, void *arg)
{
    int as_info_length;
    uint8_t sense_key;
    uint8_t asl;
    uint8_t asc;
    uint8_t ascq;


    DBG(30, "scsi_sense_handler: fd=%d, sense=%p arg=%p\n",fd, sense, arg);

    dump_area(sense, RQS_LENGTH(sense), "SenseBuffer");

    sense_key = RQS_SENSEKEY(sense);
    asl = RQS_ASL(sense);
    asc = RQS_ASC(sense);
    ascq = RQS_ASCQ(sense);

    DBG(5, "scsi_sense_handler: SENSE KEY (0x%02x), "
           "ASC (0x%02x), ASCQ (0x%02x)\n", sense_key, asc, ascq);

    if ( (as_info_length = RQS_ASINFOLENGTH(sense)) > 0 )
        DBG(5,"scsi_sense_handler: info: '%*s'\n",
                as_info_length, RQS_ASINFO(sense));

    switch ( sense_key )
      {
        case RQS_SENSEKEY_NOSENSE:
          return SANE_STATUS_GOOD;

        case RQS_SENSEKEY_HWERR:
        case RQS_SENSEKEY_ILLEGAL:
        case RQS_SENSEKEY_VENDOR:
          if ( asc == 0x4a && ascq == 0x00 )
              DBG(5, "scsi_sense_handler: Command phase error\n");
          else if ( asc == 0x2c && ascq == 0x00 )
              DBG(5, "scsi_sense_handler: Command sequence error\n");
          else if ( asc == 0x4b && ascq == 0x00 )
              DBG(5, "scsi_sense_handler: Data phase error\n");
          else if ( asc == 0x40 )
            {
              DBG(5, "scsi_sense_handler: Hardware diagnostic failure:\n");
              switch ( ascq )
                {
                  case RQS_ASCQ_CPUERR:
                    DBG(5, "scsi_sense_handler: CPU error\n");
                    break;
                  case RQS_ASCQ_SRAMERR:
                    DBG(5, "scsi_sense_handler: SRAM error\n");
                    break;
                  case RQS_ASCQ_DRAMERR:
                    DBG(5, "scsi_sense_handler: DRAM error\n");
                    break;
                  case RQS_ASCQ_DCOFF:
                    DBG(5, "scsi_sense_handler: DC Offset error\n");
                    break;
                  case RQS_ASCQ_GAIN:
                    DBG(5, "scsi_sense_handler: Gain error\n");
                    break;
                  case RQS_ASCQ_POS:
                    DBG(5, "scsi_sense_handler: Positoning error\n");
                    break;
                  default:
                    DBG(5, "scsi_sense_handler: Unknown combination of ASC"
                           " (0x%02x) and ASCQ (0x%02x)\n", asc, ascq);
                    break;
                }
            }
          else if ( asc == 0x00  && ascq == 0x05)
            {
              DBG(5, "scsi_sense_handler: End of data detected\n");
              return SANE_STATUS_EOF;
            }
          else if ( asc == 0x3d  && ascq == 0x00)
              DBG(5, "scsi_sense_handler: Invalid bit in IDENTIFY\n");
          else if ( asc == 0x2c && ascq == 0x02 )
/* Ok */      DBG(5, "scsi_sense_handler: Invalid comb. of windows specfied\n");
          else if ( asc == 0x20 && ascq == 0x00 )
/* Ok */      DBG(5, "scsi_sense_handler: Invalid command opcode\n");
          else if ( asc == 0x24 && ascq == 0x00 )
/* Ok */      DBG(5, "scsi_sense_handler: Invalid field in CDB\n");
          else if ( asc == 0x26 && ascq == 0x00 )
              DBG(5, "scsi_sense_handler: Invalid field in the param list\n");
          else if ( asc == 0x49 && ascq == 0x00 )
              DBG(5, "scsi_sense_handler: Invalid message error\n");
          else if ( asc == 0x60 && ascq == 0x00 )
              DBG(5, "scsi_sense_handler: Lamp failure\n");
          else if ( asc == 0x25 && ascq == 0x00 )
              DBG(5, "scsi_sense_handler: Unsupported logic. unit\n");
          else if ( asc == 0x53 && ascq == 0x00 )
            {
              DBG(5, "scsi_sense_handler: ADF paper jam or no paper\n");
              return SANE_STATUS_NO_DOCS;
            }
          else if ( asc == 0x54 && ascq == 0x00 )
            {
              DBG(5, "scsi_sense_handler: Media bumping\n");
              return SANE_STATUS_JAMMED; /* Don't know if this is right! */
            }
          else if ( asc == 0x55 && ascq == 0x00 )
            {
              DBG(5, "scsi_sense_handler: Scan Job stopped or cancelled\n");
              return SANE_STATUS_CANCELLED;
            }
          else if ( asc == 0x3a && ascq == 0x00 )
            {
              DBG(5, "scsi_sense_handler: Media (ADF or TMA) not available\n");
              return SANE_STATUS_NO_DOCS;
            }
          else if ( asc == 0x3a && ascq == 0x01 )
            {
              DBG(5, "scsi_sense_handler: Door is not closed\n");
              return SANE_STATUS_COVER_OPEN;
            }
          else if ( asc == 0x3a && ascq == 0x02 )
              DBG(5, "scsi_sense_handler: Door is not opened\n");
          else if ( asc == 0x00 && ascq == 0x00 )
              DBG(5, "scsi_sense_handler:  No additional sense information\n");
/* Ok */  else if ( asc == 0x1a && ascq == 0x00 )
              DBG(5, "scsi_sense_handler: Parameter list length error\n");
          else if ( asc == 0x26 && ascq == 0x02 )
              DBG(5, "scsi_sense_handler: Parameter value invalid\n");
          else if ( asc == 0x03 && ascq == 0x00 )
              DBG(5, "scsi_sense_handler: Peripheral device write fault - "
                     "Firmware Download Error\n");
          else if ( asc == 0x2c && ascq == 0x01 )
              DBG(5, "scsi_sense_handler: Too many windows specified\n");
          else if ( asc == 0x80 && ascq == 0x00 )
              DBG(5, "scsi_sense_handler: Target abort scan\n");
          else if ( asc == 0x96 && ascq == 0x08 )
            {
              DBG(5, "scsi_sense_handler: Firewire Device busy\n");
              return SANE_STATUS_DEVICE_BUSY;
            }
          else
              DBG(5, "scsi_sense_handler: Unknown combination of SENSE KEY "
                     "(0x%02x), ASC (0x%02x) and ASCQ (0x%02x)\n",
                      sense_key, asc, ascq);

          return SANE_STATUS_IO_ERROR;

        default:
           DBG(5, "scsi_sense_handler: Unknown sense key (0x%02x)\n",
                   sense_key);
           return SANE_STATUS_IO_ERROR;
    }
}


/*---------- scsi_test_unit_ready() ------------------------------------------*/

static SANE_Status
scsi_test_unit_ready(Microtek2_Device *md)
{
    SANE_Status status;
    uint8_t tur[TUR_CMD_L];
    int sfd;


    DBG(30, "scsi_test_unit_ready: md=%s\n", md->name);

    TUR_CMD(tur);
    status = sanei_scsi_open(md->name, &sfd, scsi_sense_handler, 0);
    if ( status != SANE_STATUS_GOOD )
      {
	DBG(1, "scsi_test_unit_ready: open '%s'\n", sane_strstatus(status));
	return status;
      }

    if ( md_dump >= 2 )
        dump_area2(tur, sizeof(tur), "testunitready");

    status = sanei_scsi_cmd(sfd, tur, sizeof(tur), NULL, 0);
    if ( status != SANE_STATUS_GOOD )
        DBG(1, "scsi_test_unit_ready: cmd '%s'\n", sane_strstatus(status));
      
    sanei_scsi_close(sfd); 
    return status;
}


/*---------- sane_start() ----------------------------------------------------*/

SANE_Status
sane_start(SANE_Handle handle)
{
    SANE_Status status = SANE_STATUS_GOOD;
    Microtek2_Scanner *ms = handle;
    Microtek2_Device *md;
    Microtek2_Info *mi;
    uint8_t *pos;
    int color, rc, retry;

    DBG(30, "sane_start: handle=0x%p\n", handle);

    md = ms->dev;
    mi = &md->info[md->scan_source];
    ms->n_control_bytes = md->n_control_bytes;

    if ( md->model_flags & MD_READ_CONTROL_BIT )
      {
        if (ms->control_bytes) free((void *)ms->control_bytes);
        ms->control_bytes = (uint8_t *) malloc(ms->n_control_bytes);
        DBG(100, "sane_start: ms->control_bytes=%p, malloc'd %lu bytes\n",
                             ms->control_bytes, (u_long) ms->n_control_bytes);
        if ( ms->control_bytes == NULL )
          {
            DBG(1, "sane_start: malloc() for control bits failed\n");
            status = SANE_STATUS_NO_MEM;
            goto cleanup;
          }
      }

    if (ms->sfd < 0) /* first or only pass of this scan */
      {
        /* open device */
        for ( retry = 0; retry < 10; retry++ )
	  {
            status = sanei_scsi_open (md->sane.name, &ms->sfd,
                                      scsi_sense_handler, 0);
            if ( status != SANE_STATUS_DEVICE_BUSY )
	      break;
            DBG(30, "sane_start: Scanner busy, trying again\n");
            sleep(1);
	  }
        if ( status != SANE_STATUS_GOOD )
          {
            DBG(1, "sane_start: scsi_open: '%s'\n", sane_strstatus(status));
            goto cleanup;
          }

        status = scsi_read_system_status(md, ms->sfd);
        if ( status != SANE_STATUS_GOOD )
            goto cleanup;

        if ( ms->val[OPT_CALIB_BACKEND].w == SANE_TRUE )
            DBG(30, "sane_start: backend calibration on\n");
        else
            DBG(30, "sane_start: backend calibration off\n");

        if ( ( ms->val[OPT_CALIB_BACKEND].w == SANE_TRUE )
             && !( md->model_flags & MD_PHANTOM336CX_TYPE_SHADING ) )
          {
	    /* Read shading only once - possible with CIS scanners */
	    /* assuming only CIS scanners use Controlbits */
	    if ( ( md->shading_table_w == NULL )
                 || !( md->model_flags & MD_READ_CONTROL_BIT ) )
              {
		status = get_scan_parameters(ms);
                if ( status != SANE_STATUS_GOOD )
                        goto cleanup;

                status = read_shading_image(ms);
                if ( status != SANE_STATUS_GOOD )
                        goto cleanup;
              }
          }
    
	status = get_scan_parameters(ms);
        if ( status != SANE_STATUS_GOOD )
            goto cleanup;

        status = scsi_read_system_status(md, ms->sfd);
        if ( status != SANE_STATUS_GOOD )
            goto cleanup;

        md->status.aloff |= 128;
        md->status.timeremain = 10;

        if ( ms->scan_source == MS_SOURCE_FLATBED
             || ms->scan_source == MS_SOURCE_ADF )
          {
            md->status.flamp |= MD_FLAMP_ON;
            md->status.tlamp &= ~MD_TLAMP_ON;
          }
        else
          {
            md->status.flamp &= ~MD_FLAMP_ON;
            md->status.tlamp |= MD_TLAMP_ON;
          }

        if ( ms->lightlid35 )
          {
            md->status.flamp &= ~MD_FLAMP_ON;
/*            md->status.tlamp |= MD_TLAMP_ON;*/
/* with this line on some scanners (X6, 0x91) the Flamp goes on  */
          }
          
        if ( ms->no_backtracking )
            md->status.ntrack |= MD_NTRACK_ON;
        else
            md->status.ntrack &= ~MD_NTRACK_ON;

        status = scsi_send_system_status(md, ms->sfd);
        if ( status != SANE_STATUS_GOOD )
            goto cleanup;

        /* calculate gamma: we assume, that the gamma values are transferred */
        /* with one send gamma command, even if it is a 3 pass scanner */
        if ( md->model_flags & MD_NO_GAMMA )
          {
            ms->lut_size = (int) pow(2.0, (double) ms->depth);
            ms->lut_entry_size = ms->depth > 8 ? 2 : 1;
          }
        else
          {
            get_lut_size(mi, &ms->lut_size, &ms->lut_entry_size);
          }
        ms->lut_size_bytes = ms->lut_size * ms->lut_entry_size;
        ms->word = (ms->lut_entry_size == 2);

        ms->gamma_table = (uint8_t *) malloc(3 * ms->lut_size_bytes );
        DBG(100, "sane_start: ms->gamma_table=%p, malloc'd %d bytes\n",
                  ms->gamma_table, 3 * ms->lut_size_bytes);
        if ( ms->gamma_table == NULL )
          {
            DBG(1, "sane_start: malloc for gammatable failed\n");
            status = SANE_STATUS_NO_MEM;
            goto cleanup;
          }
        for ( color = 0; color < 3; color++ )
          {
            pos = ms->gamma_table + color * ms->lut_size_bytes;
            calculate_gamma(ms, pos, color, ms->gamma_mode);
          }

        /* Some models ignore the settings for the exposure time, */
        /* so we must do it ourselves. Apparently this seems to be */
        /* the case for all models that have the chunky data format */

        if ( mi->data_format == MI_DATAFMT_CHUNKY )
            set_exposure(ms);

        if ( ! (md->model_flags & MD_NO_GAMMA) )
          {
            status = scsi_send_gamma(ms);
            if ( status != SANE_STATUS_GOOD )
                goto cleanup;
          }

        status = scsi_set_window(ms, 1);
        if ( status != SANE_STATUS_GOOD )
            goto cleanup;

        ms->scanning = SANE_TRUE;
        ms->cancelled = SANE_FALSE;
      }

    ++ms->current_pass;

    status = scsi_read_image_info(ms);
    if ( status != SANE_STATUS_GOOD )
        goto cleanup;

    status = prepare_buffers(ms);
    if ( status != SANE_STATUS_GOOD )
        goto cleanup;

    status = calculate_sane_params(ms);
    if ( status != SANE_STATUS_GOOD )
        goto cleanup;

    if ( !( md->model_flags & MD_NO_RIS_COMMAND ) )
      {
        /* !!FIXME!! - hack for C6USB because RIS over USB doesn't wait until */
        /* scanner ready */
        if (mi->model_code == 0x9a)
            sleep(2);

        status = scsi_wait_for_image(ms);
        if ( status  != SANE_STATUS_GOOD )
            goto cleanup;
      }

    if ( ms->calib_backend
         && ( md->model_flags & MD_PHANTOM336CX_TYPE_SHADING )
         && ( ( md->shading_table_w == NULL )
              || ( ms->mode != md->shading_table_contents )
            )
       )
      {
        status = read_cx_shading(ms);
        if ( status  != SANE_STATUS_GOOD )
	  goto cleanup;
      }
        
    if ( ms->lightlid35 )  
    /* hopefully this leads to a switched off flatbed lamp with lightlid */
      {
        status = scsi_read_system_status(md, ms->sfd);
        if ( status != SANE_STATUS_GOOD )
            goto cleanup;

        md->status.flamp &= ~MD_FLAMP_ON;
        md->status.tlamp &= ~MD_TLAMP_ON;
      
        status = scsi_send_system_status(md, ms->sfd);
        if ( status != SANE_STATUS_GOOD )
            goto cleanup;
      }

    if ( md->model_flags & MD_READ_CONTROL_BIT )
      {
        status = scsi_read_control_bits(ms);
        if ( status != SANE_STATUS_GOOD )
            goto cleanup;

        if ( ms->calib_backend )
          {
            status = condense_shading(ms);
            if ( status != SANE_STATUS_GOOD )
               goto cleanup;
          }
      }

    /* open a pipe and fork a child process, that actually reads the data */
    rc = pipe(ms->fd);
    if ( rc == -1 )
      {
        DBG(1, "sane_start: pipe failed\n");
        status = SANE_STATUS_IO_ERROR;
        goto cleanup;
      }

    /* create reader routine as new thread or process */
    ms->pid = sanei_thread_begin( reader_process,(void*) ms);

    if ( ms->pid == -1 )
      {
        DBG(1, "sane_start: fork failed\n");
        status = SANE_STATUS_IO_ERROR;
        goto cleanup;
      }

    if (sanei_thread_is_forked()) close(ms->fd[1]);

    return SANE_STATUS_GOOD;

cleanup:
    cleanup_scanner(ms);
    return status;
}

/*---------- prepare_buffers -------------------------------------------------*/

static SANE_Status
prepare_buffers(Microtek2_Scanner *ms)
{
    SANE_Status status;
    Microtek2_Device *md;
    Microtek2_Info *mi;
    uint32_t strip_lines;
    int i;

    status = SANE_STATUS_GOOD;
    DBG(30, "prepare_buffers: ms=0x%p\n", (void *) ms);
   
    md = ms->dev;
    mi = &md->info[md->scan_source];

    /* calculate maximum number of lines to read */
    strip_lines = (int) ((double) ms->y_resolution_dpi * md->opts.strip_height);
    if ( strip_lines == 0 )
        strip_lines = 1;

    /* calculate number of lines that fit into the source buffer */
#ifdef TESTBACKEND
    ms->src_max_lines = MIN( 5000000 / ms->bpl, strip_lines);
#else
    ms->src_max_lines = MIN( sanei_scsi_max_request_size / ms->bpl, strip_lines);
#endif
    if ( ms->src_max_lines == 0 )
      {
        DBG(1, "sane_start: Scan buffer too small\n");
        status = SANE_STATUS_IO_ERROR;
        goto cleanup;
      }

    /* allocate buffers */
    ms->src_buffer_size = ms->src_max_lines * ms->bpl;       

    if ( ms->mode == MS_MODE_COLOR && mi->data_format == MI_DATAFMT_LPLSEGREG ) 
      {             
        /* In this case the data is not neccessarily in the order RGB */
        /* and there may be different numbers of read red, green and blue */
        /* segments. We allocate a second buffer to read new lines in */
        /* and hold undelivered pixels in the other buffer */
        int extra_buf_size;

        extra_buf_size = 2 * ms->bpl * mi->ccd_gap 
                         * (int) ceil( (double) mi->max_yresolution
                                      / (double) mi->opt_resolution); 
        for ( i = 0; i < 2; i++ )
          {
            if ( ms->buf.src_buffer[i] )
                free((void *) ms->buf.src_buffer[i]);
            ms->buf.src_buffer[i] = (uint8_t *) malloc(ms->src_buffer_size
                                    + extra_buf_size);
            DBG(100, "prepare_buffers: ms->buf.src_buffer[%d]=%p,"
                     "malloc'd %d bytes\n", i, ms->buf.src_buffer[i],
                     ms->src_buffer_size + extra_buf_size);
            if ( ms->buf.src_buffer[i] == NULL )
              {
                DBG(1, "sane_start: malloc for scan buffer failed\n");
                status = SANE_STATUS_NO_MEM;
                goto cleanup;
              }
          }
        ms->buf.free_lines = ms->src_max_lines + extra_buf_size / ms->bpl;
        ms->buf.free_max_lines = ms->buf.free_lines;
        ms->buf.src_buf = ms->buf.src_buffer[0];
        ms->buf.current_src = 0;         /* index to current buffer */
      }
    else
      {
        if ( ms->buf.src_buf )
            free((void *) ms->buf.src_buf);
        ms->buf.src_buf = malloc(ms->src_buffer_size);
        DBG(100, "sane_start: ms->buf.src_buf=%p, malloc'd %d bytes\n",
                            ms->buf.src_buf, ms->src_buffer_size);
        if ( ms->buf.src_buf == NULL )
          {
            DBG(1, "sane_start: malloc for scan buffer failed\n");
            status = SANE_STATUS_NO_MEM;
            goto cleanup;
          }
      }

    for ( i = 0; i < 3; i++ )
      {
        ms->buf.current_pos[i] = ms->buf.src_buffer[0];
        ms->buf.planes[0][i] = 0;
        ms->buf.planes[1][i] = 0;
      }

    /* allocate a temporary buffer for the data, if auto_adjust threshold */
    /* is selected. */

    if ( ms->auto_adjust == 1 )
      {
        ms->temporary_buffer = (uint8_t *) malloc(ms->remaining_bytes);
        DBG(100, "sane_start: ms->temporary_buffer=%p, malloc'd %d bytes\n",
                  ms->temporary_buffer, ms->remaining_bytes);
        if ( ms->temporary_buffer == NULL )
          {
            DBG(1, "sane_start: malloc() for temporary buffer failed\n");
            status = SANE_STATUS_NO_MEM;
            goto cleanup;
          } 
      }
    else 
        ms->temporary_buffer = NULL;

    /* some data formats have additional information in a scan line, which */
    /* is not transferred to the frontend; real_bpl is the number of bytes */
    /* per line, that is copied into the frontend's buffer */
    ms->real_bpl = (uint32_t) ceil( ((double) ms->ppl * 
                                      (double) ms->bits_per_pixel_out) / 8.0 );
    if ( mi->onepass && ms->mode == MS_MODE_COLOR )
        ms->real_bpl *= 3;

    ms->real_remaining_bytes = ms->real_bpl * ms->src_remaining_lines;

    return SANE_STATUS_GOOD;

cleanup:
    cleanup_scanner(ms);
    return status; 

}
static void
write_shading_buf_pnm(Microtek2_Scanner *ms, uint32_t lines)
{
  FILE *outfile;
  uint16_t pixel, color, linenr, factor;
  unsigned char  img_val_out;
  float img_val = 0;
  Microtek2_Device *md;
  Microtek2_Info *mi;

  md = ms->dev;
  mi = &md->info[md->scan_source];

  if ( mi->depth & MI_HASDEPTH_16 )
      factor = 256;
  else if ( mi->depth & MI_HASDEPTH_14 )
      factor = 64;
  else if ( mi->depth & MI_HASDEPTH_12 )
      factor = 16;
  else if ( mi->depth & MI_HASDEPTH_10 )
      factor = 4;
  else
      factor = 1;
  if ( md->model_flags & MD_16BIT_TRANSFER )
      factor = 256;

  outfile = fopen("shading_buf_w.pnm", "w");
  fprintf(outfile, "P6\n#imagedata\n%d %d\n255\n",
          mi->geo_width / mi->calib_divisor, lines);
  for ( linenr=0; linenr < lines; linenr++ )
    {
      if (mi->data_format == MI_DATAFMT_LPLSEGREG)
        {
	  DBG(1, "Output of shading buffer unsupported for"
                           "Segreg Data format\n");
          break;
        }

      for ( pixel=0;
            pixel < (uint16_t) (mi->geo_width / mi->calib_divisor);
            pixel++)
        {
          for ( color=0; color < 3; color++ )
            {
              switch( mi->data_format )
                {
                  case MI_DATAFMT_LPLCONCAT:
                    if ( md->shading_depth > 8)
                      img_val = *((uint16_t *) ms->shading_image
                                 + linenr * ( ms->bpl / ms->lut_entry_size )
                                 + mi->color_sequence[color]
                                      * ( ms->bpl / ms->lut_entry_size / 3 )
                                 + pixel);
                    else
                      img_val = *((uint8_t *) ms->shading_image
                                 + linenr * ( ms->bpl / ms->lut_entry_size )
                                 + mi->color_sequence[color]
                                      * ( ms->bpl / ms->lut_entry_size / 3 )
                                 + pixel);
                      
                    break;
                  case MI_DATAFMT_CHUNKY:
                  case MI_DATAFMT_9800:
                    img_val = *((uint16_t *)ms->shading_image
                               + linenr * 3 * ( mi->geo_width
                                                / mi->calib_divisor )
                               + 3 * pixel
                               + mi->color_sequence[color]);
                    break;
                }
              img_val /= factor;
              img_val_out = (unsigned char)img_val;
              fputc(img_val_out, outfile);
            }
        }
    }
  fclose(outfile);

  return;
}

static void
write_shading_pnm(Microtek2_Scanner *ms)
{
  FILE *outfile_w = NULL, *outfile_d = NULL;
  int pixel, color, line, offset, num_shading_pixels, output_height;
  uint16_t img_val, factor;

  Microtek2_Device *md;
  Microtek2_Info *mi;

  output_height = 180;
  md = ms->dev;
  mi = &md->info[md->scan_source];

  DBG(30, "write_shading_pnm: ms=%p\n", (void *) ms);

  if ( mi->depth & MI_HASDEPTH_16 )
      factor = 256;
  else if ( mi->depth & MI_HASDEPTH_14 )
      factor = 64;
  else if ( mi->depth & MI_HASDEPTH_12 )
      factor = 16;
  else if ( mi->depth & MI_HASDEPTH_10 )
      factor = 4;
  else
      factor = 1;
  if ( md->model_flags & MD_16BIT_TRANSFER )
      factor = 256;

  if ( md->model_flags & MD_PHANTOM336CX_TYPE_SHADING )
    num_shading_pixels = ms->n_control_bytes * 8;
  else
    num_shading_pixels = mi->geo_width / mi->calib_divisor;
  if ( md->shading_table_w != NULL )
    {
      outfile_w = fopen("microtek2_shading_w.pnm", "w");
      fprintf(outfile_w, "P6\n#imagedata\n%d %d\n255\n",
                      num_shading_pixels, output_height);
    }
  if ( md->shading_table_d != NULL )
    {
      outfile_d = fopen("microtek2_shading_d.pnm", "w");
      fprintf(outfile_d, "P6\n#imagedata\n%d %d\n255\n",
                      num_shading_pixels, output_height);
    }
  for ( line=0; line < output_height; ++line )
    {
      for ( pixel=0; pixel < num_shading_pixels ; ++pixel)
        {
          for ( color=0; color < 3; ++color )
            {
              offset = mi->color_sequence[color]
                        * num_shading_pixels
                        + pixel;
              if ( md->shading_table_w != NULL )
                {
                  if ( ms->lut_entry_size == 2 )
                    {
                      img_val = *((uint16_t *) md->shading_table_w + offset );
                      img_val /= factor;
                    }
                  else
                      img_val = *((uint8_t *) md->shading_table_w + offset );
                  fputc((unsigned char)img_val, outfile_w);
                }

              if ( md->shading_table_d != NULL )
                {
                  if ( ms->lut_entry_size == 2 )
                    {
                      img_val = *((uint16_t *) md->shading_table_d + offset );
                      img_val /= factor;
                    }
                  else
                      img_val = *((uint8_t *) md->shading_table_d + offset );
                  fputc((unsigned char)img_val, outfile_d);
                }
            }
        }
    }
  if ( md->shading_table_w != NULL )
    fclose(outfile_w);
  if ( md->shading_table_d != NULL )
    fclose(outfile_d);

  return;
}

static void
write_cshading_pnm(Microtek2_Scanner *ms)
{
  FILE *outfile;
  Microtek2_Device *md;
  Microtek2_Info *mi;
  int pixel, color, line, offset, img_val, img_height=30, factor;

  md = ms->dev;
  mi = &md->info[md->scan_source];

  if ( mi->depth & MI_HASDEPTH_16 )
      factor = 256;
  else if ( mi->depth & MI_HASDEPTH_14 )
      factor = 64;
  else if ( mi->depth & MI_HASDEPTH_12 )
      factor = 16;
  else if ( mi->depth & MI_HASDEPTH_10 )
      factor = 4;
  else
      factor = 1;
  if ( md->model_flags & MD_16BIT_TRANSFER )
      factor = 256;

  outfile = fopen("microtek2_cshading_w.pnm", "w");
  if ( ms->mode == MS_MODE_COLOR )
    fprintf(outfile, "P6\n#imagedata\n%d %d\n255\n", ms->ppl, img_height);
  else
    fprintf(outfile, "P5\n#imagedata\n%d %d\n255\n", ms->ppl, img_height);

  for ( line=0; line < img_height; ++line )
    {
      for ( pixel=0; pixel < (int)ms->ppl; ++pixel)
        {
          for ( color=0; color < 3; ++color )
            {
              offset = color * (int)ms->ppl + pixel;
              if ( ms->lut_entry_size == 1 )
                img_val = (int) *((uint8_t *)ms->condensed_shading_w + offset);
              else
                {
                  img_val = (int) *((uint16_t *)ms->condensed_shading_w
                                                 + offset);
                  img_val /= factor;
                }
              fputc((unsigned char)img_val, outfile);
              if ( ms->mode == MS_MODE_GRAY )
                break;
            }
        }
    }
  fclose(outfile);

  return;
}



/*---------- condense_shading() ----------------------------------------------*/

static SANE_Status
condense_shading(Microtek2_Scanner *ms)
{
    /* This function extracts the relevant shading pixels from */
    /* the shading image according to the 1's in the result of */
    /* 'read control bits', and stores them in a memory block. */
    /* We will then have as many shading pixels as there are */
    /* pixels per line. The order of the pixels in the condensed */
    /* shading data block will always be left to right. The color */
    /* sequence remains unchanged. */

    Microtek2_Device *md;
    Microtek2_Info *mi;
    uint32_t byte;
    uint32_t cond_length;       /* bytes per condensed shading line */
    int color, count, lfd_bit;
    int shad_bplc, shad_pixels;  /* bytes per line & color in shading image */
    int bit, flag;
    uint32_t sh_offset, csh_offset;
    int gray_filter_color = 1; /* which color of the shading is taken for gray*/

    md = ms->dev;
    mi = &md->info[md->scan_source];

    DBG(30, "condense_shading: ms=%p, ppl=%d\n", (void *) ms, ms->ppl);
    if ( md->shading_table_w == NULL )
      {
        DBG(1, "condense shading: no shading table found, skip shading\n");
        return SANE_STATUS_GOOD;
      }

    get_lut_size( mi, &ms->lut_size, &ms->lut_entry_size );

    if ( md->model_flags & MD_PHANTOM336CX_TYPE_SHADING )
      {
        shad_pixels = ms->n_control_bytes * 8;
        gray_filter_color = 0;  /* 336CX reads only one shading in gray mode*/
      }
    else
        shad_pixels = mi->geo_width;

    shad_bplc = shad_pixels * ms->lut_entry_size;

    if ( md_dump >= 3 )
      {
        dump_area2(md->shading_table_w, shad_bplc * 3, "shading_table_w");
        if ( md->model_flags & MD_PHANTOM336CX_TYPE_SHADING )
          write_shading_pnm(ms);
      }

    cond_length = ms->bpl * ms->lut_entry_size;

    if ( ms->condensed_shading_w )
      {
        free((void*) ms->condensed_shading_w );
        ms->condensed_shading_w = NULL;
      }
    ms->condensed_shading_w = (uint8_t *)malloc(cond_length);
    DBG(100, "condense_shading: ms->condensed_shading_w=%p,"
             "malloc'd %d bytes\n", ms->condensed_shading_w, cond_length);
    if ( ms->condensed_shading_w == NULL )
      {
        DBG(1, "condense_shading: malloc for white table failed\n");
        return SANE_STATUS_NO_MEM;
      }

    if ( md->shading_table_d != NULL )
      {
        if ( md_dump >= 3 )
           dump_area2(md->shading_table_d, shad_bplc * 3,
                      "shading_table_d");

        if ( ms->condensed_shading_d )
          {
            free((void*) ms->condensed_shading_d );
            ms->condensed_shading_d = NULL;
          }
        ms->condensed_shading_d = (uint8_t *)malloc(cond_length);
        DBG(100, "condense_shading: ms->condensed_shading_d=%p,"
                 " malloc'd %d bytes\n", ms->condensed_shading_d, cond_length);
        if ( ms->condensed_shading_d == NULL )
          {
            DBG(1, "condense_shading: malloc for dark table failed\n");
            return SANE_STATUS_NO_MEM;
          }
      }

    DBG(128, "controlbit offset=%d\n", md->controlbit_offset);

    count = 0;

    for (lfd_bit = 0; ( lfd_bit < mi->geo_width ) && ( count < (int)ms->ppl );
         ++lfd_bit)
      {
        byte = ( lfd_bit + md->controlbit_offset ) / 8;
        bit = ( lfd_bit + md->controlbit_offset ) % 8;

        if ( mi->direction & MI_DATSEQ_RTOL )
            flag = ((ms->control_bytes[byte] >> bit) & 0x01);
        else
            flag = ((ms->control_bytes[byte] >> (7 - bit)) & 0x01);

        if ( flag == 1 ) /* flag==1 if byte's bit is set */
          {
            for ( color = 0; color < 3; ++color )
              {
                if ( ( ms->mode == MS_MODE_COLOR )
                     || ( ( ms->mode == MS_MODE_GRAY )
                           && ( color == gray_filter_color ) )
                     || ( ( ms->mode == MS_MODE_LINEARTFAKE )
                           && ( color == gray_filter_color ) )
                    )
                  {
                    sh_offset = color * shad_pixels + lfd_bit;
                    if ( md->model_flags & MD_PHANTOM336CX_TYPE_SHADING )
                      sh_offset += md->controlbit_offset;
                    if ( ms->mode == MS_MODE_COLOR )
                      csh_offset = color * ms->ppl + count;
                    else
                      csh_offset = count;

                    if ( csh_offset > cond_length )
                      {
                        DBG(1, "condense_shading: wrong control bits data, " );
                        DBG(1, "csh_offset (%d) > cond_length(%d)\n",
                             csh_offset, cond_length );
                        csh_offset = cond_length;
                      }

                    if ( ms->lut_entry_size == 2 )
                      {
                        *((uint16_t *)ms->condensed_shading_w + csh_offset) =
                                         *((uint16_t *)md->shading_table_w
                                          + sh_offset);
                        if ( ms->condensed_shading_d != NULL )
                          *((uint16_t *)ms->condensed_shading_d + csh_offset) =
                                         *((uint16_t *)md->shading_table_d
                                          + sh_offset);
                      }
                    else
                      {
                        *((uint8_t *)ms->condensed_shading_w + csh_offset) =
                                         *((uint8_t *)md->shading_table_w
                                          + sh_offset);
                        if ( ms->condensed_shading_d != NULL )
                          *((uint8_t *)ms->condensed_shading_d + csh_offset) =
                                         *((uint8_t *)md->shading_table_d
                                          + sh_offset);
                      }
                  }
              }
            ++count;
          }
      }

    if ( md_dump >= 3 )
      {
        dump_area2(ms->condensed_shading_w, cond_length, "condensed_shading_w");
        if ( ms->condensed_shading_d != NULL )
          dump_area2(ms->condensed_shading_d, cond_length,
                     "condensed_shading_d");

        write_cshading_pnm(ms);
      }

    return SANE_STATUS_GOOD;
}


/*---------- read_shading_image() --------------------------------------------*/

static SANE_Status
read_shading_image(Microtek2_Scanner *ms)
{
    SANE_Status status;
    Microtek2_Device *md;
    Microtek2_Info *mi;
    uint32_t lines;
    uint8_t *buf;
    int max_lines;
    int lines_to_read;

    DBG(30, "read_shading_image: ms=%p\n", (void *) ms);

    md = ms->dev;
    mi = &md->info[md->scan_source];


    if ( ! MI_WHITE_SHADING_ONLY(mi->shtrnsferequ)
        || ( md->model_flags & MD_PHANTOM_C6 ) )

      /* Dark shading correction */
      /* ~~~~~~~~~~~~~~~~~~~~~~~ */
      {
        DBG(30, "read_shading_image: reading black data\n");
        md->status.ntrack |= MD_NTRACK_ON;
        md->status.ncalib &= ~MD_NCALIB_ON;
        md->status.flamp |= MD_FLAMP_ON;
        if ( md->model_flags & MD_PHANTOM_C6 )
          {
            md->status.stick |= MD_STICK_ON;
            md->status.reserved17 |= MD_RESERVED17_ON;
          }

        get_calib_params(ms);
        if ( md->model_flags & MD_PHANTOM_C6 )
             ms->stay = 1;

        status = scsi_send_system_status(md, ms->sfd);
        if ( status != SANE_STATUS_GOOD )
            return status;

        status = scsi_set_window(ms, 1);
        if ( status != SANE_STATUS_GOOD )
            return status;

#ifdef TESTBACKEND
        status = scsi_read_sh_image_info(ms);
#else
        status = scsi_read_image_info(ms);
#endif
        if ( status != SANE_STATUS_GOOD )
            return status;

        status = scsi_wait_for_image(ms);
        if ( status != SANE_STATUS_GOOD )
            return status;

        status = scsi_read_system_status(md, ms->sfd);
        if ( status != SANE_STATUS_GOOD )
            return status;

        md->status.flamp &= ~MD_FLAMP_ON;

        status = scsi_send_system_status(md, ms->sfd);
        if ( status != SANE_STATUS_GOOD )
            return status;

        ms->shading_image = malloc(ms->bpl * ms->src_remaining_lines);
        DBG(100, "read shading image: ms->shading_image=%p,"
                 " malloc'd %d bytes\n",
               ms->shading_image, ms->bpl * ms->src_remaining_lines);
        if ( ms->shading_image == NULL )
          {
            DBG(1, "read_shading_image: malloc for buffer failed\n");
            return SANE_STATUS_NO_MEM;
          }

        buf = ms->shading_image;

#ifdef TESTBACKEND
        max_lines = 5000000 / ms->bpl;
#else
        max_lines = sanei_scsi_max_request_size / ms->bpl;
#endif
        if ( max_lines == 0 )
          {
            DBG(1, "read_shading_image: buffer too small\n");
            return SANE_STATUS_IO_ERROR;
          }
        lines = ms->src_remaining_lines;
        while ( ms->src_remaining_lines > 0 )
          {
            lines_to_read = MIN(max_lines, ms->src_remaining_lines);
            ms->src_buffer_size = lines_to_read * ms->bpl;
            ms->transfer_length = ms->src_buffer_size;
#ifdef TESTBACKEND
            status = scsi_read_sh_d_image(ms, buf);
#else
            status = scsi_read_image(ms, buf, md->shading_depth>8 ? 2 : 1);
#endif
            if ( status != SANE_STATUS_GOOD )
              {
                DBG(1, "read_shading_image: read image failed: '%s'\n",
                        sane_strstatus(status));
                return status;
              }

            ms->src_remaining_lines -= lines_to_read;
            buf += ms->src_buffer_size;
          }

        status =  prepare_shading_data(ms, lines, &md->shading_table_d);
        if ( status != SANE_STATUS_GOOD )
            return status;

        /* send shading data to the device */
        /* Some models use "read_control bit", and the shading must be */
        /* applied by the backend later */
        if ( ! (md->model_flags & MD_READ_CONTROL_BIT) )
          {
            status =  shading_function(ms, md->shading_table_d);
            if ( status != SANE_STATUS_GOOD )
                return status;

            ms->word = ms->lut_entry_size == 2 ? 1 : 0;
            ms->current_color = MS_COLOR_ALL;
            status = scsi_send_shading(ms,
                                       md->shading_table_d,
                                       3 * ms->lut_entry_size
                                         * mi->geo_width / mi->calib_divisor,
                                       1);
            if ( status != SANE_STATUS_GOOD )
                return status;
          }

        DBG(100, "free memory for ms->shading_image at %p\n",
               ms->shading_image);
        free((void *) ms->shading_image);
        ms->shading_image = NULL;
      }

    /* white shading correction */
    /* ~~~~~~~~~~~~~~~~~~~~~~~~ */
    DBG(30, "read_shading_image: reading white data\n");

    /* According to the doc NCalib must be set for white shading data */
    /* if we have a black and a white shading correction and must be */
    /* cleared if we have only a white shading collection */
    if ( ! MI_WHITE_SHADING_ONLY(mi->shtrnsferequ)
        || ( md->model_flags & MD_PHANTOM_C6 ) )
      md->status.ncalib |= MD_NCALIB_ON;
    else
      md->status.ncalib &= ~MD_NCALIB_ON;

    md->status.flamp |= MD_FLAMP_ON;
/*    md->status.tlamp &= ~MD_TLAMP_ON;  */
    md->status.ntrack |= MD_NTRACK_ON;

    if ( md->model_flags & MD_PHANTOM_C6 )
      {
        md->status.stick &= ~MD_STICK_ON;
        md->status.reserved17 |= MD_RESERVED17_ON;
      }

    get_calib_params(ms);

#ifdef NO_PHANTOMTYPE_SHADING
/*    md->status.stick &= ~MD_STICK_ON; */
/*    md->status.ncalib &= ~MD_NCALIB_ON; */
/*    md->status.reserved17 &= ~MD_RESERVED17_ON; */
    ms->rawdat = 0;
#endif

    status = scsi_send_system_status(md, ms->sfd);
    if ( status != SANE_STATUS_GOOD )
        return status;

    status = scsi_set_window(ms, 1);
    if ( status != SANE_STATUS_GOOD )
        return status;

#ifdef TESTBACKEND
    status = scsi_read_sh_image_info(ms);
#else
    status = scsi_read_image_info(ms);
#endif
    if ( status != SANE_STATUS_GOOD )
        return status;

    status = scsi_wait_for_image(ms);
    if ( status != SANE_STATUS_GOOD )
        return status;

#ifdef NO_PHANTOMTYPE_SHADING
    if ( !( md->model_flags & MD_READ_CONTROL_BIT ) )
      {
#endif
	status = scsi_read_system_status(md, ms->sfd);
        if ( status != SANE_STATUS_GOOD )
            return status;
#ifdef NO_PHANTOMTYPE_SHADING
      }
#endif

#ifdef NO_PHANTOMTYPE_SHADING
    if ( mi->model_code == 0x94 )
        status = scsi_read_control_bits(ms);
#endif

    ms->shading_image = malloc(ms->bpl * ms->src_remaining_lines);
    DBG(100, "read shading image: ms->shading_image=%p, malloc'd %d bytes\n",
              ms->shading_image, ms->bpl * ms->src_remaining_lines);
    if ( ms->shading_image == NULL )
      {
        DBG(1, "read_shading_image: malloc for buffer failed\n");
        return SANE_STATUS_NO_MEM;
      }

    buf = ms->shading_image;
#ifdef TESTBACKEND
    max_lines = 5000000 / ms->bpl;
#else
    max_lines = sanei_scsi_max_request_size / ms->bpl;
#endif
    if ( max_lines == 0 )
      {
        DBG(1, "read_shading_image: buffer too small\n");
        return SANE_STATUS_IO_ERROR;
      }
    lines = ms->src_remaining_lines;
    while ( ms->src_remaining_lines > 0 )
      {
        lines_to_read = MIN(max_lines, ms->src_remaining_lines);
        ms->src_buffer_size = lines_to_read * ms->bpl;
        ms->transfer_length = ms->src_buffer_size;

#ifdef TESTBACKEND
        status = scsi_read_sh_w_image(ms, buf);
#else
	status = scsi_read_image(ms, buf, md->shading_depth>8 ? 2 : 1);
#endif
        if ( status != SANE_STATUS_GOOD )
            return status;

        ms->src_remaining_lines -= lines_to_read;
        buf += ms->src_buffer_size;
      }

    status =  prepare_shading_data(ms, lines, &md->shading_table_w);
    if ( status != SANE_STATUS_GOOD )
        return status;

    if ( md_dump >= 3 )
      {
        write_shading_buf_pnm(ms, lines);
        write_shading_pnm(ms);
      }

    /* send shading data to the device */
    /* Some models use "read_control bit", and the shading must be */
    /* applied by the backend later */
    if ( ! (md->model_flags & MD_READ_CONTROL_BIT) )
      {
        status =  shading_function(ms, md->shading_table_w);
        if ( status != SANE_STATUS_GOOD )
            return status;

        ms->word = ms->lut_entry_size == 2 ? 1 : 0;
        ms->current_color = MS_COLOR_ALL;
        status = scsi_send_shading(ms,
                                   md->shading_table_w,
                                   3 * ms->lut_entry_size
                                   * mi->geo_width / mi->calib_divisor,
                                   0);
        if ( status != SANE_STATUS_GOOD )
            return status;
      }

    ms->rawdat = 0;
    ms->stay = 0;
    md->status.ncalib |= MD_NCALIB_ON;

    if ( md->model_flags & MD_PHANTOM_C6 )
      {
        md->status.stick &= ~MD_STICK_ON;
        md->status.reserved17 &= ~MD_RESERVED17_ON;
      }

#ifdef NO_PHANTOMTYPE_SHADING
    if (mi->model_code == 0x94)
        md->status.ncalib &= ~MD_NCALIB_ON;
#endif

    status = scsi_send_system_status(md, ms->sfd);
    if ( status != SANE_STATUS_GOOD )
        return status;

    DBG(100, "free memory for ms->shading_image at %p\n",
    ms->shading_image);
    free((void *) ms->shading_image);
    ms->shading_image = NULL;

    return SANE_STATUS_GOOD;

}

/*---------- prepare_shading_data() ------------------------------------------*/

static SANE_Status
prepare_shading_data(Microtek2_Scanner *ms, uint32_t lines, uint8_t **data)
{
  /* This function calculates one line of black or white shading data */
  /* from the shading image. At the end we have one line. The */
  /* color sequence is unchanged. */

#define MICROTEK2_CALIB_USE_MEDIAN

  Microtek2_Device *md;
  Microtek2_Info *mi;
  uint32_t length,line;
  int color, i;
  SANE_Status status;

#ifdef  MICROTEK2_CALIB_USE_MEDIAN
  uint16_t *sortbuf, value;
#else
  uint32_t value;
#endif

  DBG(30, "prepare_shading_data: ms=%p, lines=%d, *data=%p\n",
          (void *) ms, lines, *data);

  md = ms->dev;
  mi = &md->info[md->scan_source];
  status = SANE_STATUS_GOOD;

  get_lut_size(mi, &ms->lut_size, &ms->lut_entry_size);
  length = 3 * ms->lut_entry_size * mi->geo_width / mi->calib_divisor;

  if ( *data == NULL )
    {
      *data = (uint8_t *) malloc(length);
      DBG(100, "prepare_shading_data: malloc'd %d bytes at %p\n",
                length, *data);
      if ( *data == NULL )
        {
          DBG(1, "prepare_shading_data: malloc for shading table failed\n");
          return SANE_STATUS_NO_MEM;
        }
    }

#ifdef  MICROTEK2_CALIB_USE_MEDIAN
  sortbuf = malloc( lines * ms->lut_entry_size );
  DBG(100, "prepare_shading_data: sortbuf= %p, malloc'd %d Bytes\n",
            (void *) sortbuf, lines * ms->lut_entry_size);
  if ( sortbuf == NULL )
    {
      DBG(1, "prepare_shading_data: malloc for sort buffer failed\n");
      return SANE_STATUS_NO_MEM;
    }
#endif

  switch( mi->data_format )
    {
      case MI_DATAFMT_LPLCONCAT:
        if ( ms->lut_entry_size == 1 )
          {
            DBG(1, "prepare_shading_data: wordsize == 1 unsupported\n");
            return SANE_STATUS_UNSUPPORTED;
          }
        for ( color = 0; color < 3; color++ )
          {
            for ( i = 0; i < ( mi->geo_width / mi->calib_divisor ); i++ )
              {
                value = 0;
                for ( line = 0; line < lines; line++ )
#ifndef  MICROTEK2_CALIB_USE_MEDIAN
/*  average the shading lines to get the shading data */
                      value += *((uint16_t *) ms->shading_image
                             + line * ( ms->bpl / ms->lut_entry_size )
                             + color * ( ms->bpl / ms->lut_entry_size / 3 )
                             + i);
                value /= lines;
                *((uint16_t *) *data
                   + color * ( mi->geo_width / mi->calib_divisor ) + i) =
                                           (uint16_t) MIN(0xffff, value);
#else
/*  use a median filter to get the shading data -- should be better */
                    *(sortbuf + line ) =
                          *((uint16_t *) ms->shading_image
                             + line * ( ms->bpl / ms->lut_entry_size )
                             + color * ( ms->bpl / ms->lut_entry_size / 3 )
                             + i);
                qsort(sortbuf, lines, sizeof(uint16_t),
                       (qsortfunc)compare_func_16);
                value = *(sortbuf + ( lines - 1 ) / 2 );
                *((uint16_t *) *data
                   + color * ( mi->geo_width / mi->calib_divisor ) + i) = value;
#endif
              }
          }
        break;

      case MI_DATAFMT_CHUNKY:
      case MI_DATAFMT_9800:
        if ( ms->lut_entry_size == 1 )
          {
            DBG(1, "prepare_shading_data: wordsize == 1 unsupported\n");
            return SANE_STATUS_UNSUPPORTED;
          }
        for ( color = 0; color < 3; color++ )
          {
            for ( i = 0; i < ( mi->geo_width / mi->calib_divisor ); i++ )
              {
                value = 0;
                for ( line = 0; line < lines; line++ )
#ifndef  MICROTEK2_CALIB_USE_MEDIAN
/*  average the shading lines to get the shading data */
                    value += *((uint16_t *) ms->shading_image
                             + line * 3 * mi->geo_width / mi->calib_divisor
                             + 3 * i
                             + color);

                value /= lines;
                *((uint16_t *) *data
                 + color * ( mi->geo_width / mi->calib_divisor ) + i) =
                                               (uint16_t) MIN(0xffff, value);
#else
/*  use a median filter to get the shading data -- should be better */
                    *(sortbuf + line ) =
                          *((uint16_t *) ms->shading_image
                             + line * 3 * mi->geo_width / mi->calib_divisor
                             + 3 * i
                             + color);
                qsort(sortbuf, lines, sizeof(uint16_t),
                       (qsortfunc)compare_func_16);
                value = *(sortbuf + ( lines - 1 ) / 2 );
                *((uint16_t *) *data
                 + color * ( mi->geo_width / mi->calib_divisor ) + i) = value;
#endif
              }
          }
        break;

      case MI_DATAFMT_LPLSEGREG:
        for ( color = 0; color < 3; color++ )
          {
            for ( i = 0; i < ( mi->geo_width / mi->calib_divisor ); i++ )
              {
                value = 0;
                if ( ms->lut_entry_size == 1 )
		  {
                    for ( line = 0; line < lines; line++ )
			value += *((uint8_t *) ms->shading_image
				+ line * 3 * mi->geo_width / mi->calib_divisor
				+ 3 * i
				+ color);

		    value /= lines;
                    *((uint8_t *) *data
			+ color * ( mi->geo_width / mi->calib_divisor ) + i) =
			(uint8_t) MIN(0xff, value);

		  }
		else
		  {
		    for ( line = 0; line < lines; line++ )
			value += *((uint16_t *) ms->shading_image
				+ line * 3 * mi->geo_width / mi->calib_divisor
				+ 3 * i
				+ color);

		    value /= lines;
#ifndef  MICROTEK2_CALIB_USE_MEDIAN
		    *((uint16_t *) *data
			+ color * ( mi->geo_width / mi->calib_divisor ) + i) =
			(uint16_t) MIN(0xffff, value);
#else
		    *((uint16_t *) *data
			+ color * ( mi->geo_width / mi->calib_divisor ) + i) = value;
#endif
		  }

              }
          }
        break;

      default:
        DBG(1, "prepare_shading_data: Unsupported data format 0x%02x\n",
                mi->data_format);
        status = SANE_STATUS_UNSUPPORTED;
    }

#ifdef  MICROTEK2_CALIB_USE_MEDIAN
  DBG(100, "prepare_shading_data: free sortbuf at %p\n", (void *) sortbuf);
  free(sortbuf);
  sortbuf = NULL;
#endif
    return status;
}


/*---------- read_cx_shading() -----------------------------------------------*/

static SANE_Status
read_cx_shading(Microtek2_Scanner *ms)
{
    SANE_Status status;
    Microtek2_Device *md;
    md = ms->dev;

    DBG(30, "read_cx_shading: ms=%p\n",(void *) ms);

    md->shading_table_contents = ms->mode;

    if ( ms->mode == MS_MODE_COLOR )
        ms->current_color = MS_COLOR_ALL;
    else
        ms->current_color = MS_COLOR_GREEN;  /* for grayscale */

    ms->word = 1;
    ms->dark = 0;

    status = read_cx_shading_image(ms);
    if ( status  != SANE_STATUS_GOOD )
        goto cleanup;

    ms->word = 0;  /* the Windows driver reads dark shading with word=0 */
    ms->dark = 1;
    status = read_cx_shading_image(ms);
    if ( status  != SANE_STATUS_GOOD )
        goto cleanup;

    return SANE_STATUS_GOOD;

cleanup:
    cleanup_scanner(ms);
    return status;
}


/*---------- read_cx_shading_image() -----------------------------------------*/

static SANE_Status
read_cx_shading_image(Microtek2_Scanner *ms)
{
    SANE_Status status;
    Microtek2_Device *md;
    uint32_t shading_bytes, linesize, buffer_size;
    uint8_t *buf;
    int max_lines, lines_to_read, remaining_lines;

    md = ms->dev;

    shading_bytes = ms->n_control_bytes * 8 * md->shading_length;
    if ( ms->current_color == MS_COLOR_ALL )
        shading_bytes *= 3;
    if ( ms->word == 1 )
        shading_bytes *= 2;

    if ( ms->shading_image )
      {
        free((void *) ms->shading_image);
        ms->shading_image = NULL;
      }
    ms->shading_image = malloc(shading_bytes);
    DBG(100, "read_cx_shading: ms->shading_image=%p, malloc'd %d bytes\n",
           ms->shading_image, shading_bytes);
    if ( ms->shading_image == NULL )
      {
        DBG(1, "read_cx_shading: malloc for cx_shading buffer failed\n");
        return SANE_STATUS_NO_MEM;
      }

    buf = ms->shading_image;

    DBG(30, "read_cx_shading_image: ms=%p, shading_bytes=%d\n",
                                       (void *) ms, shading_bytes);

    linesize = shading_bytes / md->shading_length;
#ifdef TESTBACKEND
    max_lines = 5000000 / linesize;
#else
    max_lines = sanei_scsi_max_request_size / linesize;
#endif
    /* the following part is like in "read_shading_image"  */
    remaining_lines = md->shading_length;
    while ( remaining_lines > 0 )
      {
        lines_to_read = MIN(max_lines, remaining_lines);
        buffer_size = lines_to_read * linesize;

        status = scsi_read_shading(ms, buf, buffer_size);
        if ( status != SANE_STATUS_GOOD )
          {
            DBG(1, "read_cx_shading: '%s'\n", sane_strstatus(status));
            return status;
          }
        remaining_lines -= lines_to_read;
        buf += buffer_size;
      }

    status = calc_cx_shading_line(ms);
    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "read_cx_shading: '%s'\n", sane_strstatus(status));
        return status;
      }

    if ( ms->shading_image )
      {
        DBG(100, "free memory for ms->shading_image at %p\n",
        ms->shading_image);
        free((void *) ms->shading_image);
        ms->shading_image = NULL;
      }

    return status;
}

/*---------- calc_cx_shading_line() ------------------------------------------*/
/* calculates the mean value of the shading lines and stores one line of      */
/* 8-bit shading data. Scanning direction + color sequence remain as they are */
/* ToDo: more than 8-bit data */

static SANE_Status
calc_cx_shading_line(Microtek2_Scanner *ms)
{
    Microtek2_Device *md;
    SANE_Status status;
    uint8_t *current_byte, *buf, *shading_table_pointer;
    uint8_t color, factor;
    uint32_t shading_line_pixels, shading_line_bytes,
              shading_data_bytes, line, i, accu, color_offset;
    uint16_t *sortbuf, value;

    md = ms->dev;
    status = SANE_STATUS_GOOD;

    sortbuf = malloc( md->shading_length * sizeof(float) );
    DBG(100, "calc_cx_shading: sortbuf= %p, malloc'd %lu Bytes\n",
	(void *) sortbuf, (u_long) (md->shading_length * sizeof(float)));
    if ( sortbuf == NULL )
      {
        DBG(1, "calc_cx_shading: malloc for sort buffer failed\n");
        return SANE_STATUS_NO_MEM;
      }

    buf = ms->shading_image;
    shading_line_pixels = ms->n_control_bytes * 8; /* = 2560 for 330CX  */
    shading_line_bytes = shading_line_pixels;      /* grayscale         */
    if ( ms->mode == MS_MODE_COLOR )               /* color             */
        shading_line_bytes *= 3;
    shading_data_bytes = shading_line_bytes;      /*    8-bit color depth */
    if (ms->word == 1)                            /* >  8-bit color depth */
        shading_data_bytes *= 2;
    factor = 4; /* shading bit depth = 10bit; shading line bit depth = 8bit */

    if (ms->dark == 0)  /* white shading data  */
      {
        if ( md->shading_table_w )
            free( (void *)md->shading_table_w );
        md->shading_table_w = (uint8_t *) malloc(shading_line_bytes);
        DBG(100, "calc_cx_shading: md->shading_table_w=%p, malloc'd %d bytes\n",
               md->shading_table_w, shading_line_bytes);
        if ( md->shading_table_w == NULL )
          {
            DBG(100, "calc_cx_shading: malloc for white shadingtable failed\n");
            status = SANE_STATUS_NO_MEM;
            cleanup_scanner(ms);
          }

        shading_table_pointer = md->shading_table_w;
      }

    else               /*  dark  shading data  */
      {
        if ( md->shading_table_d )
            free( (void *)md->shading_table_d);
        md->shading_table_d = (uint8_t *) malloc(shading_line_bytes);
        DBG(100, "calc_cx_shading: md->shading_table_d=%p, malloc'd %d bytes\n",
               md->shading_table_d, shading_line_bytes);

        if ( md->shading_table_d == NULL )
          {
            DBG(1, "calc_cx_shading: malloc for dark shading table failed\n");
            status = SANE_STATUS_NO_MEM;
            cleanup_scanner(ms);
          }

        shading_table_pointer = md->shading_table_d;
      }

    DBG(30, "calc_cx_shading_line: ms=%p\n"
            "md->shading_table_w=%p\n"
            "md->shading_table_d=%p\n"
            "shading_line_bytes=%d\n"
            "shading_line_pixels=%d\n"
            "shading_table_pointer=%p\n",
             (void *) ms, md->shading_table_w, md->shading_table_d,
             shading_line_bytes, shading_line_pixels, shading_table_pointer);

    /*  calculating the median pixel values over the shading lines  */
    /*  and write them to the shading table                       */
    for (color = 0; color < 3; color++)
      {
        color_offset = color * shading_line_pixels;
        if ( ms->word == 1 )
          color_offset *=2;

        for (i = 0; i < shading_line_pixels; i++)
          {
            value = 0;
            for (line = 0; line < md->shading_length; line++)
              {
                current_byte = buf + ( line * shading_data_bytes )
                               + color_offset + i;
                accu = *current_byte;

                /* word shading data: the lower bytes per line and color are */
                /* transfered first in one block and then the high bytes */
                /* in one block  */
                /* the dark shading data is also 10 bit, but only the */
                /* low byte is transferred (ms->word = 0) */
                if ( ms->word == 1 )
                  {
                    current_byte = buf + ( line * shading_data_bytes )
                               + color_offset + shading_line_pixels + i;
                    accu += ( *current_byte * 256 );
                  }
                *( sortbuf + line ) = accu;
              }
/* this is the Median filter: sort the values ascending and take the middlest */
            qsort(sortbuf, md->shading_length, sizeof(float),
                     (qsortfunc)compare_func_16);
            value = *( sortbuf + ( md->shading_length - 1 ) / 2 );
            *shading_table_pointer = (uint8_t) (value / factor);
            shading_table_pointer++;
          }
        if ( ms->mode != MS_MODE_COLOR )
           break;
      }
    return status;
}



/*---------- get_lut_size() --------------------------------------------------*/

static SANE_Status
get_lut_size(Microtek2_Info *mi, int *max_lut_size, int *lut_entry_size)
{
    /* returns the maximum lookup table size. A device might indicate */
    /* several lookup table sizes. */

    DBG(30, "get_lut_size: mi=%p\n", (void *) mi);

    *max_lut_size = 0;
    *lut_entry_size = 0;

    /* Normally this function is used for both gamma and shading tables */
    /* If, however, the device indicates, that it does not support */
    /* lookup tables, we set these values as if the device has a maximum */
    /* bitdepth of 12, and these values are only used to determine the */
    /* size of the shading table */
    if ( MI_LUTCAP_NONE(mi->lut_cap) )
      {
        *max_lut_size = 4096;
        *lut_entry_size = 2;
      }

    if ( mi->lut_cap & MI_LUTCAP_256B )
      {
        *max_lut_size = 256;
        *lut_entry_size = 1;
      }
    if ( mi->lut_cap & MI_LUTCAP_1024B )
      {
        *max_lut_size = 1024;
        *lut_entry_size = 1;
      }
    if ( mi->lut_cap & MI_LUTCAP_1024W )
      {
        *max_lut_size = 1024;
        *lut_entry_size = 2;
      }
    if ( mi->lut_cap & MI_LUTCAP_4096B )
      {
        *max_lut_size = 4096;
        *lut_entry_size = 1;
      }
    if ( mi->lut_cap & MI_LUTCAP_4096W )
      {
          *max_lut_size = 4096;
          *lut_entry_size = 2;
      }
    if ( mi->lut_cap & MI_LUTCAP_64k_W )
      {
          *max_lut_size = 65536;
          *lut_entry_size = 2;
      }
    if ( mi->lut_cap & MI_LUTCAP_16k_W )
      {
          *max_lut_size = 16384;
          *lut_entry_size = 2;
      }
    DBG(30, "get_lut_size:  mi=%p, lut_size=%d, lut_entry_size=%d\n",
             (void *) mi, *max_lut_size, *lut_entry_size);
    return SANE_STATUS_GOOD;
}


/*---------- calculate_gamma() -----------------------------------------------*/

static SANE_Status
calculate_gamma(Microtek2_Scanner *ms, uint8_t *pos, int color, char *mode)
{
    Microtek2_Device *md;
    Microtek2_Info *mi;
    double exp;
    double mult;
    double steps;
    unsigned int val;
    int i;
    int factor;           /* take into account the differences between the */
                          /* possible values for the color and the number */
                          /* of bits the scanner works with internally. */
                          /* If depth == 1 handle this as if the maximum */
                          /* depth was chosen */


    DBG(30, "calculate_gamma: ms=%p, pos=%p, color=%d, mode=%s\n",
             (void *) ms, pos, color, mode);

    md = ms->dev;
    mi = &md->info[md->scan_source];

    /* does this work everywhere ? */
    if ( md->model_flags & MD_NO_GAMMA )
      {
        factor = 1;
        mult = (double) (ms->lut_size - 1);
      }
    else
      {
        if ( mi->depth & MI_HASDEPTH_16 )
          {
            factor = ms->lut_size / 65536;
            mult = 65535.0;
          }
        else if ( mi->depth & MI_HASDEPTH_14 )
          {
            factor = ms->lut_size / 16384;
            mult = 16383.0;
          }
        else if ( mi->depth & MI_HASDEPTH_12 )
          {
            factor = ms->lut_size / 4096;
            mult = 4095.0;
          }
        else if ( mi->depth & MI_HASDEPTH_10 )
          {
            factor = ms->lut_size / 1024;
            mult = 1023.0;
          }
        else
          {
            factor = ms->lut_size / 256;
            mult = 255.0;
          }
      }

#if 0
    factor = ms->lut_size / (int) pow(2.0, (double) ms->depth);
    mult = pow(2.0, (double) ms->depth) - 1.0;  /* depending on output size */
#endif

    steps = (double) (ms->lut_size - 1);      /* depending on input size */

    DBG(30, "calculate_gamma: factor=%d, mult =%f, steps=%f, mode=%s\n",
             factor, mult, steps, ms->val[OPT_GAMMA_MODE].s);


    if ( strcmp(mode, MD_GAMMAMODE_SCALAR) == 0 )
      {
        int option;

        option = OPT_GAMMA_SCALAR;
        /* OPT_GAMMA_SCALAR_R follows OPT_GAMMA_SCALAR directly */
        if ( ms->val[OPT_GAMMA_BIND].w == SANE_TRUE )
            exp = 1.0 / SANE_UNFIX(ms->val[option].w);
        else
            exp = 1.0 / SANE_UNFIX(ms->val[option + color + 1].w);

        for ( i = 0; i < ms->lut_size; i++ )
          {
            val = (unsigned int) (mult * pow((double) i / steps, exp) + .5);

            if ( ms->lut_entry_size == 2 )
                *((uint16_t *) pos + i) = (uint16_t) val;
            else
                *((uint8_t *) pos + i) = (uint8_t) val;
          }
      }
    else if ( strcmp(mode, MD_GAMMAMODE_CUSTOM) == 0 )
      {
        int option;
        SANE_Int *src;

        option = OPT_GAMMA_CUSTOM;
        if ( ms->val[OPT_GAMMA_BIND].w == SANE_TRUE )
            src = ms->val[option].wa;
        else
            src = ms->val[option + color + 1].wa;

        for ( i = 0; i < ms->lut_size; i++ )
          {
            if ( ms->lut_entry_size == 2 )
                *((uint16_t *) pos + i) = (uint16_t) (src[i] / factor);
            else
                *((uint8_t *) pos + i) = (uint8_t) (src[i] / factor);
          }
      }
    else if ( strcmp(mode, MD_GAMMAMODE_LINEAR) == 0 )
      {
        for ( i = 0; i < ms->lut_size; i++ )
          {
            if ( ms->lut_entry_size == 2 )
                *((uint16_t *) pos + i) = (uint16_t) (i / factor);
            else
                *((uint8_t *) pos + i) = (uint8_t) (i / factor);
          }
      }

    return SANE_STATUS_GOOD;
}


/*---------- shading_function() ----------------------------------------------*/

static SANE_Status
shading_function(Microtek2_Scanner *ms, uint8_t *data)
{
    Microtek2_Device *md;
    Microtek2_Info *mi;
    uint32_t value;
    int color;
    int i;


    DBG(40, "shading_function: ms=%p, data=%p\n", (void *) ms, data);

    md = ms->dev;
    mi = &md->info[md->scan_source];
/*    mi = &md->info[MD_SOURCE_FLATBED]; */

    if ( ms->lut_entry_size == 1 )
      {
        DBG(1, "shading_function: wordsize = 1 unsupported\n");
         return SANE_STATUS_IO_ERROR;
      }

    for ( color = 0; color < 3; color++ )
      {
        for ( i = 0; i < ( mi->geo_width / mi->calib_divisor ); i++)
          {
            value = *((uint16_t *) data
                      + color * ( mi->geo_width / mi->calib_divisor ) + i);
            switch ( mi->shtrnsferequ )
              {
                case 0x00:
                  /* output == input */
                  break;

                case 0x01:
                  value = (ms->lut_size * ms->lut_size) / value;
                  *((uint16_t *) data
                    + color * ( mi->geo_width / mi->calib_divisor ) + i) =
                                               (uint16_t) MIN(0xffff, value);
                  break;

                case 0x11:
                  value = (ms->lut_size * ms->lut_size)
                           / (uint32_t) ( (double) value
                                           * ((double) mi->balance[color]
                                             / 255.0));
                  *((uint16_t *) data
                    + color * ( mi->geo_width / mi->calib_divisor ) + i) =
                                               (uint16_t) MIN(0xffff, value);
                  break;
                case 0x15:
                  value = (uint32_t) ( ( 1073741824 / (double) value )
                                           * ( (double) mi->balance[color]
                                            / 256.0) );
                  value = MIN(value, (uint32_t)65535);
                 *((uint16_t *) data
                    + color * ( mi->geo_width / mi->calib_divisor ) + i) =
                                               (uint16_t) MIN(0xffff, value);
                  break;

                default:
                  DBG(1, "Unsupported shading transfer function 0x%02x\n",
                  mi->shtrnsferequ );
                  break;
              }
          }
      }

    return SANE_STATUS_GOOD;
}


/*---------- set_exposure() --------------------------------------------------*/

static void
set_exposure(Microtek2_Scanner *ms)
{
    /* This function manipulates the colors according to the exposure time */
    /* settings on models where they are ignored. Currently this seems to */
    /* be the case for all models with the data format chunky data. They */
    /* all have tables with two byte gamma output, so for now we ignore */
    /* gamma tables with one byte output */

    Microtek2_Device *md;
    Microtek2_Info *mi;
    int color;
    int size;
    int depth;
    int maxval;
    int byte;
    uint32_t val32;
    uint8_t *from;
    uint8_t exposure;
    uint8_t exposure_rgb[3];

    
    DBG(30, "set_exposure: ms=%p\n", (void *) ms);

    md = ms->dev;
    mi = &md->info[md->scan_source];

    if ( ms->lut_entry_size == 1 )
      {
        DBG(1, "set_exposure: 1 byte gamma output tables currently ignored\n");
        return;
      }

    if ( mi->depth & MI_HASDEPTH_16 )
        depth = 16;
    else if ( mi->depth & MI_HASDEPTH_14 )
        depth = 14;
    else if ( mi->depth & MI_HASDEPTH_12 )
        depth = 12;
    else if ( mi->depth & MI_HASDEPTH_10 )
        depth = 10;
    else
        depth = 8;

    maxval = ( 1 << depth ) - 1;

    from = ms->gamma_table;
    size = ms->lut_size;

    /* first master channel, apply transformation to all colors */
    exposure = ms->exposure_m;
    for ( byte = 0; byte < ms->lut_size; byte++ )
      {
        for ( color = 0; color < 3; color++)
          {
            val32 = (uint32_t) *((uint16_t *) from + color * size + byte);
            val32 = MIN(val32 + val32
                     * (2 * (uint32_t) exposure / 100), (uint32_t) maxval);
            *((uint16_t *) from + color * size + byte) = (uint16_t) val32;
          }
      }

    /* and now apply transformation to each channel */

    exposure_rgb[0] = ms->exposure_r;
    exposure_rgb[1] = ms->exposure_g;
    exposure_rgb[2] = ms->exposure_b;
    for ( color = 0; color < 3; color++ )
      {
        for ( byte = 0; byte < size; byte++ )
          {
            val32 = (uint32_t) *((uint16_t *) from + color * size + byte);
            val32 = MIN(val32 + val32 
                         * (2 * (uint32_t) exposure_rgb[color] / 100),
                         (uint32_t) maxval);
            *((uint16_t *) from + color * size + byte) = (uint16_t) val32;
          }
      }

    return;
}


/*---------- reader_process() ------------------------------------------------*/

static int
reader_process(void *data)
{
    Microtek2_Scanner *ms = (Microtek2_Scanner *) data;

    SANE_Status status;
    Microtek2_Info *mi;
    Microtek2_Device *md;
    struct SIGACTION act;
    sigset_t sigterm_set;
    static uint8_t *temp_current = NULL;

    DBG(30, "reader_process: ms=%p\n", (void *) ms);

    md = ms->dev;
    mi = &md->info[md->scan_source];

    if (sanei_thread_is_forked()) close(ms->fd[0]);

    sigemptyset (&sigterm_set);
    sigaddset (&sigterm_set, SIGTERM);
    memset (&act, 0, sizeof (act));
    act.sa_handler = signal_handler;
    sigaction (SIGTERM, &act, 0);

    ms->fp = fdopen(ms->fd[1], "w");
    if ( ms->fp == NULL )
      {
        DBG(1, "reader_process: fdopen() failed, errno=%d\n", errno);
        return SANE_STATUS_IO_ERROR;
      }
   
    if ( ms->auto_adjust == 1 )
      {
        if ( temp_current == NULL )
            temp_current = ms->temporary_buffer;
      }

    while ( ms->src_remaining_lines > 0 )
      {
       
        ms->src_lines_to_read = MIN(ms->src_remaining_lines, ms->src_max_lines);
        ms->transfer_length = ms->src_lines_to_read * ms->bpl;

        DBG(30, "reader_process: transferlength=%d, lines=%d, linelength=%d, "
                "real_bpl=%d, srcbuf=%p\n", ms->transfer_length,
                 ms->src_lines_to_read, ms->bpl, ms->real_bpl, ms->buf.src_buf);

        sigprocmask (SIG_BLOCK, &sigterm_set, 0);
        status = scsi_read_image(ms, ms->buf.src_buf, (ms->depth > 8) ? 2 : 1);
        sigprocmask (SIG_UNBLOCK, &sigterm_set, 0);
        if ( status != SANE_STATUS_GOOD ) 
            return SANE_STATUS_IO_ERROR;

        ms->src_remaining_lines -= ms->src_lines_to_read;

        /* prepare data for frontend */
        switch (ms->mode) 
          {
	    case MS_MODE_COLOR:
              if ( ! mi->onepass )
                /* TODO */
                {
                  DBG(1, "reader_process: 3 pass not yet supported\n");
                  return SANE_STATUS_IO_ERROR;
                }
              else 
                {
                  switch ( mi->data_format ) 
                    {
		      case MI_DATAFMT_CHUNKY:
		      case MI_DATAFMT_9800:
                        status = chunky_proc_data(ms);
                        if ( status != SANE_STATUS_GOOD )
                            return status;
                        break;
		      case MI_DATAFMT_LPLCONCAT:
			status = lplconcat_proc_data(ms);
                        if ( status != SANE_STATUS_GOOD )
                            return status;
                        break;
		      case MI_DATAFMT_LPLSEGREG:
			status = segreg_proc_data(ms);
                        if ( status != SANE_STATUS_GOOD )
                            return status;
                        break;
		      case MI_DATAFMT_WORDCHUNKY:
                        status = wordchunky_proc_data(ms);
                        if ( status != SANE_STATUS_GOOD )
                            return status;
                        break;
                      default:
                        DBG(1, "reader_process: format %d\n", mi->data_format);
                        return SANE_STATUS_IO_ERROR;
                    }      
                }
              break;   
	    case MS_MODE_GRAY:
	      status = gray_proc_data(ms);
              if ( status != SANE_STATUS_GOOD )
                  return status;
              break;
	    case MS_MODE_HALFTONE:
	    case MS_MODE_LINEART:
              status = proc_onebit_data(ms);
              if ( status != SANE_STATUS_GOOD )
                  return status;
              break;
	    case MS_MODE_LINEARTFAKE:
              if ( ms->auto_adjust == 1 )
                  status = auto_adjust_proc_data(ms, &temp_current);
              else
                  status = lineartfake_proc_data(ms);

              if ( status != SANE_STATUS_GOOD )
                  return status;
              break;
            default:
              DBG(1, "reader_process: Unknown scan mode %d\n", ms->mode);
              return SANE_STATUS_IO_ERROR;
          }
      }

    fclose(ms->fp);
    return SANE_STATUS_GOOD;
}

/*---------- chunky_proc_data() ----------------------------------------------*/

static SANE_Status                
chunky_proc_data(Microtek2_Scanner *ms)
{
    SANE_Status status;
    Microtek2_Device *md;
    Microtek2_Info *mi;
    uint32_t line;
    uint8_t *from;
    int pad;
    int bpp;                    /* bytes per pixel */
    int bits_pp_in;             /* bits per pixel input */
    int bits_pp_out;            /* bits per pixel output */
    int bpl_ppl_diff;


    DBG(30, "chunky_proc_data: ms=%p\n", (void *) ms);
             
    md = ms->dev;
    mi = &md->info[md->scan_source];
    bits_pp_in = ms->bits_per_pixel_in;
    bits_pp_out = ms->bits_per_pixel_out;
    pad = (int) ceil( (double) (ms->ppl * bits_pp_in) / 8.0 ) % 2;
    bpp = bits_pp_out / 8;

    /* Some models have 3 * ppl + 6 bytes per line if the number of pixels */
    /* per line is even and 3 * ppl + 3 bytes per line if the number of */
    /* pixels per line is odd. According to the documentation it should be */
    /* bpl = 3*ppl (even number of pixels) or bpl=3*ppl+1 (odd number of */
    /* pixels. Even worse: On different models it is different at which */
    /* position in a scanline the image data starts. bpl_ppl_diff tries */
    /* to fix this. */

    if ( (md->model_flags & MD_OFFSET_2) && pad == 1 )
        bpl_ppl_diff = 2;
    else
        bpl_ppl_diff = 0;

#if 0
    if ( md->revision == 1.00 && mi->model_code != 0x81 )
        bpl_ppl_diff = ms->bpl - ( 3 * ms->ppl * bpp ) - pad;
    else
        bpl_ppl_diff = ms->bpl - ( 3 * ms->ppl * bpp );

    if ( md->revision > 1.00 )
        bpl_ppl_diff = ms->bpl - ( 3 * ms->ppl * bpp );
    else
        bpl_ppl_diff = ms->bpl - ( 3 * ms->ppl * bpp ) - pad;
#endif

    from = ms->buf.src_buf;
    from += bpl_ppl_diff;

    DBG(30, "chunky_proc_data: lines=%d, bpl=%d, ppl=%d, bpp=%d, depth=%d"
            " junk=%d\n", ms->src_lines_to_read, ms->bpl, ms->ppl,
             bpp, ms->depth, bpl_ppl_diff);

    for ( line = 0; line < (uint32_t) ms->src_lines_to_read; line++ )
      {
        status = chunky_copy_pixels(ms, from);
        if ( status != SANE_STATUS_GOOD )
            return status;
        from += ms->bpl;
      }

    return SANE_STATUS_GOOD;
}

/*---------- chunky_copy_pixels() --------------------------------------------*/

static SANE_Status
chunky_copy_pixels(Microtek2_Scanner *ms, uint8_t *from)
{
    Microtek2_Device *md;
    uint32_t pixel;
    int color;

    DBG(30, "chunky_copy_pixels: from=%p, pixels=%d, fp=%p, depth=%d\n",
             from, ms->ppl, (void *) ms->fp, ms->depth);

    md = ms->dev;
    if ( ms->depth > 8 )
      {
        if ( !( md->model_flags & MD_16BIT_TRANSFER ) )
          {
            int scale1;
            int scale2;
            uint16_t val16;

            scale1 = 16 - ms->depth;
            scale2 = 2 * ms->depth - 16;
            for ( pixel = 0; pixel < ms->ppl; pixel++ )
              {
                for ( color = 0; color < 3; color++ )
                  {
                    val16 = *( (uint16_t *) from + 3 * pixel + color );
                    val16 = ( val16 << scale1 ) | ( val16 >> scale2 );
                    fwrite((void *) &val16, 2, 1, ms->fp);
                  }
              }
          }
        else
          {
            fwrite((void *) from, 2, 3 * ms->ppl, ms->fp);
          }
      }
    else if ( ms->depth == 8 )
      {
        fwrite((void *) from, 1, 3 * ms->ppl, ms->fp);
      }
    else
      {
        DBG(1, "chunky_copy_pixels: Unknown depth %d\n", ms->depth);
        return SANE_STATUS_IO_ERROR;
      }

    return SANE_STATUS_GOOD;
}

/*---------- segreg_proc_data() ----------------------------------------------*/

static SANE_Status
segreg_proc_data(Microtek2_Scanner *ms)
{
    SANE_Status status;
    Microtek2_Device *md;
    Microtek2_Info *mi;
    char colormap[] = "RGB";
    uint8_t *from;
    uint32_t lines_to_deliver;
    int bpp;                    /* bytes per pixel */
    int bpf;                    /* bytes per frame including color indicator */
    int pad;
    int colseq2;
    int color;
    int save_current_src;
    int frame;
    int right_to_left;

    DBG(30, "segreg_proc_data: ms=%p\n", (void *) ms);

    md = ms->dev;
    mi = &md->info[md->scan_source];
    /* take a trailing junk byte into account */
    pad = (int) ceil( (double) (ms->ppl * ms->bits_per_pixel_in) / 8.0 ) % 2;
    bpp = ms->bits_per_pixel_out / 8; /* bits_per_pixel_out is either 8 or 16 */
    bpf = ms->bpl / 3;
    right_to_left = mi->direction & MI_DATSEQ_RTOL;

    DBG(30, "segreg_proc_data: lines=%d, bpl=%d, ppl=%d, bpf=%d, bpp=%d,\n"
            "depth=%d, pad=%d, freelines=%d, calib_backend=%d\n",
             ms->src_lines_to_read, ms->bpl, ms->ppl, bpf, bpp,
             ms->depth, pad, ms->buf.free_lines, ms->calib_backend);

    /* determine how many planes of each color are in the source buffer */
    from = ms->buf.src_buf;
    for ( frame = 0; frame < 3 * ms->src_lines_to_read; frame++, from += bpf )
      {
        switch ( *from )
          {
            case 'R':
              ++ms->buf.planes[0][MS_COLOR_RED];
              break;
            case 'G':
              ++ms->buf.planes[0][MS_COLOR_GREEN];
              break;
            case 'B':
              ++ms->buf.planes[0][MS_COLOR_BLUE];
              break;
            default:
              DBG(1, "segreg_proc_data: unknown color indicator (1) "
                     "0x%02x\n", *from);
              return SANE_STATUS_IO_ERROR;
          }
      }

    ms->buf.free_lines -= ms->src_lines_to_read;
    save_current_src = ms->buf.current_src;
    if ( ms->buf.free_lines < ms->src_max_lines )
      {
        ms->buf.current_src = !ms->buf.current_src;
        ms->buf.src_buf = ms->buf.src_buffer[ms->buf.current_src];
        ms->buf.free_lines = ms->buf.free_max_lines;
      }
    else
        ms->buf.src_buf += ms->src_lines_to_read * ms->bpl;

    colseq2 = mi->color_sequence[2];
    lines_to_deliver = ms->buf.planes[0][colseq2] + ms->buf.planes[1][colseq2];
    if ( lines_to_deliver == 0 )
        return SANE_STATUS_GOOD;

    DBG(30, "segreg_proc_data: planes[0][0]=%d, planes[0][1]=%d, "
            "planes[0][2]=%d\n", ms->buf.planes[0][0], ms->buf.planes[0][1],
             ms->buf.planes[0][2] );
    DBG(30, "segreg_proc_data: planes[1][0]=%d, planes[1][1]=%d, "
            "planes[1][2]=%d\n", ms->buf.planes[1][0], ms->buf.planes[1][1],
             ms->buf.planes[1][2] );

    while ( lines_to_deliver > 0 )
      {
        for ( color = 0; color < 3; color++ )
          {
            /* get the position of the next plane for each color */
            do
              {
                if ( *ms->buf.current_pos[color] == colormap[color] )
                    break;
                ms->buf.current_pos[color] += bpf;
              } while ( 1 );

            ms->buf.current_pos[color] += 2;    /* skip color indicator */
          }

        status = segreg_copy_pixels(ms);
        if ( status != SANE_STATUS_GOOD )
          {
            DBG(1, "segreg_copy_pixels:status %d\n", status);
            return status;
          }

        for ( color = 0; color < 3; color++ )
          {
            /* skip a padding byte at the end, if present */
            ms->buf.current_pos[color] += pad;

            if ( ms->buf.planes[1][color] > 0 )
              {
                --ms->buf.planes[1][color];
                if ( ms->buf.planes[1][color] == 0 )
                    /* we have copied from the prehold buffer and are */
                    /* done now, we continue with the source buffer */
                    ms->buf.current_pos[color] =
                                        ms->buf.src_buffer[save_current_src];
              }
            else
              {
                --ms->buf.planes[0][color];
                if ( ms->buf.planes[0][color] == 0
                     && ms->buf.current_src != save_current_src )

                    ms->buf.current_pos[color] =
                                    ms->buf.src_buffer[ms->buf.current_src];
              }
          }
        DBG(100, "planes_to_deliver=%d\n", lines_to_deliver);
        --lines_to_deliver;
      }

    if ( ms->buf.current_src != save_current_src )
      {
        for ( color = 0; color < 3; color++ )
          {
            ms->buf.planes[1][color] += ms->buf.planes[0][color];
            ms->buf.planes[0][color] = 0;
          }
      }

    DBG(30, "segreg_proc_data: src_buf=%p, free_lines=%d\n",
             ms->buf.src_buf, ms->buf.free_lines);

    return SANE_STATUS_GOOD;
}

/*---------- segreg_copy_pixels() --------------------------------------------*/

static SANE_Status
segreg_copy_pixels(Microtek2_Scanner *ms)
{
    Microtek2_Device *md;
    Microtek2_Info *mi;
    uint32_t pixel;
    int color, i, gamma_by_backend, right_to_left, scale1, scale2, bpp_in;
    float s_w, s_d;          /* shading byte from condensed_shading */
    float val, maxval = 0, shading_factor = 0;
    uint16_t val16 = 0;
    uint8_t val8 = 0;
    uint8_t *from_effective;
    uint8_t *gamma[3];
    float f[3];                            /* color balance factor */

    md = ms->dev;
    mi = &md->info[md->scan_source];
    gamma_by_backend =  md->model_flags & MD_NO_GAMMA ? 1 : 0;
    right_to_left = mi->direction & MI_DATSEQ_RTOL;
    scale1 = 16 - ms->depth;
    scale2 = 2 * ms->depth - 16;
    bpp_in = ( ms->bits_per_pixel_in + 7 ) / 8; /*Bytes per pixel from scanner*/

    if ((md->model_flags & MD_READ_CONTROL_BIT) && ms->calib_backend)
      {
        maxval = (float) pow(2.0, (float) ms->depth) - 1.0;
        s_w = maxval;
        s_d = 0.0;
        shading_factor = (float) pow(2.0, (double) (md->shading_depth
							 - ms->depth) );
      }

    if ( gamma_by_backend )
      {
        i = (ms->depth > 8) ? 2 : 1;
        for ( color = 0; color < 3; color++)
           gamma[color] = ms->gamma_table
                          + i * (int) pow(2.0, (double)ms->depth);
      }

    DBG(30, "segreg_copy_pixels: pixels=%d\n", ms->ppl);
    DBG(100, "segreg_copy_pixels: buffer 0x%p, right_to_left=%d, depth=%d\n",
	(void *) ms->buf.current_pos, right_to_left, ms->depth);

    for (color = 0; color < 3; color++ )
        f[color] = (float) ms->balance[color] / 100.0;

    DBG(100, "segreg_copy_pixels: color balance:\n"
             " ms->balance[R]=%d, ms->balance[G]=%d, ms->balance[B]=%d\n",
             ms->balance[0], ms->balance[1], ms->balance[2]);

    for ( pixel = 0; pixel < ms->ppl; pixel++ )
      {
        for ( color = 0; color < 3; color++ )
          {
            if ( right_to_left )
               from_effective = ms->buf.current_pos[color]
                                + ( ms->ppl - 1 - pixel ) * bpp_in;
            else
               from_effective = ms->buf.current_pos[color]  +  pixel * bpp_in;

            if ( ms->depth > 8 )
                val = (float) *(uint16_t *)from_effective;
            else if ( ms->depth == 8 )
                val = (float) *from_effective;
            else
            {
              DBG(1, "segreg_copy_pixels: Unknown depth %d\n", ms->depth);
              return SANE_STATUS_IO_ERROR;
            }

	    if ((md->model_flags & MD_READ_CONTROL_BIT) && ms->calib_backend
                 && ( ms->condensed_shading_w != NULL ))
                 /* apply shading by backend */
              {
                get_cshading_values(ms,
                                    color,
                                    pixel,
                                    shading_factor,
                                    right_to_left,
                                    &s_d,
                                    &s_w);


                if ( s_w == s_d ) s_w = s_d + 1;
                if ( val < s_d ) val = s_d;
                val = maxval *( val - s_d ) / ( s_w - s_d );

                val *= f[color];

                /* if scanner doesn't support brightness, contrast */
                if ( md->model_flags & MD_NO_ENHANCEMENTS )
                  {
                     val += ( ( ms->brightness_m - 128 ) * 2 );
                     val = ( val - 128 ) * ( ms->contrast_m / 128 ) + 128;
                  }

                val = MAX( 0.0, val);
                val = MIN( maxval, val );
              }
            
	    val16 = (uint16_t) val;
            val8  = (uint8_t)  val;

            /* apply gamma correction if needed */
            if ( gamma_by_backend )
              {
                if ( ms->depth > 8 )
                  val16 = *((uint16_t *) gamma[color] + val16);
                else
                  val8 = gamma[color][val8];
              }

            if ( ms->depth > 8 )
              {
                val16 = ( val16 << scale1 ) | ( val16 >> scale2 );
                fwrite((void *) &val16, 2, 1, ms->fp);
              }
            else
              {
                fputc((unsigned char) val8, ms->fp);
              }

          }
      }
    for ( color = 0; color < 3; color++ )
      {
        ms->buf.current_pos[color] += ms->ppl;
        if ( ms->depth > 8 )
            ms->buf.current_pos[color] += ms->ppl;
      }

    return SANE_STATUS_GOOD;

}


/*---------- lplconcat_proc_data() -------------------------------------------*/
static SANE_Status
lplconcat_proc_data(Microtek2_Scanner *ms)
{
    SANE_Status status;
    Microtek2_Device *md;
    Microtek2_Info *mi;
    uint32_t line;
    uint8_t *from[3];
    uint8_t *save_from[3];
    int color;
    int bpp;
    int pad;
    int gamma_by_backend;
    int right_to_left;       /* 0=left to right, 1=right to left */


    DBG(30, "lplconcat_proc_data: ms=%p\n", (void *) ms);

    /* This data format seems to honour the color sequence indicator */

    md = ms->dev;
    mi = &md->info[md->scan_source];

    bpp = ms->bits_per_pixel_out / 8; /* ms->bits_per_pixel_out is 8 or 16 */
    pad = (ms->ppl * bpp) % 2;
    right_to_left = mi->direction & MI_DATSEQ_RTOL;
    gamma_by_backend =  md->model_flags & MD_NO_GAMMA ? 1 : 0;

    if ( right_to_left == 1 )
      {
        for ( color = 0; color < 3; color++ )
          {
            from[color] = ms->buf.src_buf
                          + ( mi->color_sequence[color] + 1 ) * ( ms->bpl / 3 )
                          - bpp - (ms->bpl - 3 * ms->ppl * bpp) / 3;
          }
      }
    else
        for ( color = 0; color < 3; color++ )
            from[color] = ms->buf.src_buf
                          + mi->color_sequence[color] * ( ms->bpl / 3 );

    for ( line = 0; line < (uint32_t) ms->src_lines_to_read; line++ )
      {
        for ( color = 0 ; color < 3; color++ )
            save_from[color] = from[color];

        status = lplconcat_copy_pixels(ms,
                                       from,
                                       right_to_left,
                                       gamma_by_backend);
        if ( status != SANE_STATUS_GOOD )
            return status;

        for ( color = 0; color < 3; color++ )
            from[color] = save_from[color] + ms->bpl;
      }

    return SANE_STATUS_GOOD;
}


/*---------- lplconcat_copy_pixels() -----------------------------------------*/

static SANE_Status
lplconcat_copy_pixels(Microtek2_Scanner *ms,
                      uint8_t **from,
                      int right_to_left,
                      int gamma_by_backend)
{
  Microtek2_Device *md;
  Microtek2_Info *mi;
  uint32_t pixel;
  uint16_t val16 = 0;
  uint8_t val8 = 0;
  uint8_t *gamma[3];
  float s_d;                             /* dark shading pixel */
  float s_w;                             /* white shading pixel */
  float shading_factor = 0;
  float f[3];                            /* color balance factor */
  float val, maxval = 0;
  int color;
  int step, scale1, scale2;
  int i;


  DBG(30, "lplconcat_copy_pixels: ms=%p, righttoleft=%d, gamma=%d,\n",
           (void *) ms, right_to_left, gamma_by_backend);

  md = ms->dev;
  mi = &md->info[md->scan_source];

  if ((md->model_flags & MD_READ_CONTROL_BIT) && ms->calib_backend)
    {
      shading_factor = (float) pow(2.0,(double)(md->shading_depth - ms->depth));
      maxval = (float) pow(2.0, (double) ms->depth) - 1.0;
      s_w = maxval;
      s_d = 0.0;
    }

  step = ( right_to_left == 1 ) ? -1 : 1;
  if ( ms->depth > 8 ) step *= 2;
  scale1 = 16 - ms->depth;
  scale2 = 2 * ms->depth - 16;

  if ( gamma_by_backend )
    {
      i =  ( ms->depth > 8 ) ? 2 : 1;
      for ( color = 0; color < 3; color++ )
          gamma[color] = ms->gamma_table + i * (int) pow(2.0,(double)ms->depth);
    }

  for (color = 0; color < 3; color++ )
      f[color] = (float)ms->balance[color] / 100.0;

  DBG(100, "lplconcat_copy_pixels: color balance:\n"
             " ms->balance[R]=%d, ms->balance[G]=%d, ms->balance[B]=%d\n",
             ms->balance[0], ms->balance[1], ms->balance[2]);

  for ( pixel = 0; pixel < ms->ppl; pixel++ )
    {
      for ( color = 0; color < 3; color++ )
        {
          if ( ms->depth > 8 )
              val = (float) *(uint16_t *) from[color];
          else if ( ms->depth == 8 )
              val = (float) *from[color];
          else
            {
              DBG(1, "lplconcat_copy_pixels: Unknown depth %d\n", ms->depth);
              return SANE_STATUS_IO_ERROR;
            }
	  
	  if ((md->model_flags & MD_READ_CONTROL_BIT) && ms->calib_backend
               && ( ms->condensed_shading_w != NULL ))
               /* apply shading by backend */
            {
              get_cshading_values(ms,
                                  mi->color_sequence[color],
                                  pixel,
                                  shading_factor,
                                  right_to_left,
                                  &s_d,
                                  &s_w);

              if ( val < s_d ) val = s_d;
              if ( s_w == s_d ) s_w = s_d + 1;
              val = ( maxval * ( val - s_d ) ) / (s_w - s_d);

              val *= f[color]; /* apply color balance */

              /* if scanner doesn't support brightness, contrast ... */
              if ( md->model_flags & MD_NO_ENHANCEMENTS )
                {
                   val += ( ( ms->brightness_m - 128 ) * 2 );
                   val = ( val - 128 ) * ( ms->contrast_m / 128 ) + 128;
                }

              if ( val > maxval ) val = maxval;
              if ( val < 0.0 ) val = 0.0;
            }

          val16 = (uint16_t) val;
          val8  = (uint8_t)  val;

          /* apply gamma correction if needed */
          if ( gamma_by_backend )
            {
              if ( ms->depth > 8 )
                val16 = *((uint16_t *) gamma[color] + val16);
              else
                val8 = gamma[color][val8];
            }

          if ( ms->depth > 8 )
            {
              val16 = ( val16 << scale1 ) | ( val16 >> scale2 );
              fwrite((void *) &val16, 2, 1, ms->fp);
            }
          else
            {
              fputc((unsigned char) val8, ms->fp);
            }
          from[color] += step;
        }
    }
  return SANE_STATUS_GOOD;
}




/*---------- wordchunky_proc_data() ------------------------------------------*/

static SANE_Status
wordchunky_proc_data(Microtek2_Scanner *ms)
{
    SANE_Status status;
    uint8_t *from;
    uint32_t line;


    DBG(30, "wordchunky_proc_data: ms=%p\n", (void *) ms);

    from = ms->buf.src_buf;
    for ( line = 0; line < (uint32_t) ms->src_lines_to_read; line++ )
      {
        status = wordchunky_copy_pixels(from, ms->ppl, ms->depth, ms->fp);
        if ( status != SANE_STATUS_GOOD )
            return status;
        from += ms->bpl;
      }

    return SANE_STATUS_GOOD;
}


/*---------- wordchunky_copy_pixels() ----------------------------------------*/

static SANE_Status
wordchunky_copy_pixels(uint8_t *from, uint32_t pixels, int depth, FILE *fp)
{
    uint32_t pixel;
    int color;

    DBG(30, "wordchunky_copy_pixels: from=%p, pixels=%d, depth=%d\n",
             from, pixels, depth);

    if ( depth > 8 )
      {
        int scale1;
        int scale2;
        uint16_t val16;

        scale1 = 16 - depth;
        scale2 = 2 * depth - 16;
        for ( pixel = 0; pixel < pixels; pixel++ )
          {
            for ( color = 0; color < 3; color++ )
              {
                val16 = *(uint16_t *) from;
                val16 = ( val16 << scale1 ) | ( val16 >> scale2 );
                fwrite((void *) &val16, 2, 1, fp);
                from += 2;
              }
          }
      }
    else if ( depth == 8 )
      {
        pixel = 0;
        do
          {
            fputc((char ) *from, fp);
            fputc((char) *(from + 2), fp);
            fputc((char) *(from + 4), fp);
            ++pixel;
            if ( pixel < pixels )
              {
                fputc((char) *(from + 1), fp);
                fputc((char) *(from + 3), fp);
                fputc((char) *(from + 5), fp);
                ++pixel;
              }
            from += 6;
          } while ( pixel < pixels );
      }
    else
      {
        DBG(1, "wordchunky_copy_pixels: Unknown depth %d\n", depth);
        return SANE_STATUS_IO_ERROR;
      }

    return SANE_STATUS_GOOD;
}


/*---------- gray_proc_data() ------------------------------------------------*/

static SANE_Status
gray_proc_data(Microtek2_Scanner *ms)
{
    SANE_Status status;
    Microtek2_Device *md;
    Microtek2_Info *mi;
    uint8_t *from;
    int gamma_by_backend, bpp;
    int right_to_left;   /* for scanning direction */


    DBG(30, "gray_proc_data: lines=%d, bpl=%d, ppl=%d, depth=%d\n",
             ms->src_lines_to_read, ms->bpl, ms->ppl, ms->depth);

    md = ms->dev;
    mi = &md->info[md->scan_source];

    gamma_by_backend =  md->model_flags & MD_NO_GAMMA ? 1 : 0;
    right_to_left = mi->direction & MI_DATSEQ_RTOL;
    bpp = ( ms->bits_per_pixel_in + 7 ) / 8;

    if ( right_to_left == 1 )
      from = ms->buf.src_buf + ms->ppl * bpp - bpp;
    else
      from = ms->buf.src_buf;

    do
      {
        status = gray_copy_pixels(ms,
                                  from,
                                  right_to_left,
                                  gamma_by_backend);
        if ( status != SANE_STATUS_GOOD )
            return status;

        from += ms->bpl;
        --ms->src_lines_to_read;
      } while ( ms->src_lines_to_read > 0 );

    return SANE_STATUS_GOOD;
}


/*---------- gray_copy_pixels() ----------------------------------------------*/

static SANE_Status
gray_copy_pixels(Microtek2_Scanner *ms,
                 uint8_t *from,
                 int right_to_left,
                 int gamma_by_backend)
{
    Microtek2_Device *md;
    uint32_t pixel;
    uint16_t val16;
    uint8_t val8;
    int step, scale1, scale2;
    float val, maxval = 0;
    float s_w, s_d, shading_factor = 0;

    DBG(30, "gray_copy_pixels: pixels=%d, from=%p, fp=%p, depth=%d\n",
             ms->ppl, from, (void *) ms->fp, ms->depth);

    md = ms->dev;
    step = right_to_left == 1 ? -1 : 1;
    if ( ms->depth > 8 ) step *= 2;
    val = 0;
    scale1 = 16 - ms->depth;
    scale2 = 2 * ms->depth - 16;

    if ((md->model_flags & MD_READ_CONTROL_BIT) && ms->calib_backend)
      {
        maxval = (float) pow(2.0, (float) ms->depth) - 1.0;
        s_w = maxval;
        s_d = 0.0;
        shading_factor = (float) pow(2.0, (double) (md->shading_depth - ms->depth) );
      }

    if ( ms->depth >= 8 )
      {
        for ( pixel = 0; pixel < ms->ppl; pixel++ )
          {
            if ( ms->depth > 8 )
                val = (float) *(uint16_t *) from;
            if ( ms->depth == 8 )
                val = (float) *from;

            if ((md->model_flags & MD_READ_CONTROL_BIT) && ms->calib_backend
                 && ( ms->condensed_shading_w != NULL ))
                 /* apply shading by backend */
              {
                get_cshading_values(ms,
                                    0,
                                    pixel,
                                    shading_factor,
                                    right_to_left,
                                    &s_d,
                                    &s_w);

                if ( val < s_d ) val = s_d;
                val = ( val - s_d ) * maxval / (s_w - s_d );
                val = MAX( 0.0, val );
                val = MIN( maxval, val );
              }

            if ( ms->depth > 8 )
              {
                val16 = (uint16_t) val;
                if ( gamma_by_backend )
                    val16 = *((uint16_t *) ms->gamma_table + val16);
                if ( !( md->model_flags & MD_16BIT_TRANSFER ) )
                    val16 = ( val16 << scale1 ) | ( val16 >> scale2 );
                fwrite((void *) &val16, 2, 1, ms->fp);
              }

            if ( ms->depth == 8 )
              {
                val8  = (uint8_t)  val;
                if ( gamma_by_backend )
                    val8 =  ms->gamma_table[(int)val8];
                fputc((char)val8, ms->fp);
              }
            from += step;
          }
      }
    else if ( ms->depth == 4 )
      {
        pixel = 0;
        while ( pixel < ms->ppl )
          {
            fputc((char) ( ((*from >> 4) & 0x0f) | (*from & 0xf0) ), ms->fp);
            ++pixel;
            if ( pixel < ms->ppl )
                fputc((char) ((*from & 0x0f) | ((*from << 4) & 0xf0)), ms->fp);
            from += step;
            ++pixel;
          }
      }
    else
      {
        DBG(1, "gray_copy_pixels: Unknown depth %d\n", ms->depth);
        return SANE_STATUS_IO_ERROR;
      }

    return SANE_STATUS_GOOD;
}

/*---------- proc_onebit_data() ----------------------------------------------*/

static SANE_Status
proc_onebit_data(Microtek2_Scanner *ms)
{
    Microtek2_Device *md;
    Microtek2_Info *mi;
    uint32_t bytes_to_copy;     /* bytes per line to copy */
    uint32_t line;
    uint32_t byte;
    uint32_t ppl;
    uint8_t *from;
    uint8_t to;
    int right_to_left;
    int bit;
    int toindex;


    DBG(30, "proc_onebit_data: ms=%p\n", (void *) ms);

    md = ms->dev;
    mi = &md->info[md->scan_source];
    from = ms->buf.src_buf;
    bytes_to_copy = ( ms->ppl + 7 ) / 8 ;
    right_to_left = mi->direction & MI_DATSEQ_RTOL;

    DBG(30, "proc_onebit_data: bytes_to_copy=%d, lines=%d\n",
             bytes_to_copy, ms->src_lines_to_read);

    line = 0;
    to = 0;
    do
      {
        /* in onebit mode black and white colors are inverted */
        if ( right_to_left )
          {
            /* If the direction is right_to_left, we must skip some */
            /* trailing bits at the end of the scan line and invert the */
            /* bit sequence. We copy 8 bits into a byte, but these bits */
            /* are normally not byte aligned. */

            /* Determine the position of the first bit to copy */
            ppl = ms->ppl;
            byte = ( ppl + 7 ) / 8 - 1;
            bit = ppl % 8 - 1;
            to = 0;
            toindex = 8;

            while ( ppl > 0 )
              {
                to |= ( ( from[byte] >> (7 - bit) ) & 0x01);
                --toindex;
                if ( toindex == 0 )
                  {
                    fputc( (char) ~to, ms->fp);
                    toindex = 8;
                    to = 0;
                  }
                else
                    to <<= 1;

                --bit;
                if ( bit < 0 )
                  {
                    bit = 7;
                    --byte;
                  }
                --ppl;
              }
            /* print the last byte of the line, if it was not */
            /*  completely filled */
            bit = ms->ppl % 8;
            if ( bit != 0 )
                fputc( (char) ~(to << (7 - bit)), ms->fp);
          }
        else
            for ( byte = 0; byte < bytes_to_copy; byte++ )
                fputc( (char) ~from[byte], ms->fp);

        from += ms->bpl;

      } while ( ++line < (uint32_t) ms->src_lines_to_read );

    return SANE_STATUS_GOOD;
}


/*---------- lineartfake_proc_data() -----------------------------------------*/

static SANE_Status
lineartfake_proc_data(Microtek2_Scanner *ms)
{
    Microtek2_Device *md;
    Microtek2_Info *mi;
    SANE_Status status;
    uint8_t *from;
    int right_to_left;


    DBG(30, "lineartfake_proc_data: lines=%d, bpl=%d, ppl=%d, depth=%d\n",
             ms->src_lines_to_read, ms->bpl, ms->ppl, ms->depth);

    md = ms->dev;
    mi = &md->info[md->scan_source];
    right_to_left = mi->direction & MI_DATSEQ_RTOL;

    if ( right_to_left == 1 )
        from = ms->buf.src_buf + ms->ppl - 1;
    else
        from = ms->buf.src_buf;

    do
      {
        status = lineartfake_copy_pixels(ms,
                                         from,
                                         ms->ppl,
                                         ms->threshold,
                                         right_to_left,
                                         ms->fp);
        if ( status != SANE_STATUS_GOOD )
            return status;

        from += ms->bpl;
        --ms->src_lines_to_read;
      } while ( ms->src_lines_to_read > 0 );

    return SANE_STATUS_GOOD;
}

/*---------- lineartfake_copy_pixels() ---------------------------------------*/

static SANE_Status
lineartfake_copy_pixels(Microtek2_Scanner *ms,
                        uint8_t *from,
                        uint32_t pixels,
                        uint8_t threshold,
                        int right_to_left,
                        FILE *fp)
{
    Microtek2_Device *md;
    uint32_t pixel;
    uint32_t bit;
    uint8_t dest;
    uint8_t val;
    float s_d, s_w, maxval, shading_factor, grayval;
    int step;


    DBG(30, "lineartfake_copy_pixels: from=%p,pixels=%d,threshold=%d,file=%p\n",
             from, pixels, threshold, (void *) fp);
    md = ms->dev;
    bit = 0;
    dest = 0;
    step = right_to_left == 1 ? -1 : 1;
    maxval = 255.0;
    s_w = maxval;
    s_d = 0.0;
    shading_factor = (float) pow(2.0, (double) (md->shading_depth - 8) );

    for ( pixel = 0; pixel < pixels; pixel++ )
      {
        if ((md->model_flags & MD_READ_CONTROL_BIT) && ms->calib_backend
             && ( ms->condensed_shading_w != NULL ))
             /* apply shading by backend */
          {
            get_cshading_values(ms,
                                0,
                                pixel,
                                shading_factor,
                                right_to_left,
                                &s_d,
                                &s_w);
          }
        else    /* no shading */
          {
            s_w = maxval;
            s_d = 0.0;
          }

        grayval = (float) *from;

        if ( grayval < s_d ) grayval = s_d;
        grayval = ( grayval - s_d ) * maxval / (s_w - s_d );
        grayval = MAX( 0.0, grayval );
        grayval = MIN( maxval, grayval );

        if ( (uint8_t)grayval < threshold ) val = 1; else val = 0;
        dest = ( dest << 1 ) | val;
        bit = ( bit + 1 ) % 8;
        if ( bit == 0 )                   /* 8 input bytes processed */
          {
            fputc((char) dest, fp);
            dest = 0;
          }
        from += step;
      }

    if ( bit != 0 )
      {
        dest <<= 7 - bit;
        fputc((char) dest, fp);
      }

    return SANE_STATUS_GOOD;
}

/*---------- auto_adjust_proc_data() -----------------------------------------*/

static SANE_Status
auto_adjust_proc_data(Microtek2_Scanner *ms, uint8_t **temp_current)
{
    Microtek2_Device *md;
    Microtek2_Info *mi;
    SANE_Status status;
    uint8_t *from;
    uint32_t line;
    uint32_t lines;
    uint32_t pixel;
    uint32_t threshold;
    int right_to_left;


    DBG(30, "auto_adjust_proc_data: ms=%p, temp_current=%p\n",
            (void *) ms, *temp_current);

    md = ms->dev;
    mi = &md->info[md->scan_source];
    right_to_left = mi->direction & MI_DATSEQ_RTOL;

    memcpy(*temp_current, ms->buf.src_buf, ms->transfer_length);
    *temp_current += ms->transfer_length;
    threshold = 0;
    status = SANE_STATUS_GOOD;

    if ( ms->src_remaining_lines == 0 ) /* we have read all the image data, */
      {                                 /* calculate threshold value */
        for ( pixel = 0; pixel < ms->remaining_bytes; pixel++ )
            threshold += *(ms->temporary_buffer + pixel);

        threshold /= ms->remaining_bytes;
        lines = ms->remaining_bytes / ms->bpl;
        for ( line = 0; line < lines; line++ )
          {
            from = ms->temporary_buffer + line * ms->bpl;
            if ( right_to_left == 1 )
                from += ms->ppl - 1;
            status = lineartfake_copy_pixels(ms,
                                             from,
                                             ms->ppl,
                                             (uint8_t) threshold,
                                             right_to_left,
                                             ms->fp);
          }
        *temp_current = NULL;
      }

    return status;
}

/*-------------- get_cshading_values -----------------------------------------*/

static SANE_Status
get_cshading_values(Microtek2_Scanner *ms,
                    uint8_t color,
                    uint32_t pixel,
                    float shading_factor,
                    int right_to_left,
                    float *s_d,
                    float *s_w)
{
  Microtek2_Device *md;
  uint32_t csh_offset;

  md = ms->dev;

  if ( right_to_left == 1 )
    csh_offset = (color + 1) * ms->ppl - 1 - pixel;
  else
    csh_offset = color * ms->ppl + pixel;

  if ( ( md->shading_depth > 8 ) && ( ms->lut_entry_size == 2) )
    /* condensed shading is 2 byte color data */
    {
      if ( ms->condensed_shading_d != NULL )
          *s_d = (float) *( (uint16_t *)ms->condensed_shading_d
                                         + csh_offset );
      else
          *s_d = 0.0;

      *s_w = (float) *( (uint16_t *)ms->condensed_shading_w
                                     + csh_offset );
      *s_w /= shading_factor;
      *s_d /= shading_factor;
    }

  else
    /* condensed shading is 8 bit data */
    {
      *s_w = (float) *( ms->condensed_shading_w + csh_offset );
      if ( ms->condensed_shading_d != NULL )
        *s_d = (float) *( ms->condensed_shading_d + csh_offset );
      else
        *s_d = 0.0;
    }
  return SANE_STATUS_GOOD;
}
