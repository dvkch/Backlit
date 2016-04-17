/* sane - Scanner Access Now Easy.
   Copyright (C) 2001-2012 Stéphane Voltz <stef.dev@free.fr>
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

   This file implements a SANE backend for Umax PP flatbed scanners.  */

#define DEBUG_DECLARE_ONLY
#undef BACKEND_NAME
#define BACKEND_NAME umax_pp

#include "../include/sane/config.h"
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include "../include/sane/sanei_debug.h"

#define __MAIN__


#include "umax_pp_mid.h"

/* this function locks the parallel port so that other devices */
/* won't interfere. Returns UMAX1220P_BUSY is port cannot be   */
/* lock or UMAX1220P_OK if it is locked                        */
static int locked = 0;
#ifdef HAVE_LINUX_PPDEV_H
static int exmode = IEEE1284_MODE_COMPAT;
static int exflags = 0;
#endif

static int
lock_parport (void)
{
#ifdef HAVE_LINUX_PPDEV_H
  int mode, fd;
#endif

  DBG_INIT ();
  DBG (3, "lock_parport\n");

#ifdef HAVE_LINUX_PPDEV_H
  fd = sanei_umax_pp_getparport ();
  if ((fd > 0) && (!locked))
    {
      if (ioctl (sanei_umax_pp_getparport (), PPCLAIM))
        {
          return UMAX1220P_BUSY;
        }
#ifdef PPGETMODE
      if (ioctl (fd, PPGETMODE, &exmode))
        exmode = IEEE1284_MODE_COMPAT;
      if (ioctl (fd, PPGETFLAGS, &exflags))
        exflags = 0;
#endif
      mode = IEEE1284_MODE_EPP;
      ioctl (fd, PPNEGOT, &mode);
      ioctl (fd, PPSETMODE, &mode);
      locked = 1;
    }
#else
  locked = 1;
#endif
  return UMAX1220P_OK;
}


/* this function release parport */
static int
unlock_parport (void)
{
#ifdef HAVE_LINUX_PPDEV_H
  int fd, mode;

  fd = sanei_umax_pp_getparport ();
  if ((fd > 0) && (locked))
    {
      mode = IEEE1284_MODE_COMPAT;
      ioctl (fd, PPNEGOT, &mode);
      ioctl (fd, PPSETMODE, &exmode);
#ifdef PPSETFLAGS
      ioctl (fd, PPSETFLAGS, &exflags);
#endif
      ioctl (fd, PPRELEASE);
      locked = 1;
    }
#endif
  DBG (3, "unlock_parport\n");
  locked = 0;
  return UMAX1220P_OK;
}




/* 
 *
 *  This function recognize the scanner model by sending an image
 * filter command. 1220P will use it as is, but 2000P will return
 * it back modified.
 *
 */
int
sanei_umax_pp_model (int port, int *model)
{
  int recover = 0, rc;

  /* set up port */
  DBG (3, "sanei_umax_pp_model\n");
  sanei_umax_pp_setport (port);
  if (lock_parport () == UMAX1220P_BUSY)
    return UMAX1220P_BUSY;

  /* init transport layer */
  /* 0: failed
     1: success
     2: retry
     3: busy
   */
  do
    {
      rc = sanei_umax_pp_initTransport (recover);
    }
  while (rc == 2);

  if (rc == 3)
    {
      unlock_parport ();
      return UMAX1220P_BUSY;
    }
  if (rc != 1)
    {
      DBG (0, "sanei_umax_pp_initTransport() failed (%s:%d)\n", __FILE__,
           __LINE__);
      unlock_parport ();
      return UMAX1220P_TRANSPORT_FAILED;
    }

  /* check model only, and if only none given in conf file */
  if (!sanei_umax_pp_getastra ())
    {
      rc = sanei_umax_pp_checkModel ();
    }
  else
    {
      rc = sanei_umax_pp_getastra ();
    }
  sanei_umax_pp_endSession ();
  unlock_parport ();
  if (rc < 600)
    {
      DBG (0, "sanei_umax_pp_CheckModel() failed (%s:%d)\n", __FILE__,
           __LINE__);
      return UMAX1220P_PROBE_FAILED;
    }
  *model = rc;


  /* OK */
  return UMAX1220P_OK;
}

int
sanei_umax_pp_attach (int port, char *name)
{
  int recover = 0;

  /* set up port */
  if (name == NULL)
    {
      DBG (3, "sanei_umax_pp_attach(%d,NULL)\n", port);
    }
  else
    {
      DBG (3, "sanei_umax_pp_attach(%d,%s)\n", port, name);
    }

  sanei_umax_pp_setport (port);
  if (sanei_umax_pp_initPort (port, name) != 1)
    return UMAX1220P_PROBE_FAILED;

  /* init port locks the port, so we flag that */
  locked = 1;

  if (sanei_umax_pp_probeScanner (recover) != 1)
    {
      if (recover)
        {
          sanei_umax_pp_initTransport (recover);
          sanei_umax_pp_endSession ();
          if (sanei_umax_pp_probeScanner (recover) != 1)
            {
              DBG (0, "Recover failed ....\n");
              unlock_parport ();
              return UMAX1220P_PROBE_FAILED;
            }
        }
      else
        {
          unlock_parport ();
          return UMAX1220P_PROBE_FAILED;
        }
    }
  sanei_umax_pp_endSession ();
  unlock_parport ();


  /* OK */
  return UMAX1220P_OK;
}


