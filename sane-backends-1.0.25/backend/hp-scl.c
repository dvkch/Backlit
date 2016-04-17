/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 Geoffrey T. Dairiki
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

/*
   $Log$
   Revision 1.15  2008/03/28 14:37:36  kitno-guest
   add usleep to improve usb performance, from jim a t meyering d o t net

   Revision 1.14  2004-10-04 18:09:05  kig-guest
   Rename global function hp_init_openfd to sanei_hp_init_openfd

   Revision 1.13  2004/03/27 13:52:39  kig-guest
   Keep USB-connection open (was problem with Linux 2.6.x)

   Revision 1.12  2003/10/09 19:34:57  kig-guest
   Redo when TEST UNIT READY failed
   Redo when read returns with 0 bytes (non-SCSI only)


*/

/*
#define STUBS
extern int sanei_debug_hp;*/
#define DEBUG_DECLARE_ONLY
#include "../include/sane/config.h"
#include "../include/lalloca.h"		/* Must be first */

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "../include/lassert.h"
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "../include/sane/sanei_scsi.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/sanei_pio.h"

#include "hp.h"

#include "../include/sane/sanei_backend.h"

#include "hp-option.h"
#include "hp-scsi.h"
#include "hp-scl.h"
#include "hp-device.h"

#define HP_SCSI_INQ_LEN		(36)
#define HP_SCSI_CMD_LEN		(6)
#define HP_SCSI_BUFSIZ	(HP_SCSI_MAX_WRITE + HP_SCSI_CMD_LEN)

#define HP_MAX_OPEN_FD 16
static struct hp_open_fd_s  /* structure to save info about open file descriptor */
{
    char *devname;
    HpConnect connect;
    int fd;
} asHpOpenFd[HP_MAX_OPEN_FD];


/*
 *
 */
struct hp_scsi_s
{
    int		fd;
    char      * devname;

    /* Output buffering */
    hp_byte_t	buf[HP_SCSI_BUFSIZ];
    hp_byte_t *	bufp;

    hp_byte_t	inq_data[HP_SCSI_INQ_LEN];
};

#define HP_TMP_BUF_SIZE (1024*4)
#define HP_WR_BUF_SIZE (1024*4)

typedef struct
{
  HpProcessData procdata;

  int outfd;
  const unsigned char *map;

  unsigned char *image_buf; /* Buffer to store complete image (if req.) */
  unsigned char *image_ptr;
  int image_buf_size;

  unsigned char *tmp_buf; /* Buffer for scan data to get even number of bytes */
  int tmp_buf_size;
  int tmp_buf_len;

  unsigned char wr_buf[HP_WR_BUF_SIZE];
  unsigned char *wr_ptr;
  int wr_buf_size;
  int wr_left;
} PROCDATA_HANDLE;


/* Initialize structure where we remember out open file descriptors */
void
sanei_hp_init_openfd ()
{int iCount;
 memset (asHpOpenFd, 0, sizeof (asHpOpenFd));

 for (iCount = 0; iCount < HP_MAX_OPEN_FD; iCount++)
     asHpOpenFd[iCount].fd = -1;
}


/* Look if the device is still open */
static SANE_Status
hp_GetOpenDevice (const char *devname, HpConnect connect, int *pfd)

{int iCount;

 for (iCount = 0; iCount < HP_MAX_OPEN_FD; iCount++)
     {
     if (!asHpOpenFd[iCount].devname) continue;
     if (   (strcmp (asHpOpenFd[iCount].devname, devname) == 0)
         && (asHpOpenFd[iCount].connect == connect) )
         {
         if (pfd) *pfd = asHpOpenFd[iCount].fd;
         DBG(3, "hp_GetOpenDevice: device %s is open with fd=%d\n", devname,
             asHpOpenFd[iCount].fd);
         return SANE_STATUS_GOOD;
         }
     }
 DBG(3, "hp_GetOpenDevice: device %s not open\n", devname);
 return SANE_STATUS_INVAL;
}

/* Add an open file descriptor. This also decides */
/* if we keep a connection open or not. */
static SANE_Status
hp_AddOpenDevice (const char *devname, HpConnect connect, int fd)

{int iCount, iKeepOpen;
 static int iInitKeepFlags = 1;

 /* The default values which connections to keep open or not */
 static int iKeepOpenSCSI = 0;
 static int iKeepOpenUSB = 1;
 static int iKeepOpenDevice = 0;
 static int iKeepOpenPIO = 0;

 if (iInitKeepFlags) /* Change the defaults by environment */
     {char *eptr;

     iInitKeepFlags = 0;

     eptr = getenv ("SANE_HP_KEEPOPEN_SCSI");
     if ( (eptr != NULL) && ((*eptr == '0') || (*eptr == '1')) )
         iKeepOpenSCSI = (*eptr == '1');

     eptr = getenv ("SANE_HP_KEEPOPEN_USB");
     if ( (eptr != NULL) && ((*eptr == '0') || (*eptr == '1')) )
         iKeepOpenUSB = (*eptr == '1');

     eptr = getenv ("SANE_HP_KEEPOPEN_DEVICE");
     if ( (eptr != NULL) && ((*eptr == '0') || (*eptr == '1')) )
         iKeepOpenDevice = (*eptr == '1');

     eptr = getenv ("SANE_HP_KEEPOPEN_PIO");
     if ( (eptr != NULL) && ((*eptr == '0') || (*eptr == '1')) )
         iKeepOpenPIO = (*eptr == '1');
     }

 /* Look if we should keep it open or not */
 iKeepOpen = 0;
 switch (connect)
     {
     case HP_CONNECT_SCSI: iKeepOpen = iKeepOpenSCSI;
                           break;
     case HP_CONNECT_PIO : iKeepOpen = iKeepOpenPIO;
                           break;
     case HP_CONNECT_USB : iKeepOpen = iKeepOpenUSB;
                           break;
     case HP_CONNECT_DEVICE : iKeepOpen = iKeepOpenDevice;
                           break;
     case HP_CONNECT_RESERVE:
                           break;
     }
 if (!iKeepOpen)
     {
     DBG(3, "hp_AddOpenDevice: %s should not be kept open\n", devname);
     return SANE_STATUS_INVAL;
     }

 for (iCount = 0; iCount < HP_MAX_OPEN_FD; iCount++)
     {
     if (!asHpOpenFd[iCount].devname)  /* Is this entry free ? */
         {
         asHpOpenFd[iCount].devname = sanei_hp_strdup (devname);
         if (!asHpOpenFd[iCount].devname) return SANE_STATUS_NO_MEM;
         DBG(3, "hp_AddOpenDevice: added device %s with fd=%d\n", devname, fd);
         asHpOpenFd[iCount].connect = connect;
         asHpOpenFd[iCount].fd = fd;
         return SANE_STATUS_GOOD;
         }
     }
 DBG(3, "hp_AddOpenDevice: %s not added\n", devname);
 return SANE_STATUS_NO_MEM;
}


/* Check if we have remembered an open file descriptor */
static SANE_Status
hp_IsOpenFd (int fd, HpConnect connect)

{int iCount;

 for (iCount = 0; iCount < HP_MAX_OPEN_FD; iCount++)
     {
     if (   (asHpOpenFd[iCount].devname != NULL)
         && (asHpOpenFd[iCount].fd == fd)
         && (asHpOpenFd[iCount].connect == connect) )
         {
         DBG(3, "hp_IsOpenFd: %d is open\n", fd);
         return SANE_STATUS_GOOD;
         }
     }
 DBG(3, "hp_IsOpenFd: %d not open\n", fd);
 return SANE_STATUS_INVAL;
}


static SANE_Status
hp_RemoveOpenFd (int fd, HpConnect connect)

{int iCount;

 for (iCount = 0; iCount < HP_MAX_OPEN_FD; iCount++)
     {
     if (   (asHpOpenFd[iCount].devname != NULL)
         && (asHpOpenFd[iCount].fd == fd)
         && (asHpOpenFd[iCount].connect == connect) )
         {
         sanei_hp_free (asHpOpenFd[iCount].devname);
         asHpOpenFd[iCount].devname = NULL;
         DBG(3, "hp_RemoveOpenFd: removed %d\n", asHpOpenFd[iCount].fd);
         asHpOpenFd[iCount].fd = -1;
         return SANE_STATUS_GOOD;
         }
     }
 DBG(3, "hp_RemoveOpenFd: %d not removed\n", fd);
 return SANE_STATUS_INVAL;
}


static SANE_Status
hp_nonscsi_write (HpScsi this, hp_byte_t *data, size_t len, HpConnect connect)

{int n = -1;
 size_t loc_len;
 SANE_Status status = SANE_STATUS_GOOD;

 if (len <= 0) return SANE_STATUS_GOOD;

 switch (connect)
 {
   case HP_CONNECT_DEVICE:   /* direct device-io */
     n = write (this->fd, data, len);
     break;

   case HP_CONNECT_PIO:      /* Use sanepio interface */
     n = sanei_pio_write (this->fd, data, len);
     break;

   case HP_CONNECT_USB:      /* Not supported */
     loc_len = len;
     status = sanei_usb_write_bulk ((SANE_Int)this->fd, data, &loc_len);
     n = loc_len;
     break;

   case HP_CONNECT_RESERVE:
     n = -1;
     break;

   default:
     n = -1;
     break;
 }

 if (n == 0) return SANE_STATUS_EOF;
 else if (n < 0) return SANE_STATUS_IO_ERROR;

 return status;
}

static SANE_Status
hp_nonscsi_read (HpScsi this, hp_byte_t *data, size_t *len, HpConnect connect,
  int UNUSEDARG isResponse)

{int n = -1;
 static int retries = -1;
 size_t save_len = *len;
 SANE_Status status = SANE_STATUS_GOOD;

 if (*len <= 0) return SANE_STATUS_GOOD;

 if (retries < 0)  /* Read environment */
 {char *eptr = getenv ("SANE_HP_RDREDO");

   retries = 1;       /* Set default value */
   if (eptr != NULL)
   {
     if (sscanf (eptr, "%d", &retries) != 1) retries = 1; /* Restore default */
     else if (retries < 0) retries = 0; /* Allow no retries here */
   }
 }

 for (;;) /* Retry on EOF */
 {
   switch (connect)
   {
     case HP_CONNECT_DEVICE:
       n = read (this->fd, data, *len);
       break;

     case HP_CONNECT_PIO:
       n = sanei_pio_read (this->fd, data, *len);
       break;

     case HP_CONNECT_USB:
       status = sanei_usb_read_bulk((SANE_Int)this->fd, (SANE_Byte *)data, len);
       n = *len;
       break;

     case HP_CONNECT_RESERVE:
       n = -1;
       break;

     default:
       n = -1;
       break;
   }
   if ((n != 0) || (retries <= 0)) break;
   retries--;
   usleep (100*1000);  /* sleep 0.1 seconds */
   *len = save_len;    /* Restore value */
 }

 if (n == 0) return SANE_STATUS_EOF;
 else if (n < 0) return SANE_STATUS_IO_ERROR;

 *len = n;
 return status;
}

static SANE_Status
hp_nonscsi_open (const char *devname, int *fd, HpConnect connect)

{int lfd, flags;
 SANE_Int dn;
 SANE_Status status = SANE_STATUS_INVAL;

#ifdef _O_RDWR
 flags = _O_RDWR;
#else
 flags = O_RDWR;
#endif
#ifdef _O_EXCL
 flags |= _O_EXCL;
#else
 flags |= O_EXCL;
#endif
#ifdef _O_BINARY
 flags |= _O_BINARY;
#endif
#ifdef O_BINARY
 flags |= O_BINARY;
#endif

 switch (connect)
 {
   case HP_CONNECT_DEVICE:
     lfd = open (devname, flags);
     if (lfd < 0)
     {
        DBG(1, "hp_nonscsi_open: open device %s failed (%s)\n", devname,
            strerror (errno) );
       status = (errno == EACCES) ? SANE_STATUS_ACCESS_DENIED : SANE_STATUS_INVAL;
     }
     else
       status = SANE_STATUS_GOOD;
     break;

   case HP_CONNECT_PIO:
     status = sanei_pio_open (devname, &lfd);
     break;

   case HP_CONNECT_USB:
     DBG(17, "hp_nonscsi_open: open usb with \"%s\"\n", devname);
     status = sanei_usb_open (devname, &dn);
     lfd = (int)dn;
     break;

   case HP_CONNECT_RESERVE:
     status = SANE_STATUS_INVAL;
     break;

   default:
     status = SANE_STATUS_INVAL;
     break;
 }

 if (status != SANE_STATUS_GOOD)
 {
    DBG(1, "hp_nonscsi_open: open device %s failed\n", devname);
 }
 else
 {
    DBG(17,"hp_nonscsi_open: device %s opened, fd=%d\n", devname, lfd);
 }

 if (fd) *fd = lfd;

 return status;
}

static void
hp_nonscsi_close (int fd, HpConnect connect)

{
 switch (connect)
 {
   case HP_CONNECT_DEVICE:
     close (fd);
     break;

   case HP_CONNECT_PIO:
     sanei_pio_close (fd);
     break;

   case HP_CONNECT_USB:
     sanei_usb_close (fd);
     break;

   case HP_CONNECT_RESERVE:
     break;

   default:
     break;
 }
 DBG(17,"hp_nonscsi_close: closed fd=%d\n", fd);
}

SANE_Status
sanei_hp_nonscsi_new (HpScsi * newp, const char * devname, HpConnect connect)
{
 HpScsi new;
 SANE_Status status;
 int iAlreadyOpen = 0;

  new = sanei_hp_allocz(sizeof(*new));
  if (!new)
    return SANE_STATUS_NO_MEM;

  /* Is the device already open ? */
  if ( hp_GetOpenDevice (devname, connect, &new->fd) == SANE_STATUS_GOOD )
  {
    iAlreadyOpen = 1;
  }
  else
  {
    status = hp_nonscsi_open(devname, &new->fd, connect);
    if (FAILED(status))
    {
      DBG(1, "nonscsi_new: open failed (%s)\n", sane_strstatus(status));
      sanei_hp_free(new);
      return SANE_STATUS_IO_ERROR;
    }
  }

  /* For SCSI-devices we would have the inquire command here */
  strncpy ((char *)new->inq_data, "\003zzzzzzzHP      ------          R000",
           sizeof (new->inq_data));

  new->bufp = new->buf + HP_SCSI_CMD_LEN;
  new->devname = sanei_hp_alloc ( strlen ( devname ) + 1 );
  if ( new->devname ) strcpy (new->devname, devname);

  *newp = new;

  /* Remember the open device */
  if (!iAlreadyOpen) hp_AddOpenDevice (devname, connect, new->fd);

  return SANE_STATUS_GOOD;
}

static void
hp_scsi_close (HpScsi this, int completely)
{HpConnect connect;

 DBG(3, "scsi_close: closing fd %ld\n", (long)this->fd);

 connect = sanei_hp_scsi_get_connect (this);

 if (!completely)  /* May we keep the device open ? */
 {
   if ( hp_IsOpenFd (this->fd, connect) == SANE_STATUS_GOOD )
   {
     DBG(3, "scsi_close: not closing. Keep open\n");
     return;
   }
   
 }
 assert(this->fd >= 0);

 if (connect != HP_CONNECT_SCSI)
   hp_nonscsi_close (this->fd, connect);
 else
   sanei_scsi_close (this->fd);

 DBG(3,"scsi_close: really closed\n");

 /* Remove a remembered open device */
 hp_RemoveOpenFd (this->fd, connect);
}