int
sanei_umax_pp_open (int port, char *name)
{
  int rc;
  int recover = 0;

  /* set up port */
  DBG (3, "sanei_umax_pp_open\n");

  if (name == NULL)
    sanei_umax_pp_setport (port);

  if (lock_parport () == UMAX1220P_BUSY)
    return UMAX1220P_BUSY;

  /* init transport layer */
  /* 0: failed
     1: success
     2: retry
     3: scanner busy
   */
  do
    {
      rc = sanei_umax_pp_initTransport (recover);
    }
  while (rc == 2);

  if (rc == 3)
    {
      unlock_parport ();
      return UMAX1220P_BUSY;
    }

  if (rc != 1)
    {

      DBG (0, "sanei_umax_pp_initTransport() failed (%s:%d)\n", __FILE__,
           __LINE__);
      unlock_parport ();
      return UMAX1220P_TRANSPORT_FAILED;
    }
  /* init scanner */
  if (sanei_umax_pp_initScanner (recover) == 0)
    {
      DBG (0, "sanei_umax_pp_initScanner() failed (%s:%d)\n", __FILE__,
           __LINE__);
      sanei_umax_pp_endSession ();
      unlock_parport ();
      return UMAX1220P_SCANNER_FAILED;
    }

  /* OK */
  unlock_parport ();
  return UMAX1220P_OK;
}


int
sanei_umax_pp_cancel (void)
{
  DBG (3, "sanei_umax_pp_cancel\n");
  if (lock_parport () == UMAX1220P_BUSY)
    return UMAX1220P_BUSY;

  /* maybe EPAT reset here if exists */
  sanei_umax_pp_cmdSync (0xC2);
  sanei_umax_pp_cmdSync (0x00);
  sanei_umax_pp_cmdSync (0x00);
  if (sanei_umax_pp_park () == 0)
    {
      DBG (0, "sanei_umax_pp_park failed !!! (%s:%d)\n", __FILE__, __LINE__);
      unlock_parport ();
      return UMAX1220P_PARK_FAILED;
    }
  /* endSession() cancels any pending command  */
  /* such as parking ...., so we only return   */
  unlock_parport ();
  return UMAX1220P_OK;
}



int
sanei_umax_pp_start (int x, int y, int width, int height, int dpi, int color,
                     int autoset,
                     int gain, int offset, int *rbpp, int *rtw,
                     int *rth)
{
  int col = BW_MODE;

  DBG (3, "sanei_umax_pp_start\n");
  if (lock_parport () == UMAX1220P_BUSY)
    return UMAX1220P_BUSY;
  /* end session isn't done by cancel any more */
  sanei_umax_pp_endSession ();

  if (autoset)
    sanei_umax_pp_setauto (1);
  else
    sanei_umax_pp_setauto (0);

  switch (color)
    {
    case 0:
      col = BW2_MODE;
      break;
    case 1:
      col = BW_MODE;
      break;
    case 2:
      col = RGB_MODE;
      break;
    }

  if (sanei_umax_pp_startScan
      (x + sanei_umax_pp_getLeft (), y, width, height, dpi, col, gain,
       offset, rbpp, rtw, rth) != 1)
    {
      sanei_umax_pp_endSession ();
      unlock_parport ();
      return UMAX1220P_START_FAILED;
    }
  unlock_parport ();
  return UMAX1220P_OK;
}

int
sanei_umax_pp_read (long len, int window, int dpi, int last,
                    unsigned char *buffer)
{
  int read = 0;
  int bytes;

  DBG (3, "sanei_umax_pp_read\n");
  if (lock_parport () == UMAX1220P_BUSY)
    return UMAX1220P_BUSY;

  /* since 610P may override len and last to meet its */
  /* hardware requirements, we have to loop until we  */
  /* have all the data                                */
  while (read < len)
    {
      bytes =
        sanei_umax_pp_readBlock (len - read, window, dpi, last,
                                 buffer + read);
      if (bytes == 0)
        {
          sanei_umax_pp_endSession ();
          return UMAX1220P_READ_FAILED;
        }
      read += bytes;
    }
  unlock_parport ();
  return UMAX1220P_OK;
}



int
sanei_umax_pp_lamp (int on)
{
  /* init transport layer */
  DBG (3, "sanei_umax_pp_lamp\n");

  /* no lamp support for 610P ... */
  if (sanei_umax_pp_getastra () < 1210)
    return UMAX1220P_OK;

  if (lock_parport () == UMAX1220P_BUSY)
    return UMAX1220P_BUSY;

  if (sanei_umax_pp_setLamp (on) == 0)
    {
      DBG (0, "Setting lamp state failed!\n");
    }

  unlock_parport ();
  return UMAX1220P_OK;
}




int
sanei_umax_pp_status (void)
{
  int status;

  DBG (3, "sanei_umax_pp_status\n");
  if (lock_parport () == UMAX1220P_BUSY)
    return UMAX1220P_BUSY;
  /* check if head is at home */
  sanei_umax_pp_cmdSync (0x40);
  status = sanei_umax_pp_scannerStatus ();
  unlock_parport ();
  DBG (8, "sanei_umax_pp_status=0x%02X\n", status);
  if (((status & ASIC_BIT) != 0x00)||((status & MOTOR_BIT) == 0x00))
    return UMAX1220P_BUSY;

  return UMAX1220P_OK;
}

int
sanei_umax_pp_close ()
{
#ifdef HAVE_LINUX_PPDEV_H
  int fd;
#endif

  DBG (3, "sanei_umax_pp_close\n");

  lock_parport ();
  sanei_umax_pp_endSession ();
  unlock_parport ();

#ifdef HAVE_LINUX_PPDEV_H
  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {
      close (fd);
      sanei_umax_pp_setparport (0);
    }
#endif
  return UMAX1220P_OK;
}