SANE_Status
sanei_hp_scsi_new (HpScsi * newp, const char * devname)
{
  static hp_byte_t inq_cmd[] = { 0x12, 0, 0, 0, HP_SCSI_INQ_LEN, 0};
  static hp_byte_t tur_cmd[] = { 0x00, 0, 0, 0, 0, 0};
  size_t	inq_len		= HP_SCSI_INQ_LEN;
  HpScsi	new;
  HpConnect     connect;
  SANE_Status	status;
  int iAlreadyOpen = 0;

  connect = sanei_hp_get_connect (devname);

  if (connect != HP_CONNECT_SCSI)
    return sanei_hp_nonscsi_new (newp, devname, connect);

  new = sanei_hp_allocz(sizeof(*new));
  if (!new)
      return SANE_STATUS_NO_MEM;

  /* Is the device still open ? */
  if ( hp_GetOpenDevice (devname, connect, &new->fd) == SANE_STATUS_GOOD )
  {
    iAlreadyOpen = 1;
  }
  else
  {
    status = sanei_scsi_open(devname, &new->fd, 0, 0);
    if (FAILED(status))
      {
        DBG(1, "scsi_new: open failed (%s)\n", sane_strstatus(status));
        sanei_hp_free(new);
        return SANE_STATUS_IO_ERROR;
      }
  }

  DBG(3, "scsi_inquire: sending INQUIRE\n");
  status = sanei_scsi_cmd(new->fd, inq_cmd, 6, new->inq_data, &inq_len);
  if (FAILED(status))
    {
      DBG(1, "scsi_inquire: inquiry failed: %s\n", sane_strstatus(status));
      sanei_scsi_close(new->fd);
      sanei_hp_free(new);
      return status;
    }

  {char vendor[9], model[17], rev[5];
   memset (vendor, 0, sizeof (vendor));
   memset (model, 0, sizeof (model));
   memset (rev, 0, sizeof (rev));
   memcpy (vendor, new->inq_data + 8, 8);
   memcpy (model, new->inq_data + 16, 16);
   memcpy (rev, new->inq_data + 32, 4);

   DBG(3, "vendor=%s, model=%s, rev=%s\n", vendor, model, rev);
  }

  DBG(3, "scsi_new: sending TEST_UNIT_READY\n");
  status = sanei_scsi_cmd(new->fd, tur_cmd, 6, 0, 0);
  if (FAILED(status))
    {
      DBG(1, "hp_scsi_open: test unit ready failed (%s)\n",
	  sane_strstatus(status));
      usleep (500*1000); /* Wait 0.5 seconds */
      DBG(3, "scsi_new: sending TEST_UNIT_READY second time\n");
      status = sanei_scsi_cmd(new->fd, tur_cmd, 6, 0, 0);
    }

  if (FAILED(status))
    {
      DBG(1, "hp_scsi_open: test unit ready failed (%s)\n",
	  sane_strstatus(status));

      sanei_scsi_close(new->fd);
      sanei_hp_free(new);
      return status; /* Fix problem with non-scanner devices */
    }

  new->bufp = new->buf + HP_SCSI_CMD_LEN;
  new->devname = sanei_hp_alloc ( strlen ( devname ) + 1 );
  if ( new->devname ) strcpy (new->devname, devname);

  *newp = new;

  /* Remember the open device */
  if (!iAlreadyOpen) hp_AddOpenDevice (devname, connect, new->fd);

  return SANE_STATUS_GOOD;
}



/* The "completely" parameter was added for OfficeJet support.
 * For JetDirect connections, closing and re-opening the scan
 * channel is very time consuming.  Also, the OfficeJet G85
 * unloads a loaded document in the ADF when the scan channel
 * gets closed.  The solution is to "completely" destroy the
 * connection, including closing and deallocating the PTAL
 * channel, when initially probing the device in hp-device.c,
 * but leave it open while the frontend is actually using the
 * device (from hp-handle.c), and "completely" destroy it when
 * the frontend closes its handle. */
void
sanei_hp_scsi_destroy (HpScsi this,int completely)
{
  /* Moved to hp_scsi_close():
   * assert(this->fd >= 0);
   * DBG(3, "scsi_close: closing fd %d\n", this->fd);
   */

  hp_scsi_close (this, completely);
  if ( this->devname ) sanei_hp_free (this->devname);
  sanei_hp_free(this);
}

hp_byte_t *
sanei_hp_scsi_inq (HpScsi this)
{
  return this->inq_data;
}

const char *
sanei_hp_scsi_vendor (HpScsi this)
{
  static char buf[9];
  memcpy(buf, sanei_hp_scsi_inq(this) + 8, 8);
  buf[8] = '\0';
  return buf;
}

const char *
sanei_hp_scsi_model (HpScsi this)
{

  static char buf[17];
  memcpy(buf, sanei_hp_scsi_inq(this) + 16, 16);
  buf[16] = '\0';
  return buf;
}

const char *
sanei_hp_scsi_devicename (HpScsi this)
{
  return this->devname;
}

hp_bool_t
sanei_hp_is_active_xpa (HpScsi scsi)
{HpDeviceInfo *info;
 int model_num;

 info = sanei_hp_device_info_get ( sanei_hp_scsi_devicename  (scsi) );
 if (info->active_xpa < 0)
 {
   model_num = sanei_hp_get_max_model (scsi);
   info->active_xpa = (model_num >= 17);
   DBG(5,"sanei_hp_is_active_xpa: model=%d, active_xpa=%d\n",
       model_num, info->active_xpa);
 }
 return info->active_xpa;
}

int
sanei_hp_get_max_model (HpScsi scsi)

{HpDeviceInfo *info;

 info = sanei_hp_device_info_get ( sanei_hp_scsi_devicename  (scsi) );
 if (info->max_model < 0)
 {enum hp_device_compat_e compat;
  int model_num;

   if ( sanei_hp_device_probe_model ( &compat, scsi, &model_num, 0)
            == SANE_STATUS_GOOD )
     info->max_model = model_num;
 }
 return info->max_model;
}


int
sanei_hp_is_flatbed_adf (HpScsi scsi)

{int model = sanei_hp_get_max_model (scsi);

 return ((model == 2) || (model == 4) || (model == 5) || (model == 8));
}


HpConnect
sanei_hp_get_connect (const char *devname)

{const HpDeviceInfo *info;
 HpConnect connect = HP_CONNECT_SCSI;
 int got_connect_type = 0;

 info = sanei_hp_device_info_get (devname);
 if (!info)
 {
   DBG(1, "sanei_hp_get_connect: Could not get info for %s. Assume SCSI\n",
       devname);
   connect = HP_CONNECT_SCSI;
 }
 else
 if ( !(info->config_is_up) )
 {
   DBG(1, "sanei_hp_get_connect: Config not initialized for %s. Assume SCSI\n",
       devname);
   connect = HP_CONNECT_SCSI;
 }
 else
 {
   connect = info->config.connect;
   got_connect_type = info->config.got_connect_type;
 }

 /* Beware of using a USB-device as a SCSI-device (not 100% perfect) */
 if ((connect == HP_CONNECT_SCSI) && !got_connect_type)
 {int maybe_usb;

   maybe_usb = (   strstr (devname, "usb")
                || strstr (devname, "uscanner")
                || strstr (devname, "ugen"));
   if (maybe_usb)
   {static int print_warning = 1;

     if (print_warning)
     {
       print_warning = 0;
       DBG(1,"sanei_hp_get_connect: WARNING\n");
       DBG(1,"  Device %s assumed to be SCSI, but device name\n",devname);
       DBG(1,"  looks like USB. Will continue with USB.\n");
       DBG(1,"  If you really want it as SCSI, add the following\n");
       DBG(1,"  to your file .../etc/sane.d/hp.conf:\n");
       DBG(1,"    %s\n", devname);
       DBG(1,"      option connect-scsi\n");
       DBG(1,"  The same warning applies to other device names containing\n");
       DBG(1,"  \"usb\", \"uscanner\" or \"ugen\".\n");
     }
     connect = HP_CONNECT_DEVICE;
   }
 }
 return connect;
}

HpConnect
sanei_hp_scsi_get_connect (HpScsi this)

{
 return sanei_hp_get_connect (sanei_hp_scsi_devicename (this));
}


static SANE_Status
hp_scsi_flush (HpScsi this)
{
  hp_byte_t *	data	= this->buf + HP_SCSI_CMD_LEN;
  size_t 	len 	= this->bufp - data;
  HpConnect     connect;

  assert(len < HP_SCSI_MAX_WRITE);
  if (len == 0)
      return SANE_STATUS_GOOD;

  this->bufp = this->buf;

  DBG(16, "scsi_flush: writing %lu bytes:\n", (unsigned long) len);
  DBGDUMP(16, data, len);

  *this->bufp++ = 0x0A;
  *this->bufp++ = 0;
  *this->bufp++ = len >> 16;
  *this->bufp++ = len >> 8;
  *this->bufp++ = len;
  *this->bufp++ = 0;

  connect = sanei_hp_scsi_get_connect (this);
  if (connect == HP_CONNECT_SCSI)
    return sanei_scsi_cmd (this->fd, this->buf, HP_SCSI_CMD_LEN + len, 0, 0);
  else
    return hp_nonscsi_write (this, this->buf+HP_SCSI_CMD_LEN, len, connect);
}

static size_t
hp_scsi_room (HpScsi this)
{
  return this->buf + HP_SCSI_BUFSIZ - this->bufp;
}

static SANE_Status
hp_scsi_need (HpScsi this, size_t need)
{
  assert(need < HP_SCSI_MAX_WRITE);

  if (need > hp_scsi_room(this))
      RETURN_IF_FAIL( hp_scsi_flush(this) );

  return SANE_STATUS_GOOD;
}

static SANE_Status
hp_scsi_write (HpScsi this, const void *data, size_t len)
{
  if ( len < HP_SCSI_MAX_WRITE )
    {
      RETURN_IF_FAIL( hp_scsi_need(this, len) );
      memcpy(this->bufp, data, len);
      this->bufp += len;
    }
  else
    {size_t maxwrite = HP_SCSI_MAX_WRITE - 16;
     const char *c_data = (const char *)data;

      while ( len > 0 )
        {
          if ( maxwrite > len ) maxwrite = len;
          RETURN_IF_FAIL( hp_scsi_write(this, c_data, maxwrite) );
          c_data += maxwrite;
          len -= maxwrite;
        }
    }
  return SANE_STATUS_GOOD;
}

static SANE_Status
hp_scsi_scl(HpScsi this, HpScl scl, int val)
{
  char	group	= tolower(SCL_GROUP_CHAR(scl));
  char	param	= toupper(SCL_PARAM_CHAR(scl));
  int	count;

  assert(IS_SCL_CONTROL(scl) || IS_SCL_COMMAND(scl));
  assert(isprint(group) && isprint(param));

  RETURN_IF_FAIL( hp_scsi_need(this, 10) );

  /* Dont try to optimize SCL-commands like using <ESC>*a1b0c5T */
  /* Some scanners have problems with it (e.g. HP Photosmart Photoscanner */
  /* with window position/extent, resolution) */
  count = sprintf((char *)this->bufp, "\033*%c%d%c", group, val, param);
  this->bufp += count;

  assert(count > 0 && this->bufp < this->buf + HP_SCSI_BUFSIZ);

  return hp_scsi_flush(this);
}

/* Read it bytewise */
static SANE_Status
hp_scsi_read_slow (HpScsi this, void * dest, size_t *len)
{static hp_byte_t read_cmd[6] = { 0x08, 0, 0, 0, 0, 0 };
 size_t leftover = *len;
 SANE_Status status = SANE_STATUS_GOOD;
 unsigned char *start_dest = (unsigned char *)dest;
 unsigned char *next_dest = start_dest;

 DBG(16, "hp_scsi_read_slow: Start reading %d bytes bytewise\n", (int)*len);

 while (leftover > 0)  /* Until we got all the bytes */
 {size_t one = 1;

   read_cmd[2] = 0;
   read_cmd[3] = 0;
   read_cmd[4] = 1;   /* Read one byte */

   status = sanei_scsi_cmd (this->fd, read_cmd, sizeof(read_cmd),
                            next_dest, &one);
   if ((status != SANE_STATUS_GOOD) || (one != 1))
   {
     DBG(250,"hp_scsi_read_slow: Reading byte %d: status=%s, len=%d\n",
         (int)(next_dest-start_dest), sane_strstatus(status), (int)one);
   }

   if (status != SANE_STATUS_GOOD) break;  /* Finish on error */

   next_dest++;
   leftover--;
 }

 *len = next_dest-start_dest; /* This is the number of bytes we got */

 DBG(16, "hp_scsi_read_slow: Got %d bytes\n", (int)*len);

 if ((status != SANE_STATUS_GOOD) && (*len > 0))
 {
   DBG(16, "We got some data. Ignore the error \"%s\"\n",
       sane_strstatus(status));
   status = SANE_STATUS_GOOD;
 }
 return status;
}

/* The OfficeJets tend to return inquiry responses containing array
 * data in two packets.  The added "isResponse" parameter tells
 * whether we should keep reading until we get
 * a well-formed response.  Naturally, this parameter would be zero
 * when reading scan data. */
static SANE_Status
hp_scsi_read (HpScsi this, void * dest, size_t *len, int isResponse)
{
  HpConnect connect;

  RETURN_IF_FAIL( hp_scsi_flush(this) );

  connect = sanei_hp_scsi_get_connect (this);
  if (connect == HP_CONNECT_SCSI)
  {int read_bytewise = 0;

    if (*len <= 32)   /* Is it a candidate for reading bytewise ? */
    {const HpDeviceInfo *info;

      info = sanei_hp_device_info_get (sanei_hp_scsi_devicename (this));
      if ((info != NULL) && (info->config_is_up) && info->config.dumb_read)
        read_bytewise = 1;
    }

    if ( ! read_bytewise )
    {static hp_byte_t read_cmd[6] = { 0x08, 0, 0, 0, 0, 0 };
      read_cmd[2] = *len >> 16;
      read_cmd[3] = *len >> 8;
      read_cmd[4] = *len;

      RETURN_IF_FAIL( sanei_scsi_cmd (this->fd, read_cmd,
                                      sizeof(read_cmd), dest, len) );
    }
    else
    {
      RETURN_IF_FAIL (hp_scsi_read_slow (this, dest, len));
    }
  }
  else
  {
    RETURN_IF_FAIL( hp_nonscsi_read (this, dest, len, connect, isResponse) );
  }
  DBG(16, "scsi_read:  %lu bytes:\n", (unsigned long) *len);
  DBGDUMP(16, dest, *len);
  return SANE_STATUS_GOOD;
}


static int signal_caught = 0;

static RETSIGTYPE
signal_catcher (int sig)
{
  DBG(1,"signal_catcher(sig=%d): old signal_caught=%d\n",sig,signal_caught);
  if (!signal_caught)
      signal_caught = sig;
}

static void
hp_data_map (register const unsigned char *map, register int count,
             register unsigned char *data)
{
  if (count <= 0) return;
  while (count--)
  {
    *data = map[*data];
    data++;
  }
}

static const unsigned char *
hp_get_simulation_map (const char *devname, const HpDeviceInfo *info)
{
 hp_bool_t     sim_gamma, sim_brightness, sim_contrast;
 int           k, ind;
 const unsigned char *map = NULL;
 static unsigned char map8x8[256];

  sim_gamma = info->simulate.gamma_simulate;
  sim_brightness = sanei_hp_device_simulate_get (devname, SCL_BRIGHTNESS);
  sim_contrast = sanei_hp_device_simulate_get (devname, SCL_CONTRAST);

  if ( sim_gamma )
  {
    map = &(info->simulate.gamma_map[0]);
  }
  else if ( sim_brightness && sim_contrast )
  {
    for (k = 0; k < 256; k++)
    {
      ind = info->simulate.contrast_map[k];
      map8x8[k] = info->simulate.brightness_map[ind];
    }
    map = &(map8x8[0]);
  }
  else if ( sim_brightness )
    map = &(info->simulate.brightness_map[0]);
  else if ( sim_contrast )
    map = &(info->simulate.contrast_map[0]);

  return map;
}


/* Check the native byte order on the local machine */
static hp_bool_t
is_lowbyte_first_byteorder (void)

{unsigned short testvar = 1;
 unsigned char *testptr = (unsigned char *)&testvar;

 if (sizeof (unsigned short) == 2)
   return (testptr[0] == 1);
 else if (sizeof (unsigned short) == 4)
   return ((testptr[0] == 1) || (testptr[2] == 1));
 else
   return (   (testptr[0] == 1) || (testptr[2] == 1)
           || (testptr[4] == 1) || (testptr[6] == 1));
}

/* The SANE standard defines that 2-byte data must use the full 16 bit range.
 * Byte order returned by the backend must be native byte order.
 * Scaling to 16 bit and byte order is achived by hp_scale_to_16bit.
 * for >8 bits data, take the two data bytes and scale their content
 * to the full 16 bit range, using
 *     scaled = unscaled << (newlen - oldlen) +
 *              unscaled >> (oldlen - (newlen - oldlen)),
 * with newlen=16 and oldlen the original bit depth.
 */
static void
hp_scale_to_16bit(int count, register unsigned char *data, int depth,
                  hp_bool_t invert)
{
    register unsigned int tmp;
    register unsigned int mask;
    register hp_bool_t lowbyte_first = is_lowbyte_first_byteorder ();
    unsigned int shift1 = 16 - depth;
    unsigned int shift2 = 2*depth - 16;
    int k;

    if (count <= 0) return;

    mask = 1;
    for (k = 1; k < depth; k++) mask |= (1 << k);

    if (lowbyte_first)
    {
      while (count--) {
         tmp = ((((unsigned int)data[0])<<8) | ((unsigned int)data[1])) & mask;
         tmp = (tmp << shift1) + (tmp >> shift2);
         if (invert) tmp = ~tmp;
         *data++ = tmp & 255U;
         *data++ = (tmp >> 8) & 255U;
      }
    }
    else  /* Highbyte first */
    {
      while (count--) {
         tmp = ((((unsigned int)data[0])<<8) | ((unsigned int)data[1])) & mask;
         tmp = (tmp << shift1) + (tmp >> shift2);
         if (invert) tmp = ~tmp;
         *data++ = (tmp >> 8) & 255U;
         *data++ = tmp & 255U;
      }
    }
}


static void
hp_scale_to_8bit(int count, register unsigned char *data, int depth,
                 hp_bool_t invert)
{
    register unsigned int tmp, mask;
    register hp_bool_t lowbyte_first = is_lowbyte_first_byteorder ();
    unsigned int shift1 = depth-8;
    int k;
    unsigned char *dataout = data;

    if ((count <= 0) || (shift1 <= 0)) return;

    mask = 1;
    for (k = 1; k < depth; k++) mask |= (1 << k);

    if (lowbyte_first)
    {
      while (count--) {
         tmp = ((((unsigned int)data[0])<<8) | ((unsigned int)data[1])) & mask;
         tmp >>= shift1;
         if (invert) tmp = ~tmp;
         *(dataout++) = tmp & 255U;
         data += 2;
      }
    }
    else  /* Highbyte first */
    {
      while (count--) {
         tmp = ((((unsigned int)data[0])<<8) | ((unsigned int)data[1])) & mask;
         tmp >>= shift1;
         if (invert) tmp = ~tmp;
         *(dataout++) = tmp & 255U;
         data += 2;
      }
    }
}

static void
hp_soft_invert(int count, register unsigned char *data) {
	while (count>0) {
		*data = ~(*data);
		data++;
		count--;
	}
}

static PROCDATA_HANDLE *
process_data_init (HpProcessData *procdata, const unsigned char *map,
                   int outfd, hp_bool_t use_imgbuf)

{PROCDATA_HANDLE *ph = sanei_hp_alloc (sizeof (PROCDATA_HANDLE));
 int tsz;

 if (ph == NULL) return NULL;

 memset (ph, 0, sizeof (*ph));
 memcpy (&(ph->procdata), procdata, sizeof (*procdata));
 procdata = &(ph->procdata);

 tsz = (HP_TMP_BUF_SIZE <= 0) ? procdata->bytes_per_line : HP_TMP_BUF_SIZE;
 ph->tmp_buf = sanei_hp_alloc (tsz);
 if (ph->tmp_buf == NULL)
 {
   sanei_hp_free (ph);
   return NULL;
 }
 ph->tmp_buf_size = tsz;
 ph->tmp_buf_len = 0;

 ph->map = map;
 ph->outfd = outfd;

 if ( procdata->mirror_vertical || use_imgbuf)
 {
   tsz = procdata->lines*procdata->bytes_per_line;
   if (procdata->out8) tsz /= 2;
   ph->image_ptr = ph->image_buf = sanei_hp_alloc (tsz);
   if ( !ph->image_buf )
   {
     procdata->mirror_vertical = 0;
     ph->image_buf_size = 0;
     DBG(1, "process_scanline_init: Not enough memory to mirror image\n");
   }
   else
     ph->image_buf_size = tsz;
 }

 ph->wr_ptr = ph->wr_buf;
 ph->wr_buf_size = ph->wr_left = sizeof (ph->wr_buf);

 return ph;
}


static SANE_Status
process_data_write (PROCDATA_HANDLE *ph, unsigned char *data, int nbytes)

{int ncopy;

 if (ph == NULL) return SANE_STATUS_INVAL;

 /* Fill up write buffer */
 ncopy = ph->wr_left;
 if (ncopy > nbytes) ncopy = nbytes;

 memcpy (ph->wr_ptr, data, ncopy);
 ph->wr_ptr += ncopy;
 ph->wr_left -= ncopy;
 data += ncopy;
 nbytes -= ncopy;

 if ( ph->wr_left > 0 )  /* Did not fill up the write buffer ? Finished */
   return SANE_STATUS_GOOD;

 DBG(12, "process_data_write: write %d bytes\n", ph->wr_buf_size);
 /* Don't write data if we got a signal in the meantime */
 if (   signal_caught
     || (write (ph->outfd, ph->wr_buf, ph->wr_buf_size) != ph->wr_buf_size))
 {
   DBG(1, "process_data_write: write failed: %s\n",
       signal_caught ? "signal caught" : strerror(errno));
   return SANE_STATUS_IO_ERROR;
 }
 ph->wr_ptr = ph->wr_buf;
 ph->wr_left = ph->wr_buf_size;

 /* For large amount of data write it from data-buffer */
 while ( nbytes > ph->wr_buf_size )
 {
   if (   signal_caught
       || (write (ph->outfd, data, ph->wr_buf_size) != ph->wr_buf_size))
   {
     DBG(1, "process_data_write: write failed: %s\n",
         signal_caught ? "signal caught" : strerror(errno));
     return SANE_STATUS_IO_ERROR;
   }
   nbytes -= ph->wr_buf_size;
   data += ph->wr_buf_size;
 }

 if ( nbytes > 0 ) /* Something left ? Save it to (empty) write buffer */
 {
   memcpy (ph->wr_ptr, data, nbytes);
   ph->wr_ptr += nbytes;
   ph->wr_left -= nbytes;
 }
 return SANE_STATUS_GOOD;
}

static SANE_Status
process_scanline (PROCDATA_HANDLE *ph, unsigned char *linebuf,
                  int bytes_per_line)

{int out_bytes_per_line = bytes_per_line;
 HpProcessData *procdata;

 if (ph == NULL) return SANE_STATUS_INVAL;
 procdata = &(ph->procdata);

 if ( ph->map )
   hp_data_map (ph->map, bytes_per_line, linebuf);

 if (procdata->bits_per_channel > 8)
 {
   if (procdata->out8)
   {
     hp_scale_to_8bit( bytes_per_line/2, linebuf,
                       procdata->bits_per_channel,
                       procdata->invert);
     out_bytes_per_line /= 2;
   }
   else
   {
     hp_scale_to_16bit( bytes_per_line/2, linebuf,
                        procdata->bits_per_channel,
                        procdata->invert);
   }
 } else if (procdata->invert) {
   hp_soft_invert(bytes_per_line,linebuf);
 }

 if ( ph->image_buf )
 {
   DBG(5, "process_scanline: save in memory\n");

   if (    ph->image_ptr+out_bytes_per_line-1
        <= ph->image_buf+ph->image_buf_size-1 )
   {
     memcpy(ph->image_ptr, linebuf, out_bytes_per_line);
     ph->image_ptr += out_bytes_per_line;
   }
   else
   {
     DBG(1, "process_scanline: would exceed image buffer\n");
   }
 }
 else /* Save scanlines in a bigger buffer. */
 {    /* Otherwise we will get performance problems */

   RETURN_IF_FAIL ( process_data_write (ph, linebuf, out_bytes_per_line) );
 }
 return SANE_STATUS_GOOD;
}


static SANE_Status
process_data (PROCDATA_HANDLE *ph, unsigned char *read_ptr, int nread)

{int bytes_left;
 HpProcessData *procdata;

 if (nread <= 0) return SANE_STATUS_GOOD;

 if (ph == NULL) return SANE_STATUS_INVAL;

 procdata = &(ph->procdata);
 if ( ph->tmp_buf_len > 0 )  /* Something left ? */
 {
   bytes_left = ph->tmp_buf_size - ph->tmp_buf_len;
   if (nread < bytes_left)  /* All to buffer ? */
   {
     memcpy (ph->tmp_buf+ph->tmp_buf_len, read_ptr, nread);
     ph->tmp_buf_len += nread;
     return SANE_STATUS_GOOD;
   }
   memcpy (ph->tmp_buf+ph->tmp_buf_len, read_ptr, bytes_left);
   read_ptr += bytes_left;
   nread -= bytes_left;
   RETURN_IF_FAIL ( process_scanline (ph, ph->tmp_buf, ph->tmp_buf_size) );
   ph->tmp_buf_len = 0;
 }
 while (nread > 0)
 {
   if (nread >= ph->tmp_buf_size)
   {
     RETURN_IF_FAIL ( process_scanline (ph, read_ptr, ph->tmp_buf_size) );
     read_ptr += ph->tmp_buf_size;
     nread -= ph->tmp_buf_size;
   }
   else
   {
     memcpy (ph->tmp_buf, read_ptr, nread);
     ph->tmp_buf_len = nread;
     nread = 0;
   }
 }
 return SANE_STATUS_GOOD;
}


static SANE_Status
process_data_flush (PROCDATA_HANDLE *ph)

{SANE_Status status = SANE_STATUS_GOOD;
 HpProcessData *procdata;
 unsigned char *image_data;
 size_t image_len;
 int num_lines, bytes_per_line;
 int nbytes;

 if (ph == NULL) return SANE_STATUS_INVAL;

 if ( ph->tmp_buf_len > 0 )
   process_scanline (ph, ph->tmp_buf, ph->tmp_buf_len);

 if ( ph->wr_left != ph->wr_buf_size ) /* Something in write buffer ? */
 {
   nbytes = ph->wr_buf_size - ph->wr_left;
   if ( signal_caught || (write (ph->outfd, ph->wr_buf, nbytes) != nbytes))
   {
     DBG(1, "process_data_flush: write failed: %s\n",
         signal_caught ? "signal caught" : strerror(errno));
     return SANE_STATUS_IO_ERROR;
   }
   ph->wr_ptr = ph->wr_buf;
   ph->wr_left = ph->wr_buf_size;
 }

 procdata = &(ph->procdata);
 if ( ph->image_buf )
 {
   bytes_per_line = procdata->bytes_per_line;
   if (procdata->out8) bytes_per_line /= 2;
   image_len = (size_t) (ph->image_ptr - ph->image_buf);
   num_lines = ((int)(image_len + bytes_per_line-1)) / bytes_per_line;

   DBG(3, "process_data_finish: write %d bytes from memory...\n",
       (int)image_len);

   if ( procdata->mirror_vertical )
   {
     image_data = ph->image_buf + (num_lines-1) * bytes_per_line;
     while (num_lines > 0 )
     {
       if (   signal_caught
           || (write(ph->outfd, image_data, bytes_per_line) != bytes_per_line))
       {
         DBG(1,"process_data_finish: write from memory failed: %s\n",
             signal_caught ? "signal caught" : strerror(errno));
         status = SANE_STATUS_IO_ERROR;
         break;
       }
       num_lines--;
       image_data -= bytes_per_line;
     }
   }
   else
   {
     image_data = ph->image_buf;
     while (num_lines > 0 )
     {
       if (   signal_caught
           || (write(ph->outfd, image_data, bytes_per_line) != bytes_per_line))
       {
         DBG(1,"process_data_finish: write from memory failed: %s\n",
             signal_caught ? "signal caught" : strerror(errno));
         status = SANE_STATUS_IO_ERROR;
         break;
       }
       num_lines--;
       image_data += bytes_per_line;
     }
   }
 }
 return status;
}


static void
process_data_finish (PROCDATA_HANDLE *ph)

{
 DBG(12, "process_data_finish called\n");

 if (ph == NULL) return;

 if (ph->image_buf != NULL) sanei_hp_free (ph->image_buf);

 sanei_hp_free (ph->tmp_buf);
 sanei_hp_free (ph);
}


SANE_Status
sanei_hp_scsi_pipeout (HpScsi this, int outfd, HpProcessData *procdata)
{
  /* We will catch these signals, and rethrow them after cleaning up,
   * anything not in this list, we will ignore. */
  static int kill_sig[] = {
      SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGPIPE, SIGALRM, SIGTERM,
      SIGUSR1, SIGUSR2, SIGBUS,
#ifdef SIGSTKFLT
      SIGSTKFLT,
#endif
#ifdef SIGIO
      SIGIO,
#else
# ifdef SIGPOLL
      SIGPOLL,
# endif
#endif
#ifdef SIGXCPU
      SIGXCPU,
#endif
#ifdef SIGXFSZ
      SIGXFSZ,
#endif
#ifdef SIGVTALRM
      SIGVTALRM,
#endif
#ifdef SIGPWR
      SIGPWR,
#endif
  };
#define HP_NSIGS (sizeof(kill_sig)/sizeof(kill_sig[0]))
  struct SIGACTION old_handler[HP_NSIGS];
  struct SIGACTION sa;
  sigset_t	old_set, sig_set;
  int		i;
  int           bits_per_channel = procdata->bits_per_channel;

#define HP_PIPEBUF	32768
  SANE_Status	status	= SANE_STATUS_GOOD;
  struct {
      size_t	len;
      void *	id;
      hp_byte_t	cmd[6];
      hp_byte_t	data[HP_PIPEBUF];
  } 	buf[2], *req = NULL;

  int		reqs_completed = 0;
  int		reqs_issued = 0;
  char          *image_buf = 0;
  char          *read_buf = 0;
  const HpDeviceInfo *info;
  const char    *devname = sanei_hp_scsi_devicename (this);
  int           enable_requests = 1;
  int           enable_image_buffering = 0;
  const unsigned char *map = NULL;
  HpConnect     connect;
  PROCDATA_HANDLE *ph = NULL;
  size_t count = procdata->lines * procdata->bytes_per_line;

  RETURN_IF_FAIL( hp_scsi_flush(this) );

  connect = sanei_hp_get_connect (devname);
  info = sanei_hp_device_info_get (devname);

  assert (info);

  if ( info->config_is_up )
  {
    enable_requests = info->config.use_scsi_request;
    enable_image_buffering = info->config.use_image_buffering;
  }
  else
  {
    enable_requests = 0;
  }

  if (connect != HP_CONNECT_SCSI)
    enable_requests = 0;

  /* Currently we can only simulate 8 bits mapping */
  if (bits_per_channel == 8)
    map = hp_get_simulation_map (devname, info);

  sigfillset(&sig_set);
  sigprocmask(SIG_BLOCK, &sig_set, &old_set);

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = signal_catcher;
  sigfillset(&sa.sa_mask);

  sigemptyset(&sig_set);
  for (i = 0; i < (int)(HP_NSIGS); i++)
    {
      sigaction(kill_sig[i], &sa, &old_handler[i]);
      sigaddset(&sig_set, kill_sig[i]);
    }
  signal_caught = 0;
  sigprocmask(SIG_UNBLOCK, &sig_set, 0);

  /* Wait for front button push ? */
  if ( procdata->startscan )
  {
    for (;;)
    {int val = 0;

       if (signal_caught) goto quit;
       sanei_hp_scl_inquire (this, SCL_FRONT_BUTTON, &val, 0, 0);
       if (val) break;
       usleep ((unsigned long)333*1000); /* Wait 1/3 second */
    }
    status = sanei_hp_scl_startScan (this, procdata->startscan);
    if (status != SANE_STATUS_GOOD )
    {
      DBG(1, "do_read: Error starting scan in reader process\n");
      goto quit;
    }
  }
  ph = process_data_init (procdata, map, outfd, enable_image_buffering);

  if ( ph == NULL )
  {
    DBG(1, "do_read: Error with process_data_init()\n");
    goto quit;
  }

  DBG(1, "do_read: Start reading data from scanner\n");

  if (enable_requests)   /* Issue SCSI-requests ? */
  {
    while (count > 0 || reqs_completed < reqs_issued)
    {
      while (count > 0 && reqs_issued < reqs_completed + 2)
	{
	  req = buf + (reqs_issued++ % 2);

	  req->len = HP_PIPEBUF;
	  if (count < req->len)
	      req->len = count;
	  count -= req->len;

	  req->cmd[0] = 0x08;
	  req->cmd[1] = 0;
	  req->cmd[2] = req->len >> 16;
	  req->cmd[3] = req->len >> 8;
	  req->cmd[4] = req->len;
	  req->cmd[5] = 0;

	  DBG(3, "do_read: entering request to read %lu bytes\n",
	      (unsigned long) req->len);

	  status = sanei_scsi_req_enter(this->fd, req->cmd, 6,
				      req->data, &req->len, &req->id);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG(1, "do_read: Error from scsi_req_enter: %s\n",
		  sane_strstatus(status));
	      goto quit;
	    }
	  if (signal_caught)
	      goto quit;
	}

      if (signal_caught)
	  goto quit;

      assert(reqs_completed < reqs_issued);
      req = buf + (reqs_completed++ % 2);

      DBG(3, "do_read: waiting for data\n");
      status = sanei_scsi_req_wait(req->id);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG(1, "do_read: Error from scsi_req_wait: %s\n",
	      sane_strstatus(status));
	  goto quit;
	}
      if (signal_caught)
	  goto quit;

      status = process_data (ph, (unsigned char *)req->data, (int)req->len);
      if ( status != SANE_STATUS_GOOD )
      {
        DBG(1,"do_read: Error in process_data\n");
        goto quit;
      }
    }
  }
  else  /* Read directly */
  {
    read_buf = sanei_hp_alloc ( HP_PIPEBUF );
    if (!read_buf)
    {
      DBG(1, "do_read: not enough memory for read buffer\n");
      goto quit;
    }

    while (count > 0)
    {size_t nread;

      if (signal_caught)
	  goto quit;

      DBG(5, "do_read: %lu bytes left to read\n", (unsigned long)count);

      nread = HP_PIPEBUF;
      if (nread > count) nread = count;

      DBG(3, "do_read: try to read data (%lu bytes)\n", (unsigned long)nread);

      status = hp_scsi_read (this, read_buf, &nread, 0);
      if (status != SANE_STATUS_GOOD)
      {
        DBG(1, "do_read: Error from scsi_read: %s\n",sane_strstatus(status));
        goto quit;
      }

      DBG(3, "do_read: got %lu bytes\n", (unsigned long)nread);

      if (nread <= 0)
      {
        DBG(1, "do_read: Nothing read\n");
        continue;
      }

      status = process_data (ph, (unsigned char *)read_buf, (int)nread);
      if ( status != SANE_STATUS_GOOD )
      {
        DBG(1,"do_read: Error in process_data\n");
        goto quit;
      }
      count -= nread;
    }
  }

  process_data_flush (ph);

quit:

  process_data_finish (ph);

  if ( image_buf ) sanei_hp_free ( image_buf );
  if ( read_buf ) sanei_hp_free ( read_buf );

  if (enable_requests && (reqs_completed < reqs_issued))
    {
      DBG(1, "do_read: cleaning up leftover requests\n");
      while (reqs_completed < reqs_issued)
	{
	  req = buf + (reqs_completed++ % 2);
	  sanei_scsi_req_wait(req->id);
	}
    }

  sigfillset(&sig_set);
  sigprocmask(SIG_BLOCK, &sig_set, 0);
  for (i = 0; i < (int)(HP_NSIGS); i++)
      sigaction(kill_sig[i], &old_handler[i], 0);
  sigprocmask(SIG_SETMASK, &old_set, 0);

  if (signal_caught)
    {
      DBG(1, "do_read: caught signal %d\n", signal_caught);
      raise(signal_caught);
      return SANE_STATUS_CANCELLED;
    }

  return status;
}



/*
 *
 */

static SANE_Status
_hp_scl_inq (HpScsi scsi, HpScl scl, HpScl inq_cmnd,
	     void *valp, size_t *lengthp)
{
  size_t	bufsize	= 16 + (lengthp ? *lengthp: 0);
  char *	buf	= alloca(bufsize);
  char		expect[16], expect_char;
  int		val, count;
  SANE_Status	status;

  if (!buf)
      return SANE_STATUS_NO_MEM;

  /* Flush data before sending inquiry. */
  /* Otherwise scanner might not generate a response. */
  RETURN_IF_FAIL( hp_scsi_flush (scsi)) ;

  RETURN_IF_FAIL( hp_scsi_scl(scsi, inq_cmnd, SCL_INQ_ID(scl)) );
  usleep (1000); /* 500 works, too, but not 100 */

  status =  hp_scsi_read(scsi, buf, &bufsize, 1);
  if (FAILED(status))
    {
      DBG(1, "scl_inq: read failed (%s)\n", sane_strstatus(status));
      return status;
    }

  if (SCL_PARAM_CHAR(inq_cmnd) == 'R')
      expect_char = 'p';
  else
      expect_char = tolower(SCL_PARAM_CHAR(inq_cmnd) - 1);

  count = sprintf(expect, "\033*s%d%c", SCL_INQ_ID(scl), expect_char);
  if (memcmp(buf, expect, count) != 0)
    {
      DBG(1, "scl_inq: malformed response: expected '%s', got '%.*s'\n",
	  expect, count, buf);
      return SANE_STATUS_IO_ERROR;
    }
  buf += count;

  if (buf[0] == 'N')
    {				/* null response */
      DBG(3, "scl_inq: parameter %d unsupported\n", SCL_INQ_ID(scl));
      return SANE_STATUS_UNSUPPORTED;
    }

  if (sscanf(buf, "%d%n", &val, &count) != 1)
    {
      DBG(1, "scl_inq: malformed response: expected int, got '%.8s'\n", buf);
      return SANE_STATUS_IO_ERROR;
    }
  buf += count;

  expect_char = lengthp ? 'W' : 'V';
  if (*buf++ != expect_char)
    {
      DBG(1, "scl_inq: malformed response: expected '%c', got '%.4s'\n",
	  expect_char, buf - 1);
      return SANE_STATUS_IO_ERROR;
    }

  if (!lengthp)
      *(int *)valp = val; /* Get integer value */
  else
    {
      if (val > (int)*lengthp)
	{
	  DBG(1, "scl_inq: inquiry returned %d bytes, expected <= %lu\n",
	      val, (unsigned long) *lengthp);
	  return SANE_STATUS_IO_ERROR;
	}
      *lengthp = val;
      memcpy(valp, buf , *lengthp); /* Get binary data */
    }

  return SANE_STATUS_GOOD;
}


SANE_Status
sanei_hp_scl_upload_binary (HpScsi scsi, HpScl scl, size_t *lengthhp,
                            char **bufhp)
{
  size_t	bufsize	= 16, sv;
  char *	buf	= alloca(bufsize);
  char *        bufstart = buf;
  char *        hpdata;
  char		expect[16], expect_char;
  int		n, val, count;
  SANE_Status	status;

  if (!buf)
      return SANE_STATUS_NO_MEM;

  assert ( IS_SCL_DATA_TYPE (scl) );

  /* Flush data before sending inquiry. */
  /* Otherwise scanner might not generate a response. */
  RETURN_IF_FAIL( hp_scsi_flush (scsi)) ;

  RETURN_IF_FAIL( hp_scsi_scl(scsi, SCL_UPLOAD_BINARY_DATA, SCL_INQ_ID(scl)) );

  status =  hp_scsi_read(scsi, buf, &bufsize, 0);
  if (FAILED(status))
    {
      DBG(1, "scl_upload_binary: read failed (%s)\n", sane_strstatus(status));
      return status;
    }

  expect_char = 't';
  count = sprintf(expect, "\033*s%d%c", SCL_INQ_ID(scl), expect_char);
  if (memcmp(buf, expect, count) != 0)
    {
      DBG(1, "scl_upload_binary: malformed response: expected '%s', got '%.*s'\n",
	  expect, count, buf);
      return SANE_STATUS_IO_ERROR;
    }
  buf += count;

  if (buf[0] == 'N')
    {				/* null response */
      DBG(1, "scl_upload_binary: parameter %d unsupported\n", SCL_INQ_ID(scl));
      return SANE_STATUS_UNSUPPORTED;
    }

  if (sscanf(buf, "%d%n", &val, &count) != 1)
    {
      DBG(1, "scl_inq: malformed response: expected int, got '%.8s'\n", buf);
      return SANE_STATUS_IO_ERROR;
    }
  buf += count;

  expect_char = 'W';
  if (*buf++ != expect_char)
    {
      DBG(1, "scl_inq: malformed response: expected '%c', got '%.4s'\n",
	  expect_char, buf - 1);
      return SANE_STATUS_IO_ERROR;
    }

  *lengthhp = val;
  *bufhp = hpdata = sanei_hp_alloc ( val );
  if (!hpdata)
      return SANE_STATUS_NO_MEM;

  if (buf < bufstart + bufsize)
    {
       n = bufsize - (buf - bufstart);
       if (n > val) n = val;
       memcpy (hpdata, buf, n);
       hpdata += n;
       val -= n;
    }

  status = SANE_STATUS_GOOD;
  if ( val > 0 )
    {
      sv = val;
      status = hp_scsi_read(scsi, hpdata, &sv, 0);
      if (status != SANE_STATUS_GOOD)
        sanei_hp_free ( *bufhp );
    }

  return status;
}


SANE_Status
sanei_hp_scl_set(HpScsi scsi, HpScl scl, int val)
{
  RETURN_IF_FAIL( hp_scsi_scl(scsi, scl, val) );


#ifdef PARANOID
  RETURN_IF_FAIL( sanei_hp_scl_errcheck(scsi) );
#endif

  return SANE_STATUS_GOOD;
}

SANE_Status
sanei_hp_scl_inquire(HpScsi scsi, HpScl scl, int * valp, int * minp, int * maxp)
{
  HpScl	inquiry = ( IS_SCL_CONTROL(scl)
		    ? SCL_INQUIRE_PRESENT_VALUE
		    : SCL_INQUIRE_DEVICE_PARAMETER );

  assert(IS_SCL_CONTROL(scl) || IS_SCL_PARAMETER(scl));
  assert(IS_SCL_CONTROL(scl) || (!minp && !maxp));

  if (valp)
      RETURN_IF_FAIL( _hp_scl_inq(scsi, scl, inquiry, valp, 0) );
  if (minp)
      RETURN_IF_FAIL( _hp_scl_inq(scsi, scl,
				  SCL_INQUIRE_MINIMUM_VALUE, minp, 0) );
  if (maxp)
      RETURN_IF_FAIL( _hp_scl_inq(scsi, scl,
				  SCL_INQUIRE_MAXIMUM_VALUE, maxp, 0) );
  return SANE_STATUS_GOOD;
}

#ifdef _HP_NOT_USED
static SANE_Status
hp_scl_get_bounds(HpScsi scsi, HpScl scl, int * minp, int * maxp)
{
  assert(IS_SCL_CONTROL(scl));
  RETURN_IF_FAIL( _hp_scl_inq(scsi, scl, SCL_INQUIRE_MINIMUM_VALUE, minp, 0) );
  return _hp_scl_inq(scsi, scl, SCL_INQUIRE_MAXIMUM_VALUE, maxp, 0);
}
#endif

#ifdef _HP_NOT_USED
static SANE_Status
hp_scl_get_bounds_and_val(HpScsi scsi, HpScl scl,
			  int * minp, int * maxp, int * valp)
{
  assert(IS_SCL_CONTROL(scl));
  RETURN_IF_FAIL( _hp_scl_inq(scsi, scl, SCL_INQUIRE_MINIMUM_VALUE, minp, 0) );
  RETURN_IF_FAIL( _hp_scl_inq(scsi, scl, SCL_INQUIRE_MAXIMUM_VALUE, maxp, 0) );
  return    _hp_scl_inq(scsi, scl, SCL_INQUIRE_PRESENT_VALUE, valp, 0);
}
#endif

SANE_Status
sanei_hp_scl_download(HpScsi scsi, HpScl scl, const void * valp, size_t len)
{
  assert(IS_SCL_DATA_TYPE(scl));

  sanei_hp_scl_clearErrors ( scsi );
  RETURN_IF_FAIL( hp_scsi_need(scsi, 16) );
  RETURN_IF_FAIL( hp_scsi_scl(scsi, SCL_DOWNLOAD_TYPE, SCL_INQ_ID(scl)) );
                            /* Download type not supported ? */
  RETURN_IF_FAIL( sanei_hp_scl_errcheck(scsi) );
  RETURN_IF_FAIL( hp_scsi_scl(scsi, SCL_DOWNLOAD_LENGTH, len) );
  RETURN_IF_FAIL( hp_scsi_write(scsi, valp, len) );

#ifdef PARANOID
  RETURN_IF_FAIL( sanei_hp_scl_errcheck(scsi) );
#endif

  return SANE_STATUS_GOOD;
}

SANE_Status
sanei_hp_scl_upload(HpScsi scsi, HpScl scl, void * valp, size_t len)
{
  size_t	nread = len;
  HpScl		inquiry = ( IS_SCL_DATA_TYPE(scl)
			    ? SCL_UPLOAD_BINARY_DATA
			    : SCL_INQUIRE_DEVICE_PARAMETER );

  assert(IS_SCL_DATA_TYPE(scl) || IS_SCL_PARAMETER(scl));

  RETURN_IF_FAIL( _hp_scl_inq(scsi, scl, inquiry, valp, &nread) );
  if (IS_SCL_PARAMETER(scl) && nread < len)
      ((char *)valp)[nread] = '\0';
  else if (len != nread)
    {
      DBG(1, "scl_upload: requested %lu bytes, got %lu\n",
	  (unsigned long) len, (unsigned long) nread);
      return SANE_STATUS_IO_ERROR;
    }
  return SANE_STATUS_GOOD;
}

SANE_Status
sanei_hp_scl_calibrate(HpScsi scsi)
{
  RETURN_IF_FAIL( hp_scsi_scl(scsi, SCL_CALIBRATE, 0) );
  return hp_scsi_flush(scsi);
}

SANE_Status
sanei_hp_scl_startScan(HpScsi scsi, HpScl scl)
{
  char *msg = "";

  if (scl == SCL_ADF_SCAN) msg = " (ADF)";
  else if (scl == SCL_XPA_SCAN) msg = " (XPA)";
  else scl = SCL_START_SCAN;

  DBG(1, "sanei_hp_scl_startScan: Start scan%s\n", msg);

  /* For active XPA we must not use XPA scan */
  if ((scl == SCL_XPA_SCAN) && sanei_hp_is_active_xpa (scsi))
  {
    DBG(3,"Map XPA scan to scan because of active XPA\n");
    scl = SCL_START_SCAN;
  }

  RETURN_IF_FAIL( hp_scsi_scl(scsi, scl, 0) );
  return hp_scsi_flush(scsi);
}

SANE_Status
sanei_hp_scl_reset(HpScsi scsi)
{
  RETURN_IF_FAIL( hp_scsi_write(scsi, "\033E", 2) );
  RETURN_IF_FAIL( hp_scsi_flush(scsi) );
  return sanei_hp_scl_errcheck(scsi);
}

SANE_Status
sanei_hp_scl_clearErrors(HpScsi scsi)
{
  RETURN_IF_FAIL( hp_scsi_flush(scsi) );
  RETURN_IF_FAIL( hp_scsi_write(scsi, "\033*oE", 4) );
  return hp_scsi_flush(scsi);
}

static const char *
hp_scl_strerror (int errnum)
{
  static const char * errlist[] = {
      "Command Format Error",
      "Unrecognized Command",
      "Parameter Error",
      "Illegal Window",
      "Scaling Error",
      "Dither ID Error",
      "Tone Map ID Error",
      "Lamp Error",
      "Matrix ID Error",
      "Cal Strip Param Error",
      "Gross Calibration Error"
  };

  if (errnum >= 0 && errnum < (int)(sizeof(errlist)/sizeof(errlist[0])))
      return errlist[errnum];
  else
      switch(errnum) {
      case 1024: return "ADF Paper Jam";
      case 1025: return "Home Position Missing";
      case 1026: return "Paper Not Loaded";
      default: return "??Unkown Error??";
      }
}

/* Check for SCL errors */
SANE_Status
sanei_hp_scl_errcheck (HpScsi scsi)
{
  int		errnum;
  int		nerrors;
  SANE_Status	status;

  status = sanei_hp_scl_inquire(scsi, SCL_CURRENT_ERROR_STACK, &nerrors,0,0);
  if (!FAILED(status) && nerrors)
      status = sanei_hp_scl_inquire(scsi, SCL_OLDEST_ERROR, &errnum,0,0);
  if (FAILED(status))
    {
      DBG(1, "scl_errcheck: Can't read SCL error stack: %s\n",
	  sane_strstatus(status));
      return SANE_STATUS_IO_ERROR;
    }

  if (nerrors)
    {
      DBG(1, "Scanner issued SCL error: (%d) %s\n",
	  errnum, hp_scl_strerror(errnum));

      sanei_hp_scl_clearErrors (scsi);
      return SANE_STATUS_IO_ERROR;
    }

  return SANE_STATUS_GOOD;
}
