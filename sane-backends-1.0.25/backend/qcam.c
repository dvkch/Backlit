/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 David Mosberger-Tang
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

   This file implements a SANE backend for the Connectix QuickCam.  At
   present, only the color camera is supported though the driver
   should be able to easily accommodate black and white cameras.

   Portions of this code are derived from Scott Laird's qcam driver.
   It's copyright notice is reproduced here:

   Copyright (C) 1996 by Scott Laird

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without
   restriction, including without limitation the rights to use, copy,
   modify, merge, publish, distribute, sublicense, and/or sell copies
   of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT.  IN NO EVENT SHALL SCOTT LAIRD BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
   CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
   WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */

#ifdef _AIX
# include "lalloca.h"		/* MUST come first for AIX! */
#endif

#include "../include/sane/config.h"
#include "lalloca.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"


#define BACKEND_NAME qcam
#include "../include/sane/sanei_backend.h"

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

#include "../include/sane/sanei_config.h"
#define QCAM_CONFIG_FILE "qcam.conf"

#include "qcam.h"

/* status bits */
#define NeedRamTable		(1 << 1)
#define BlackBalanceInProgress	(1 << 6)
#define CameraNotReady		(1 << 7)

/* lpdata bits: */
#define Cmd0_7		0xff
#define CamRdy2		(   1 << 0)	/* byte mode */
#define Data0_6		(0x7f << 1)	/* byte mode */

/* lpstatus bits: */
#define CamRdy1		(   1 << 3)	/* nibble mode */
#define Nibble0_3	(0x0f << 4)	/* nibble mode */
#define Data7_11	(0x1f << 3)	/* byte mode */

/* lpcontrol bits: */
#define Strobe		(   1 << 0)	/* unused */
#define Autofeed	(   1 << 1)
#define Reset_N		(   1 << 2)
#define PCAck		(   1 << 3)
#define BiDir		(   1 << 5)

static int num_devices;
static QC_Device *first_dev;
static QC_Scanner *first_handle;

static const SANE_String_Const resolution_list[] = {
  "Low",			/* million-mode */
  "High",			/* billion-mode */
  0
};

static const SANE_Int mono_depth_list[] = {
  2,				/* # of elements */
  4, 6
};

static const SANE_Int color_depth_list[] = {
  /*2 */ 1,
  /* # of elements */
  /*16, */ 24
    /* "thousand" mode not implemented yet */
};

static const SANE_Int xfer_scale_list[] = {
  3,				/* # of elements */
  1, 2, 4
};

static const SANE_Range u8_range = {
  /* min, max, quantization */
  0, 255, 0
};

static const SANE_Range brightness_range = {
  /* min, max, quantization */
  0, 254, 0			/* 255 is bulb mode! */
};

static const SANE_Range x_range[] = {
  /* min, max, quantization */
  {0, 338, 2},			/* million mode */
  {0, 676, 4},			/* billion mode */
};

static const SANE_Range odd_x_range[] = {
  /* min, max, quantization */
  {1, 339, 2},			/* million mode */
  {3, 683, 4},			/* billion mode */
};

static const SANE_Range y_range[] = {
  /* min, max, quantization */
  {0, 249, 1},			/* million mode */
  {0, 498, 2},			/* billion mode */
};

static const SANE_Range odd_y_range[] = {
  /* min, max, quantization */
  {0, 249, 1},			/* million mode */
  {1, 499, 2},			/* billion mode */
};

static const SANE_Range bw_x_range = { 0, 334, 2 };
static const SANE_Range odd_bw_x_range = { 1, 335, 2 };
static const SANE_Range bw_y_range = { 0, 241, 1 };
static const SANE_Range odd_bw_y_range = { 1, 242, 1 };

#if defined(HAVE_SYS_IO_H) || defined(HAVE_ASM_IO_H) || defined (HAVE_SYS_HW_H)

#ifdef HAVE_SYS_IO_H
# include <sys/io.h>		/* GNU libc based OS */
#elif HAVE_ASM_IO_H
# include <asm/io.h>		/* older Linux */
#elif HAVE_SYS_HW_H
# include <sys/hw.h>		/* OS/2 */
#endif

#endif /* <sys/io.h> || <asm/io.h> || <sys/hw.h> */

#define read_lpdata(d)		inb ((d)->port)
#define read_lpstatus(d)	inb ((d)->port + 1)
#define read_lpcontrol(d)	inb ((d)->port + 2)
#define write_lpdata(d,v)	outb ((v), (d)->port)
#define write_lpcontrol(d,v)	outb ((v), (d)->port + 2)


static SANE_Status
enable_ports (QC_Device * q)
{
  /* better safe than sorry */
  if (q->port < 0x278 || q->port > 0x3bc)
    return SANE_STATUS_INVAL;

  if (ioperm (q->port, 3, 1) < 0)
    return SANE_STATUS_INVAL;

  return SANE_STATUS_GOOD;
}

static SANE_Status
disable_ports (QC_Device * q)
{
  if (ioperm (q->port, 3, 0) < 0)
    return SANE_STATUS_INVAL;

  return SANE_STATUS_GOOD;
}

/* We need a short delay loop -- somthing well under a millisecond.
   Unfortunately, adding 2 usleep(1)'s to qc_command slowed it down by
   a factor of over 1000 over the same loop with 2 usleep(0)'s, and
   that's too slow -- qc_start was taking over a second to run.  This
   seems to help, but if anyone has a good speed-independent pause
   routine, please tell me. -- Scott

   If you're worried about hogging the CPU: don't worry, the qcam
   interface leaves you no choice, so this doesn't make the situation
   any worse... */

static int
qc_wait (QC_Device * q)
{
  return read_lpstatus (q);
}


/* This function uses POSIX fcntl-style locking on a file created in
   the /tmp directory.  Because it uses the Unix record locking
   facility, locks are relinquished automatically on process
   termination, so "dead locks" are not a problem.  (FYI, the lock
   file will remain after process termination, but this is actually
   desired so that the next process need not re-creat(2)e it... just
   lock it.)  The wait argument indicates whether or not this funciton
   should "block" waiting for the previous lock to be relinquished.
   This is ideal so that multiple processes (eg. qcam) taking
   "snapshots" can peacefully coexist.

   -- Dave Plonka (plonka@carroll1.cc.edu) */
static SANE_Status
qc_lock_wait (QC_Device * q, int wait)
{
#ifdef F_SETLK

#ifndef HAVE_STRUCT_FLOCK
  struct flock
  {
    off_t l_start;
    off_t l_len;
    pid_t l_pid;
    short l_type;
    short l_whence;
  };
#endif /* !HAVE_STRUCT_FLOCK */
  struct flock sfl;
#endif

  DBG (3, "qc_lock_wait: acquiring lock for 0x%x\n", q->port);

#ifdef F_SETLK
  memset (&sfl, 0, sizeof (sfl));
#endif

  if (q->lock_fd < 0)
    {
      char lockfile[128];

      sprintf (lockfile, "/tmp/LOCK.qcam.0x%x", q->port);
      q->lock_fd = open (lockfile, O_WRONLY | O_CREAT | O_EXCL, 0666);
      if (q->lock_fd < 0)
	{
	  DBG (1, "qc_lock_wait: failed to open %s (%s)\n",
	       lockfile, strerror (errno));
	  return SANE_STATUS_INVAL;
	}

    }

#ifdef F_SETLK
  sfl.l_type = F_WRLCK;
  if (fcntl (q->lock_fd, wait ? F_SETLKW : F_SETLK, &sfl) != 0)
    {
      DBG (1, "qc_lock_wait: failed to acquire lock (%s)\n",
	   strerror (errno));
      return SANE_STATUS_INVAL;
    }
#endif

  DBG (3, "qc_lock_wait: got lock for 0x%x\n", q->port);
  return SANE_STATUS_GOOD;
}

static SANE_Status
qc_unlock (QC_Device * q)
{
  SANE_Status status;
  char lockfile[128];
#ifdef F_SETLK
#ifndef HAVE_STRUCT_FLOCK
  struct flock
  {
    off_t l_start;
    off_t l_len;
    pid_t l_pid;
    short l_type;
    short l_whence;
  };
#endif /* !HAVE_STRUCT_FLOCK */
  struct flock sfl;
#endif

  DBG (3, "qc_unlock: releasing lock for 0x%x\n", q->port);

#ifdef F_SETLK
  memset (&sfl, 0, sizeof (sfl));
#endif
  if (q->lock_fd < 0)
    {
      DBG (3, "qc_unlock; port was not locked\n");
      return SANE_STATUS_INVAL;
    }
  /* clear the exclusive lock */

#ifdef F_SETLK
  sfl.l_type = F_UNLCK;

  if (fcntl (q->lock_fd, F_SETLK, &sfl) != 0)
    {
      DBG (3, "qc_unlock: failed to release lock (%s)\n", strerror (errno));
      return SANE_STATUS_INVAL;
    }
#endif
  sprintf (lockfile, "/tmp/LOCK.qcam.0x%x", q->port);
  DBG (1, "qc_unlock: /tmp/LOCK.qcam.0x%x\n", q->port);
  unlink (lockfile);
  close (q->lock_fd);
  q->lock_fd = -1;
  DBG (1, "qc_unlock: exit\n");
  status = SANE_STATUS_GOOD;
  return status;
}

static SANE_Status
qc_lock (QC_Device * q)
{
  return qc_lock_wait (q, 1);
}

/* Busy-waits for a handshake signal from the QuickCam.  Almost all
   communication with the camera requires handshaking.  This is why
   qcam is a CPU hog.  */
static int
qc_waithand (QC_Device * q, int val)
{
  int status;

  while (((status = read_lpstatus (q)) & CamRdy1) != val);
  return status;
}

/* This is used when the qcam is in bidirectional ("byte") mode, and
   the handshaking signal is CamRdy2 (bit 0 of data reg) instead of
   CamRdy1 (bit 3 of status register).  It also returns the last value
   read, since this data is useful.  */
static unsigned int
qc_waithand2 (QC_Device * q, int val)
{
  unsigned int status;

  do
    {
      status = read_lpdata (q);
    }
  while ((status & CamRdy2) != (unsigned int) val);
  return status;
}

static unsigned int
qc_send (QC_Device * q, unsigned int byte)
{
  unsigned int echo;
  int n1, n2;

  write_lpdata (q, byte);
  qc_wait (q);
  write_lpcontrol (q, Autofeed | Reset_N);
  qc_wait (q);

  n1 = qc_waithand (q, CamRdy1);

  write_lpcontrol (q, Autofeed | Reset_N | PCAck);
  qc_wait (q);
  n2 = qc_waithand (q, 0);

  echo = (n1 & 0xf0) | ((n2 & 0xf0) >> 4);
#ifndef NDEBUG
  if (echo != byte)
    {
      DBG (1, "qc_send: sent 0x%02x, camera echoed 0x%02x\n", byte, echo);
      n2 = read_lpstatus (q);
      echo = (n1 & 0xf0) | ((n2 & 0xf0) >> 4);
      if (echo != byte)
	DBG (1, "qc_send: (re-read does not help)\n");
      else
	DBG (1, "qc_send: (fixed on re-read)\n");
    }
#endif
  return echo;
}

static int
qc_readparam (QC_Device * q)
{
  int n1, n2;
  int cmd;

  write_lpcontrol (q, Autofeed | Reset_N);	/* clear PCAck */
  n1 = qc_waithand (q, CamRdy1);

  write_lpcontrol (q, Autofeed | Reset_N | PCAck);	/* set PCAck */
  n2 = qc_waithand (q, 0);

  cmd = (n1 & 0xf0) | ((n2 & 0xf0) >> 4);
  return cmd;
}

static unsigned int
qc_getstatus (QC_Device * q)
{
  unsigned int status;

  qc_send (q, QC_SEND_STATUS);
  status = qc_readparam (q);
  DBG (3, "qc_getstatus: status=0x%02x\n", status);
  return status;
}

static void
qc_setscanmode (QC_Scanner * s, u_int * modep)
{
  QC_Device *q = s->hw;
  u_int mode = 0;

  if (q->version != QC_COLOR)
    {
      switch (s->val[OPT_XFER_SCALE].w)
	{
	case 1:
	  mode = 0;
	  break;
	case 2:
	  mode = 4;
	  break;
	case 4:
	  mode = 8;
	  break;
	}
      switch (s->val[OPT_DEPTH].w)
	{
	case 4:
	  break;
	case 6:
	  mode += 2;
	  break;
	}
    }
  else
    {
      switch (s->val[OPT_XFER_SCALE].w)
	{
	case 1:
	  mode = 0;
	  break;
	case 2:
	  mode = 2;
	  break;
	case 4:
	  mode = 4;
	  break;
	}
      if (s->resolution == QC_RES_LOW)
	mode |= 0x18;		/* millions mode */
      else
	mode |= 0x10;		/* billions mode */
    }
  if (s->val[OPT_TEST].w)
    mode |= 0x40;		/* test mode */

  if (q->port_mode == QC_BIDIR)
    mode |= 1;

  DBG (2, "scanmode (before increment): 0x%x\n", mode);

  if (q->version == QC_COLOR)
    ++mode;

  *modep = mode;
}

/* Read data bytes from the camera.  The number of bytes read is
   returned as the function result.  Depending on the mode, it may be
   either 1, 3 or 6.  On failure, 0 is returned.  If buffer is 0, the
   internal state-machine is reset.  */
static size_t
qc_readbytes (QC_Scanner * s, unsigned char buffer[])
{
  QC_Device *q = s->hw;
  unsigned int hi, lo;
  unsigned int hi2, lo2;
  size_t bytes = 0;

  if (!buffer)
    {
      s->readbytes_state = 0;
      return 0;
    }

  switch (q->port_mode)
    {
    case QC_BIDIR:
      /* bi-directional port */

      /* read off 24 bits: */
      write_lpcontrol (q, Autofeed | Reset_N | BiDir);
      lo = qc_waithand2 (q, 1) >> 1;
      hi = (read_lpstatus (q) >> 3) & 0x1f;
      write_lpcontrol (q, Autofeed | Reset_N | PCAck | BiDir);
      lo2 = qc_waithand2 (q, 0) >> 1;
      hi2 = (read_lpstatus (q) >> 3) & 0x1f;
      if (q->version == QC_COLOR)
	{
	  /* is Nibble3 inverted for color quickcams only? */
	  hi ^= 0x10;
	  hi2 ^= 0x10;
	}
      switch (s->val[OPT_DEPTH].w)
	{
	case 4:
	  buffer[0] = lo & 0xf;
	  buffer[1] = ((lo & 0x70) >> 4) | ((hi & 1) << 3);
	  buffer[2] = (hi & 0x1e) >> 1;
	  buffer[3] = lo2 & 0xf;
	  buffer[4] = ((lo2 & 0x70) >> 4) | ((hi2 & 1) << 3);
	  buffer[5] = (hi2 & 0x1e) >> 1;
	  bytes = 6;
	  break;

	case 6:
	  buffer[0] = lo & 0x3f;
	  buffer[1] = ((lo & 0x40) >> 6) | (hi << 1);
	  buffer[2] = lo2 & 0x3f;
	  buffer[3] = ((lo2 & 0x40) >> 6) | (hi2 << 1);
	  bytes = 4;
	  break;

	case 24:
	  buffer[0] = lo | ((hi & 0x1) << 7);
	  buffer[1] = ((hi2 & 0x1e) >> 1) | ((hi & 0x1e) << 3);
	  buffer[2] = lo2 | ((hi2 & 0x1) << 7);
	  bytes = 3;
	  break;
	}
      break;

    case QC_UNIDIR:		/* Unidirectional Port */
      write_lpcontrol (q, Autofeed | Reset_N);
      lo = (qc_waithand (q, CamRdy1) & 0xf0) >> 4;
      write_lpcontrol (q, Autofeed | Reset_N | PCAck);
      hi = (qc_waithand (q, 0) & 0xf0) >> 4;

      if (q->version == QC_COLOR)
	{
	  /* invert Nibble3 */
	  hi ^= 8;
	  lo ^= 8;
	}

      switch (s->val[OPT_DEPTH].w)
	{
	case 4:
	  buffer[0] = lo;
	  buffer[1] = hi;
	  bytes = 2;
	  break;

	case 6:
	  switch (s->readbytes_state)
	    {
	    case 0:
	      buffer[0] = (lo << 2) | ((hi & 0xc) >> 2);
	      s->saved_bits = (hi & 3) << 4;
	      s->readbytes_state = 1;
	      bytes = 1;
	      break;

	    case 1:
	      buffer[0] = lo | s->saved_bits;
	      s->saved_bits = hi << 2;
	      s->readbytes_state = 2;
	      bytes = 1;
	      break;

	    case 2:
	      buffer[0] = ((lo & 0xc) >> 2) | s->saved_bits;
	      buffer[1] = ((lo & 3) << 4) | hi;
	      s->readbytes_state = 0;
	      bytes = 2;
	      break;

	    default:
	      DBG (1, "qc_readbytes: bad unidir 6-bit stat %d\n",
		   s->readbytes_state);
	      break;
	    }
	  break;

	case 24:
	  buffer[0] = (lo << 4) | hi;
	  bytes = 1;
	  break;

	default:
	  DBG (1, "qc_readbytes: bad unidir bit depth %d\n",
	       s->val[OPT_DEPTH].w);
	  break;
	}
      break;

    default:
      DBG (1, "qc_readbytes: bad port_mode %d\n", q->port_mode);
      break;
    }
  return bytes;
}

static void
qc_reset (QC_Device * q)
{
  write_lpcontrol (q, Strobe | Autofeed | Reset_N | PCAck);
  qc_wait (q);
  write_lpcontrol (q, Strobe | Autofeed | PCAck);
  qc_wait (q);
  write_lpcontrol (q, Strobe | Autofeed | Reset_N | PCAck);
}

/* This function is executed as a child process.  The reason this is
   executed as a subprocess is because the qcam interface directly reads
   off of a I/O port (rather than a filedescriptor).  Thus, to have
   something to select() on, we transfer the data through a pipe.

   WARNING: Since this is executed as a subprocess, it's NOT possible
   to update any of the variables in the main process (in particular
   the scanner state cannot be updated).  */

static jmp_buf env;

static void
sighandler (int signal)
{
  DBG (3, "sighandler: got signal %d\n", signal);
  longjmp (env, 1);
}

/* Original despeckling code by Patrick Reynolds <patrickr@virginia.edu> */

static void
despeckle (int width, int height, SANE_Byte * in, SANE_Byte * out)
{
  long x, i;
  /* The light-check threshold.  Higher numbers remove more lights but
     blur the image more.  30 is good for indoor lighting.  */
# define NO_LIGHTS 30

  /* macros to make the code a little more readable, p=previous, n=next */
# define R	in[i*3]
# define G	in[i*3+1]
# define B	in[i*3+2]
# define pR	in[i*3-3]
# define pG	in[i*3-2]
# define pB	in[i*3-1]
# define nR	in[i*3+3]
# define nG	in[i*3+4]
# define nB	in[i*3+5]

  DBG (1, "despeckle: width=%d, height=%d\n", width, height);

  for (x = i = 0; i < width * height; ++i)
    {
      if (x == 0 || x == width - 1)
	memcpy (&out[i * 3], &in[i * 3], 3);
      else
	{
	  if (R - (G + B) / 2 >
	      NO_LIGHTS + ((pR - (pG + pB) / 2) + (nR - (nG + nB) / 2)))
	    out[i * 3] = (pR + nR) / 2;
	  else
	    out[i * 3] = R;

	  if (G - (R + B) / 2 >
	      NO_LIGHTS + ((pG - (pR + pB) / 2) + (nG - (nR + nB) / 2)))
	    out[i * 3 + 1] = (pG + nG) / 2;
	  else
	    out[i * 3 + 1] = G;

	  if (B - (G + R) / 2 >
	      NO_LIGHTS + ((pB - (pG + pR) / 2) + (nB - (nG + nR) / 2)))
	    out[i * 3 + 2] = (pB + nB) / 2;
	  else
	    out[i * 3 + 2] = B;
	}
      if (++x >= width)
	x = 0;
    }
# undef R
# undef G
# undef B
# undef pR
# undef pG
# undef pB
# undef nR
# undef nG
# undef nB
}

static void
despeckle32 (int width, int height, SANE_Byte * in, SANE_Byte * out)
{
  long x, i;
  /* macros to make the code a little more readable, p=previous, n=next */
# define B	in[i*4]
# define Ga	in[i*4 + 1]
# define Gb	in[i*4 + 1]	/* ignore Gb and use Ga instead---Gb is weird */
# define R	in[i*4 + 3]
# define pB	in[i*4 - 4]
# define pGa	in[i*4 - 3]
# define pGb	in[i*4 - 1]	/* ignore Gb and use Ga instead---Gb is weird */
# define pR	in[i*4 - 1]
# define nB	in[i*4 + 4]
# define nGa	in[i*4 + 5]
# define nGb	in[i*4 + 5]	/* ignore Gb and use Ga instead---Gb is weird */
# define nR	in[i*4 + 7]

  DBG (1, "despeckle32: width=%d, height=%d\n", width, height);

  for (x = i = 0; i < width * height; ++i)
    {
      if (x == 0 || x == width - 1)
	memcpy (&out[i * 4], &in[i * 4], 4);
      else
	{
	  if (x >= width - 2)
	    /* the last red pixel seems to be black at all times, use
	       R instead: */
	    nR = R;

	  if (R - ((Ga + Gb) / 2 + B) / 2 >
	      NO_LIGHTS + ((pR - ((pGa + pGb) / 2 + pB) / 2) +
			   (nR - ((nGa + nGb) / 2 + nB) / 2)))
	    out[i * 4 + 3] = (pR + nR) / 2;
	  else
	    out[i * 4 + 3] = R;

	  if (Ga - (R + B) / 2 > NO_LIGHTS + ((pGa - (pR + pB) / 2) +
					      (nGa - (nR + nB) / 2)))
	    out[i * 4 + 1] = (pGa + nGa) / 2;
	  else
	    out[i * 4 + 1] = Ga;

	  if (Gb - (R + B) / 2 > NO_LIGHTS + ((pGb - (pR + pB) / 2) +
					      (nGb - (nR + nB) / 2)))
	    out[i * 4 + 2] = (pGb + nGb) / 2;
	  else
	    out[i * 4 + 2] = Gb;

	  if (B - ((Ga + Gb) / 2 + R) / 2 >
	      NO_LIGHTS + ((pB - ((pGa + pGb) / 2 + pR) / 2) +
			   (nB - ((nGa + nGb) / 2 + nR) / 2)))
	    out[i * 4 + 0] = (pB + nB) / 2;
	  else
	    out[i * 4 + 0] = B;
	}
      if (++x >= width)
	x = 0;
    }
# undef R
# undef Ga
# undef Gb
# undef B
# undef pR
# undef pGa
# undef pGb
# undef pB
# undef nR
# undef nGa
# undef nGb
# undef nB
}

static int
reader_process (QC_Scanner * s, int in_fd, int out_fd)
{
  static SANE_Byte *buffer = 0, *extra = 0;
  static size_t buffer_size = 0;
  size_t count, len, num_bytes;
  QC_Device *q = s->hw;
  QC_Scan_Request req;
  int width, height;
  SANE_Byte *src;
  FILE *ofp;

  DBG (5, "reader_process: enter\n");

  enable_ports (q);

  ofp = fdopen (out_fd, "w");
  if (!ofp)
    return 1;

  while (1)
    {
      if (setjmp (env))
	{
	  char ch;

	  /* acknowledge the signal: */
	  DBG (1, "reader_process: sending signal ACK\n");
	  fwrite (&ch, 1, 1, ofp);
	  fflush (ofp);		/* force everything out the pipe */
	  continue;
	}
      signal (SIGINT, sighandler);

      /* the main process gets us started by writing a size_t giving
         the number of bytes we should expect: */
      if (read (in_fd, &req, sizeof (req)) != sizeof (req))
	{
	  perror ("read");
	  return 1;
	}
      num_bytes = req.num_bytes;

      DBG (3, "reader_process: got request for %lu bytes\n",
	   (u_long) num_bytes);

      /* Don't do this in sane_start() since there may be a long
         timespan between it and the first sane_read(), which would
         result in poor images.  */
      qc_send (q, QC_SEND_VIDEO_FRAME);
      qc_send (q, req.mode);

      if (req.despeckle
	  && (!extra || buffer_size < num_bytes
	      || buffer_size >= 2 * num_bytes))
	{
	  if (extra)
	    extra = realloc (extra, num_bytes);
	  else
	    extra = malloc (num_bytes);
	  if (!extra)
	    {
	      DBG (1, "reader_process: malloc(%ld) failed\n",
		   (long) num_bytes);
	      exit (1);
	    }
	}

      if (buffer_size < num_bytes || buffer_size >= 2 * num_bytes)
	{
	  if (buffer)
	    buffer = realloc (buffer, num_bytes);
	  else
	    buffer = malloc (num_bytes);
	  if (!buffer)
	    {
	      DBG (1, "reader_process: malloc(%ld) failed\n",
		   (long) num_bytes);
	      exit (1);
	    }
	  buffer_size = num_bytes;
	}

      if (q->port_mode == QC_BIDIR)
	{
	  /* turn port into input port */
	  write_lpcontrol (q, Autofeed | Reset_N | PCAck | BiDir);
	  usleep (3);
	  write_lpcontrol (q, Autofeed | Reset_N | BiDir);
	  qc_waithand (q, CamRdy1);
	  write_lpcontrol (q, Autofeed | Reset_N | PCAck | BiDir);
	  qc_waithand (q, 0);
	}

      if (q->version == QC_COLOR)
	for (len = 0; len < num_bytes; len += count)
	  count = qc_readbytes (s, buffer + len);
      else
	{
	  /* strange -- should be 15:63 below, but 4bpp is odd */
	  int shift, invert;
	  unsigned int i;
	  u_char val;

	  switch (s->val[OPT_DEPTH].w)
	    {
	    case 4:
	      invert = 16;
	      shift = 4;
	      break;

	    case 6:
	      invert = 63;
	      shift = 2;
	      break;

	    default:
	      DBG (1, "reader_process: unexpected depth %d\n",
		   s->val[OPT_DEPTH].w);
	      return 1;
	    }

	  for (len = 0; len < num_bytes; len += count)
	    {
	      count = qc_readbytes (s, buffer + len);
	      for (i = 0; i < count; ++i)
		{
		  /* 4bpp is odd (again) -- inverter is 16, not 15,
		     but output must be 0-15 */
		  val = buffer[len + i];
		  if (val > 0 || invert != 16)
		    val = invert - val;
		  buffer[len + i] = (val << shift) | (val >> (8 - 2 * shift));
		}
	    }
	  qc_readbytes (s, 0);	/* reset state machine */
	}
      /* we're done reading this frame: */
      DBG (2, "reader_process: frame complete\n");

      if (q->port_mode == QC_BIDIR)
	{
	  /* return port to output mode */
	  write_lpcontrol (q, Autofeed);
	  usleep (3);
	  write_lpcontrol (q, Autofeed | Reset_N);
	  usleep (3);
	  write_lpcontrol (q, Autofeed | Reset_N | PCAck);
	}

      if (req.resolution == QC_RES_HIGH)
	{
	  SANE_Byte buf[6];
	  int x, y;

	  /* in billions mode, we need to oversample the data: */
	  src = buffer;
	  width = req.params.pixels_per_line;
	  height = req.params.lines;

	  if (req.despeckle)
	    {
	      despeckle32 (width / 2, req.params.lines / 2, buffer, extra);
	      src = extra;
	    }

	  assert (!(width & 1));	/* width must be even */

	  for (y = 0; y < height; ++y)
	    {
	      /* even line */

	      for (x = 0; x < width; x += 2)
		{
		  int red1, green1, blue1, green2, blue2;

		  blue1 = src[0];
		  green1 = src[1];
		  red1 = src[3];
		  if (x >= width - 2)
		    {
		      red1 = src[-1];	/* last red seems to be missing */
		      blue2 = blue1;
		      green2 = green1;
		    }
		  else
		    {
		      blue2 = src[4];
		      green2 = src[5];
		    }
		  src += 4;

		  buf[0] = red1;
		  buf[1] = green1;
		  buf[2] = blue1;
		  buf[3] = red1;
		  buf[4] = green2;
		  buf[5] = blue2;
		  if (fwrite (buf, 1, 6, ofp) != 6)
		    {
		      perror ("fwrite: short write");
		      return 1;
		    }
		}
	      if (++y >= height)
		break;

	      src -= 2 * width;	/* 4 bytes/pixel -> 2 pixels of 3 bytes each */

	      /* odd line */
	      for (x = 0; x < width; x += 2)
		{
		  int red1, green3, blue3, green4, blue4;
		  int yoff;

		  if (x >= width - 2)
		    red1 = src[-1];	/* last red seems to be missing */
		  else
		    red1 = src[3];
		  yoff = 2 * width;
		  if (y >= height - 1)
		    yoff = 0;
		  green3 = src[yoff + 1];
		  blue3 = src[yoff + 0];
		  if (x >= width - 2)
		    {
		      blue4 = blue3;
		      green4 = green3;
		    }
		  else
		    {
		      blue4 = src[yoff + 4];
		      green4 = src[yoff + 5];
		    }
		  src += 4;

		  buf[0] = red1;
		  buf[1] = green3;
		  buf[2] = blue3;
		  buf[3] = red1;
		  buf[4] = green4;
		  buf[5] = blue4;
		  if (fwrite (buf, 1, 6, ofp) != 6)
		    {
		      perror ("fwrite: short write");
		      return 1;
		    }
		}
	    }
	}
      else
	{
	  src = buffer;
	  if (req.despeckle)
	    {
	      despeckle (req.params.pixels_per_line, req.params.lines,
			 buffer, extra);
	      src = extra;
	    }

	  /* now write the whole thing to the main process: */
	  if (fwrite (src, 1, num_bytes, ofp) != num_bytes)
	    {
	      perror ("fwrite: short write");
	      return 1;
	    }
	}
      fflush (ofp);
    }
  assert (SANE_FALSE);		/* not reached */
  DBG (5, "reader_process: exit\n");
  return 1;
}

static SANE_Status
attach (const char *devname, QC_Device ** devp)
{
  int i, n1, n2, s1, s2, cmd, port, force_unidir;
  SANE_Status result, status;
  QC_Device *q;
  char *endp;

  DBG (3, "attach: enter\n");
  errno = 0;
  force_unidir = 0;
  if (devname[0] == 'u')
    {
      force_unidir = 1;
      ++devname;
    }
  port = strtol (devname, &endp, 0);
  if (endp == devname || errno == ERANGE)
    {
      DBG (1, "attach: invalid port address `%s'\n", devname);
      return SANE_STATUS_INVAL;
    }

  for (q = first_dev; q; q = q->next)
    if (port == q->port)
      {
	if (devp)
	  *devp = q;
	return SANE_STATUS_GOOD;
      }

  q = malloc (sizeof (*q));
  if (!q)
    return SANE_STATUS_NO_MEM;

  memset (q, 0, sizeof (*q));
  q->port = port;
  q->lock_fd = -1;

  result = enable_ports (q);
  if (result != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: cannot enable ports (%s)\n", strerror (errno));
      free (q);
      return SANE_STATUS_INVAL;
    }

  /* lock camera while we determine its version: */
  qc_lock (q);

  qc_reset (q);

  write_lpdata (q, QC_SEND_VERSION);
  qc_wait (q);
  write_lpcontrol (q, Autofeed | Reset_N);	/* make PCAck inactive */
  qc_wait (q);

  for (i = 0; (i < 1000) && !(s1 = (n1 = read_lpstatus (q)) & CamRdy1); i++);
  if (!s1)
    {
      DBG (2, "attach: failed to get CamRdy1 at port 0x%x\n", q->port);
      goto unlock_and_fail;
    }

  write_lpcontrol (q, Autofeed | Reset_N | PCAck);
  qc_wait (q);

  for (i = 0; (i < 1000) && (s2 = (n2 = read_lpstatus (q)) & CamRdy1); i++);
  if (s2)
    {
      DBG (2, "attach: CamRdy1 failed to clear at port 0x%x\n", q->port);
      goto unlock_and_fail;
    }

  cmd = (n1 & 0xf0) | ((n2 & 0xf0) >> 4);

  if (cmd != QC_SEND_VERSION)
    {
      DBG (2, "attach: got 0x%02x instead of 0x%02x\n", cmd, QC_SEND_VERSION);
      goto unlock_and_fail;
    }

  q->version = qc_readparam (q);
  DBG (1, "attach: found QuickCam version 0x%02x\n", q->version);

  q->port_mode = QC_UNIDIR;
  if (!force_unidir)
    {
      write_lpcontrol (q, BiDir);
      write_lpdata (q, 0x75);
      if (read_lpdata (q) != 0x75)
	q->port_mode = QC_BIDIR;
    }

  /* For some reason the color quickcam needs two set-black commands
     after a reset.  Thus, we now set the black-level to some
     reasonable value (0) so that the next set-black level command
     will really go through.  */
  if (q->version == QC_COLOR)
    {
      qc_send (q, QC_SET_BLACK);
      qc_send (q, 0);

      DBG (3, "attach: resetting black_level\n");

      /* wait for set black level command to finish: */
      while (qc_getstatus (q) & (CameraNotReady | BlackBalanceInProgress))
	usleep (10000);
    }

  status = qc_unlock (q);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: status qc_unlock NOK\n");
      /* status = SANE_STATUS_GOOD;  */
    }
  q->sane.name = strdup (devname);
  q->sane.vendor = "Connectix";
  q->sane.model =
    (q->version == QC_COLOR) ? "Color QuickCam" : "B&W QuickCam";
  q->sane.type = "video camera";

  ++num_devices;
  q->next = first_dev;
  first_dev = q;

  if (devp)
    *devp = q;
  DBG (3, "attach: exit status OK\n");
  status = SANE_STATUS_GOOD;
  return status;


unlock_and_fail:
  status = qc_unlock (q);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: unlock_and_fail status qc_unlock NOK\n");
    }
  free (q);
  DBG (3, "attach: exit status NOK\n");
  status = SANE_STATUS_INVAL;
  return status;
}

static SANE_Status
init_options (QC_Scanner * s)
{
  int i;

  DBG (3, "init_options: enter\n");
  
  memset (s->opt, 0, sizeof (s->opt));
  memset (s->val, 0, sizeof (s->val));

  for (i = 0; i < NUM_OPTIONS; ++i)
    {
      s->opt[i].size = sizeof (SANE_Word);
      s->opt[i].cap = (SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT);
    }

  s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  s->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

  /* "Mode" group: */

  s->opt[OPT_MODE_GROUP].title = "Scan Mode";
  s->opt[OPT_MODE_GROUP].desc = "";
  s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MODE_GROUP].cap = 0;
  s->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* resolution */
  s->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].type = SANE_TYPE_STRING;
  s->opt[OPT_RESOLUTION].size = 5;	/* sizeof("High") */
  s->opt[OPT_RESOLUTION].unit = SANE_UNIT_NONE;
  s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_RESOLUTION].constraint.string_list = resolution_list;
  s->val[OPT_RESOLUTION].s = strdup (resolution_list[QC_RES_LOW]);

  /* bit-depth */
  s->opt[OPT_DEPTH].name = SANE_NAME_BIT_DEPTH;
  s->opt[OPT_DEPTH].title = "Pixel depth";
  s->opt[OPT_DEPTH].desc = "Number of bits per pixel.";
  s->opt[OPT_DEPTH].type = SANE_TYPE_INT;
  s->opt[OPT_DEPTH].unit = SANE_UNIT_BIT;
  s->opt[OPT_DEPTH].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->opt[OPT_DEPTH].constraint.word_list = color_depth_list;
  s->val[OPT_DEPTH].w = color_depth_list[NELEMS (color_depth_list) - 1];

  /* test */
  s->opt[OPT_TEST].name = "test-image";
  s->opt[OPT_TEST].title = "Force test image";
  s->opt[OPT_TEST].desc =
    "Acquire a test-image instead of the image seen by the camera. "
    "The test image consists of red, green, and blue squares of "
    "32x32 pixels each.  Use this to find out whether the "
    "camera is connected properly.";
  s->opt[OPT_TEST].type = SANE_TYPE_BOOL;
  s->val[OPT_TEST].w = SANE_FALSE;

  /* "Geometry" group: */
  s->opt[OPT_GEOMETRY_GROUP].title = "Geometry";
  s->opt[OPT_GEOMETRY_GROUP].desc = "";
  s->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* top-left x */
  s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  s->opt[OPT_TL_X].type = SANE_TYPE_INT;
  s->opt[OPT_TL_X].unit = SANE_UNIT_PIXEL;
  s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_X].constraint.range = &x_range[QC_RES_LOW];
  s->val[OPT_TL_X].w = 10;

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_INT;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_PIXEL;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &y_range[QC_RES_LOW];
  s->val[OPT_TL_Y].w = 0;

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_INT;
  s->opt[OPT_BR_X].unit = SANE_UNIT_PIXEL;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &odd_x_range[QC_RES_LOW];
  s->val[OPT_BR_X].w = 339;

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_INT;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_PIXEL;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &odd_y_range[QC_RES_LOW];
  s->val[OPT_BR_Y].w = 245;

  /* xfer-scale */
  s->opt[OPT_XFER_SCALE].name = "transfer-scale";
  s->opt[OPT_XFER_SCALE].title = "Transfer scale";
  s->opt[OPT_XFER_SCALE].desc =
    "The transferscale determines how many of the acquired pixels actually "
    "get sent to the computer.  For example, a transfer scale of 2 would "
    "request that every other acquired pixel would be omitted.  That is, "
    "when scanning a 200 pixel wide and 100 pixel tall area, the resulting "
    "image would be only 100x50 pixels large.  Using a large transfer scale "
    "improves acquisition speed, but reduces resolution.";
  s->opt[OPT_XFER_SCALE].type = SANE_TYPE_INT;
  s->opt[OPT_XFER_SCALE].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->opt[OPT_XFER_SCALE].constraint.word_list = xfer_scale_list;
  s->val[OPT_XFER_SCALE].w = xfer_scale_list[1];

  /* despeckle */
  s->opt[OPT_DESPECKLE].name = "despeckle";
  s->opt[OPT_DESPECKLE].title = "Speckle filter";
  s->opt[OPT_DESPECKLE].desc = "Turning on this filter will remove the "
    "christmas lights that are typically present in dark images.";
  s->opt[OPT_DESPECKLE].type = SANE_TYPE_BOOL;
  s->opt[OPT_DESPECKLE].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_DESPECKLE].w = 0;


  /* "Enhancement" group: */

  s->opt[OPT_ENHANCEMENT_GROUP].title = "Enhancement";
  s->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  s->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ENHANCEMENT_GROUP].cap = 0;
  s->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* brightness */
  s->opt[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS
    "  In a conventional camera, this control corresponds to the "
    "exposure time.";
  s->opt[OPT_BRIGHTNESS].type = SANE_TYPE_INT;
  s->opt[OPT_BRIGHTNESS].cap |= SANE_CAP_AUTOMATIC;
  s->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BRIGHTNESS].constraint.range = &brightness_range;
  s->val[OPT_BRIGHTNESS].w = 135;

  /* contrast */
  s->opt[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  s->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  s->opt[OPT_CONTRAST].desc = SANE_DESC_CONTRAST;
  s->opt[OPT_CONTRAST].type = SANE_TYPE_INT;
  s->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CONTRAST].constraint.range = &u8_range;
  s->val[OPT_CONTRAST].w = 104;

  /* black-level */
  s->opt[OPT_BLACK_LEVEL].name = SANE_NAME_BLACK_LEVEL;
  s->opt[OPT_BLACK_LEVEL].title = SANE_TITLE_BLACK_LEVEL;
  s->opt[OPT_BLACK_LEVEL].desc = SANE_DESC_BLACK_LEVEL
    " This value should be selected so that black areas just start "
    "to look really black (not gray).";
  s->opt[OPT_BLACK_LEVEL].type = SANE_TYPE_INT;
  s->opt[OPT_BLACK_LEVEL].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BLACK_LEVEL].constraint.range = &u8_range;
  s->val[OPT_BLACK_LEVEL].w = 0;

  /* white-level */
  s->opt[OPT_WHITE_LEVEL].name = SANE_NAME_WHITE_LEVEL;
  s->opt[OPT_WHITE_LEVEL].title = SANE_TITLE_WHITE_LEVEL;
  s->opt[OPT_WHITE_LEVEL].desc = SANE_DESC_WHITE_LEVEL
    " This value should be selected so that white areas just start "
    "to look really white (not gray).";
  s->opt[OPT_WHITE_LEVEL].type = SANE_TYPE_INT;
  s->opt[OPT_WHITE_LEVEL].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_WHITE_LEVEL].constraint.range = &u8_range;
  s->val[OPT_WHITE_LEVEL].w = 150;

  /* hue */
  s->opt[OPT_HUE].name = SANE_NAME_HUE;
  s->opt[OPT_HUE].title = SANE_TITLE_HUE;
  s->opt[OPT_HUE].desc = SANE_DESC_HUE;
  s->opt[OPT_HUE].type = SANE_TYPE_INT;
  s->opt[OPT_HUE].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_HUE].constraint.range = &u8_range;
  s->val[OPT_HUE].w = 128;

  /* saturation */
  s->opt[OPT_SATURATION].name = SANE_NAME_SATURATION;
  s->opt[OPT_SATURATION].title = SANE_TITLE_SATURATION;
  s->opt[OPT_SATURATION].desc = SANE_DESC_SATURATION;
  s->opt[OPT_SATURATION].type = SANE_TYPE_INT;
  s->opt[OPT_SATURATION].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_SATURATION].constraint.range = &u8_range;
  s->val[OPT_SATURATION].w = 100;

  DBG (3, "init_options: exit\n");
  
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  char dev_name[PATH_MAX], *str;
  size_t len;
  FILE *fp;
  authorize = authorize;	/* silence compilation warnings */

  DBG_INIT ();

  DBG (1, "sane_init: enter\n");

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, 0);

  fp = sanei_config_open (QCAM_CONFIG_FILE);
  if (!fp)
    {
      DBG (1, "sane_init: file `%s' not accessible\n", QCAM_CONFIG_FILE);
      return SANE_STATUS_INVAL;
    }

  while (sanei_config_read (dev_name, sizeof (dev_name), fp))
    {
      if (dev_name[0] == '#')	/* ignore line comments */
	continue;
      len = strlen (dev_name);

      if (!len)
	continue;		/* ignore empty lines */

      for (str = dev_name; *str && !isspace (*str) && *str != '#'; ++str);
      *str = '\0';

      attach (dev_name, 0);
    }
  fclose (fp);

  DBG (1, "sane_init: exit\n");
  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  QC_Device *dev, *next;
  static const SANE_Device **devlist;
  
  DBG (5, "sane_exit: enter\n");
  
  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      free ((void *) dev->sane.name);
      disable_ports (dev);
      free (dev);
    }
  if (devlist) {
	  free (devlist);
          devlist = NULL;
  }		  
  DBG (5, "sane_exit: exit\n");
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  static const SANE_Device **devlist = 0;
  QC_Device *dev;
  int i;

  DBG (5, "sane_get_devices: enter\n");

  local_only = local_only;	/* silence compilation warnings */

  if (devlist)
    free (devlist);

  devlist = malloc ((num_devices + 1) * sizeof (devlist[0]));
  if (!devlist)
    return SANE_STATUS_NO_MEM;

  i = 0;
  for (dev = first_dev; i < num_devices; dev = dev->next)
    devlist[i++] = &dev->sane;
  devlist[i++] = 0;

  *device_list = devlist;
  
  DBG (5, "sane_get_devices: exit\n");
  
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  SANE_Status status;
  QC_Device *dev;
  QC_Scanner *s;

  DBG (5, "sane_open: enter: (devicename = %s)\n", devicename);
  if (devicename[0])
    {
      status = attach (devicename, &dev);
      if (status != SANE_STATUS_GOOD)
	return status;
    }
  else
    /* empty devicname -> use first device */
    dev = first_dev;

  if (!dev)
    return SANE_STATUS_INVAL;

  s = malloc (sizeof (*s));
  if (!s)
    return SANE_STATUS_NO_MEM;
  memset (s, 0, sizeof (*s));
  s->user_corner = 0;
  s->hw = dev;
  s->value_changed = ~0;	/* ensure all options get updated */
  s->reader_pid = -1;
  s->to_child = -1;
  s->from_child = -1;
  s->read_fd = -1;

  init_options (s);

  /* The contrast option seems to have an effect for b&w cameras only,
     so don't give the user the impression that this is a useful thing
     to set... */
  if (s->hw->version == QC_COLOR)
    s->opt[OPT_CONTRAST].cap |= SANE_CAP_INACTIVE;
  else
    {
      /* Black level, Hue and Saturation are things the b&w cameras
         know nothing about.  Despeckle might be useful, but this code
         seems to work for color cameras only right now. The framesize
         seems to work better in these ranges.  */
      s->opt[OPT_DESPECKLE].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_BLACK_LEVEL].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_HUE].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_SATURATION].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_RESOLUTION].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_TEST].cap |= SANE_CAP_INACTIVE;

      s->opt[OPT_DEPTH].constraint.word_list = mono_depth_list;
      s->val[OPT_DEPTH].w = mono_depth_list[NELEMS (mono_depth_list) - 1];
      s->opt[OPT_TL_X].constraint.range = &bw_x_range;
      s->val[OPT_TL_X].w = 14;
      s->opt[OPT_TL_Y].constraint.range = &bw_y_range;
      s->val[OPT_TL_Y].w = 0;
      s->opt[OPT_BR_X].constraint.range = &odd_bw_x_range;
      s->val[OPT_BR_X].w = 333;
      s->opt[OPT_BR_Y].constraint.range = &odd_bw_y_range;
      s->val[OPT_BR_Y].w = 239;

      s->val[OPT_BRIGHTNESS].w = 170;
      s->val[OPT_CONTRAST].w = 150;
      s->val[OPT_WHITE_LEVEL].w = 150;
    }

  /* insert newly opened handle into list of open handles: */
  s->next = first_handle;
  first_handle = s;

  *handle = s;

  DBG (5, "sane_open: exit\n");

  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  QC_Scanner *prev, *s;

  DBG (5, "sane_close: enter\n");
  
  /* remove handle from list of open handles: */
  prev = 0;
  for (s = first_handle; s; s = s->next)
    {
      if (s == handle)
	break;
      prev = s;
    }
  if (!s)
    {
      DBG (1, "sane_close: bad handle %p\n", handle);
      return;			/* oops, not a handle we know about */
    }
  if (prev)
    prev->next = s->next;
  else
    first_handle = s->next;

  if (s->scanning)
    sane_cancel (handle);

  if (s->reader_pid >= 0)
    {
      kill (s->reader_pid, SIGTERM);
      waitpid (s->reader_pid, 0, 0);
      s->reader_pid = 0;
    }
  if (s->to_child >= 0)
    close (s->to_child);
  if (s->from_child >= 0)
    close (s->from_child);
  if (s->read_fd >= 0)
    close (s->read_fd);

  free (s);
  
  DBG (5, "sane_close: exit\n");
	  
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  QC_Scanner *s = handle;

  DBG (5, "sane_get_option_descriptor: enter\n");
  
  if ((unsigned) option >= NUM_OPTIONS)
    return 0;

  DBG (5, "sane_get_option_descriptor: exit\n");

  return s->opt + option;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  QC_Scanner *s = handle;
  QC_Resolution old_res;
  SANE_Status status;
  SANE_Word cap;
  char *old_val;
  int i;

  DBG (5, "sane_control_option: enter\n");
  
  if (info)
    *info = 0;

  if (option >= NUM_OPTIONS)
    return SANE_STATUS_INVAL;

  cap = s->opt[option].cap;

  if (!SANE_OPTION_IS_ACTIVE (cap))
    return SANE_STATUS_INVAL;

  if (action == SANE_ACTION_GET_VALUE)
    {
      switch (option)
	{
	  /* word options: */
	case OPT_NUM_OPTS:
	case OPT_DEPTH:
	case OPT_DESPECKLE:
	case OPT_TEST:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_XFER_SCALE:
	case OPT_BRIGHTNESS:
	case OPT_CONTRAST:
	case OPT_BLACK_LEVEL:
	case OPT_WHITE_LEVEL:
	case OPT_HUE:
	case OPT_SATURATION:
	  *(SANE_Word *) val = s->val[option].w;
	  return SANE_STATUS_GOOD;

	  /* string options: */
	case OPT_RESOLUTION:
	  strcpy (val, s->val[option].s);
	  return SANE_STATUS_GOOD;

	default:
	  DBG (1, "control_option: option %d unknown\n", option);
	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      if (!SANE_OPTION_IS_SETTABLE (cap))
	return SANE_STATUS_INVAL;

      status = sanei_constrain_value (s->opt + option, val, info);
      if (status != SANE_STATUS_GOOD)
	return status;

      if (option >= OPT_TL_X && option <= OPT_BR_Y)
	s->user_corner |= 1 << (option - OPT_TL_X);

      assert (option <= 31);
      s->value_changed |= 1 << option;

      switch (option)
	{
	  /* (mostly) side-effect-free word options: */
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_XFER_SCALE:
	case OPT_DEPTH:
	  if (!s->scanning && info && s->val[option].w != *(SANE_Word *) val)
	    /* only signal the reload params if we're not scanning---no point
	       in creating the frontend useless work */
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  /* fall through */
	case OPT_NUM_OPTS:
	case OPT_TEST:
	case OPT_DESPECKLE:
	case OPT_BRIGHTNESS:
	case OPT_CONTRAST:
	case OPT_BLACK_LEVEL:
	case OPT_WHITE_LEVEL:
	case OPT_HUE:
	case OPT_SATURATION:
	  s->val[option].w = *(SANE_Word *) val;
	  return SANE_STATUS_GOOD;

	  /* options with side-effects: */
	case OPT_RESOLUTION:
	  old_val = s->val[OPT_RESOLUTION].s;

	  if (strcmp (old_val, val) != 0)
	    return SANE_STATUS_GOOD;	/* no change */

	  if (info)
	    {
	      *info |= SANE_INFO_RELOAD_OPTIONS;
	      if (!s->scanning)
		*info |= SANE_INFO_RELOAD_PARAMS;
	    }
	  free (old_val);
	  s->val[OPT_RESOLUTION].s = strdup (val);

	  /* low-resolution mode: */
	  old_res = s->resolution;
	  s->resolution = QC_RES_LOW;
	  if (strcmp (val, resolution_list[QC_RES_HIGH]) == 0)
	    /* high-resolution mode: */
	    s->resolution = QC_RES_HIGH;
	  s->opt[OPT_TL_X].constraint.range = &x_range[s->resolution];
	  s->opt[OPT_BR_X].constraint.range = &odd_x_range[s->resolution];
	  s->opt[OPT_TL_Y].constraint.range = &y_range[s->resolution];
	  s->opt[OPT_BR_Y].constraint.range = &odd_y_range[s->resolution];

	  if (old_res == QC_RES_LOW && s->resolution == QC_RES_HIGH)
	    {
	      for (i = OPT_TL_X; i <= OPT_BR_Y; ++i)
		s->val[i].w *= 2;
	      s->val[OPT_BR_X].w += 1;
	      s->val[OPT_BR_Y].w += 1;
	      s->opt[OPT_TEST].cap |= SANE_CAP_INACTIVE;
	    }
	  else if (old_res == QC_RES_HIGH && s->resolution == QC_RES_LOW)
	    {
	      for (i = OPT_TL_X; i <= OPT_BR_Y; ++i)
		s->val[i].w /= 2;
	      s->opt[OPT_TEST].cap &= ~SANE_CAP_INACTIVE;
	    }

	  if (!(s->user_corner & 0x4))
	    s->val[OPT_BR_X].w = odd_x_range[s->resolution].max;
	  if (!(s->user_corner & 0x8))
	    s->val[OPT_BR_Y].w = odd_y_range[s->resolution].max - 4;

	  /* make sure the affected options have valid values: */
	  for (i = OPT_TL_X; i <= OPT_BR_Y; ++i)
	    if (s->val[i].w > s->opt[i].constraint.range->max)
	      s->val[i].w = s->opt[i].constraint.range->max;

          DBG (5, "sane_control_option: exit\n");
	  return SANE_STATUS_GOOD;
	}
    }
  else if (action == SANE_ACTION_SET_AUTO)
    {
      switch (option)
	{
	case OPT_BRIGHTNESS:
	  /* not implemented yet */
          DBG (5, "sane_control_option: exit\n");
	  return SANE_STATUS_GOOD;

	default:
	  break;
	}
    }

  DBG (5, "sane_control_option: NOK exit\n");
  return SANE_STATUS_INVAL;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  QC_Scanner *s = handle;
  QC_Device *q = s->hw;
  int xfer_scale;
  size_t Bpp = 3;		/* # of bytes per pixel */

  DBG (5, "sane_get_parameters: enter\n");
  
  if (!s->scanning)
    {
      /* Only compute new parameters when not scanning---allows
         changing width/height etc while scan is in progress.  */
      xfer_scale = s->val[OPT_XFER_SCALE].w;

      s->params.format = SANE_FRAME_RGB;
      if (q->version != QC_COLOR)
	{
	  s->params.format = SANE_FRAME_GRAY;
	  Bpp = 1;
	}
      s->params.last_frame = SANE_TRUE;

      s->params.pixels_per_line = s->val[OPT_BR_X].w - s->val[OPT_TL_X].w + 1;
      s->params.pixels_per_line /= xfer_scale;
      s->params.pixels_per_line &= ~1UL;	/* ensure it's even */
      if (s->params.pixels_per_line < 2)
	s->params.pixels_per_line = 2;

      s->params.lines = s->val[OPT_BR_Y].w - s->val[OPT_TL_Y].w + 1;
      s->params.lines /= xfer_scale;
      if (s->params.lines < 1)
	s->params.lines = 1;

      s->params.bytes_per_line = Bpp * s->params.pixels_per_line;
      s->params.depth = 8;
    }
  if (params)
    *params = s->params;

  DBG (5, "sane_get_parameters: exit\n");

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  int top, left, width, height, undecimated_width, undecimated_height;
  QC_Scanner *s = handle;
  QC_Device *q = s->hw;
  QC_Scan_Request req;

  DBG (5, "sane_start: enter\n");
  
  if (s->scanning)
    return SANE_STATUS_DEVICE_BUSY;

  if (s->reader_pid < 0)
    {
      int p2c_pipe[2];		/* parent->child pipe */
      int c2p_pipe[2];		/* child->parent pipe */

      if (pipe (p2c_pipe) < 0 || pipe (c2p_pipe) < 0)
	{
	  DBG (3, "start: failed to create pipes\n");
	  return SANE_STATUS_IO_ERROR;
	}

      s->reader_pid = fork ();
      if (s->reader_pid == 0)
	{
	  /* this is the child */
	  signal (SIGHUP, SIG_DFL);
	  signal (SIGINT, SIG_DFL);
	  signal (SIGPIPE, SIG_DFL);
	  signal (SIGTERM, SIG_DFL);
	  _exit (reader_process (s, p2c_pipe[0], c2p_pipe[1]));
	}
      close (p2c_pipe[0]);
      close (c2p_pipe[1]);
      s->to_child = p2c_pipe[1];
      s->from_child = c2p_pipe[0];
    }

  s->read_fd = dup (s->from_child);
  sane_get_parameters (s, 0);	/* ensure uptodate parameters */

  qc_lock (q);
  s->holding_lock = SANE_TRUE;

  if (q->version == QC_COLOR)
    {
      qc_send (q, QC_SET_SPEED);
      qc_send (q, 2);

      /* wait for camera to become ready: */
      while (qc_getstatus (q) & CameraNotReady)
	usleep (10000);

      /* Only send black_level if necessary; this optimization may
         fail if two applications access the camera in an interleaved
         fashion; but the black-level command is slow enough that it
         cannot be issued for every image acquisition.  */
      if (s->value_changed & (1 << OPT_BLACK_LEVEL))
	{
	  s->value_changed &= ~(1 << OPT_BLACK_LEVEL);

	  qc_send (q, QC_SET_BLACK);
	  qc_send (q, s->val[OPT_BLACK_LEVEL].w);

	  DBG (3, "start: black_level=%d\n", s->val[OPT_BLACK_LEVEL].w);

	  /* wait for set black level command to finish: */
	  while (qc_getstatus (q) & (CameraNotReady | BlackBalanceInProgress))
	    usleep (10000);
	}

      if (s->value_changed & (1 << OPT_HUE))
	{
	  s->value_changed &= ~(1 << OPT_HUE);
	  qc_send (q, QC_COL_SET_HUE);
	  qc_send (q, s->val[OPT_HUE].w);
	}

      if (s->value_changed & (1 << OPT_SATURATION))
	{
	  s->value_changed &= ~(1 << OPT_SATURATION);
	  qc_send (q, QC_SET_SATURATION);
	  qc_send (q, s->val[OPT_SATURATION].w);
	}
    }

  if (q->version != QC_COLOR)
    qc_reset (q);

  if (s->value_changed & (1 << OPT_CONTRAST))
    {
      s->value_changed &= ~(1 << OPT_CONTRAST);
      qc_send (q, ((q->version == QC_COLOR)
		   ? QC_COL_SET_CONTRAST : QC_MONO_SET_CONTRAST));
      qc_send (q, s->val[OPT_CONTRAST].w);
    }

  if (s->value_changed & (1 << OPT_BRIGHTNESS))
    {
      s->value_changed &= ~(1 << OPT_BRIGHTNESS);
      qc_send (q, QC_SET_BRIGHTNESS);
      qc_send (q, s->val[OPT_BRIGHTNESS].w);
    }

  width = s->params.pixels_per_line;
  height = s->params.lines;
  if (s->resolution == QC_RES_HIGH)
    {
      width /= 2;		/* the expansion occurs through oversampling */
      height /= 2;		/* we acquire only half the lines that we generate */
    }
  undecimated_width = width * s->val[OPT_XFER_SCALE].w;
  undecimated_height = height * s->val[OPT_XFER_SCALE].w;

  s->num_bytes = 0;
  s->bytes_per_frame = s->params.lines * s->params.bytes_per_line;

  qc_send (q, QC_SET_NUM_V);
  qc_send (q, undecimated_height);

  if (q->version == QC_COLOR)
    {
      qc_send (q, QC_SET_NUM_H);
      qc_send (q, undecimated_width / 2);
    }
  else
    {
      int val, val2;

      if (q->port_mode == QC_UNIDIR && s->val[OPT_DEPTH].w == 6)
	{
	  val = undecimated_width;
	  val2 = s->val[OPT_XFER_SCALE].w * 4;
	}
      else
	{
	  val = undecimated_width * s->val[OPT_DEPTH].w;
	  val2 =
	    ((q->port_mode == QC_BIDIR) ? 24 : 8) * s->val[OPT_XFER_SCALE].w;
	}
      val = (val + val2 - 1) / val2;
      qc_send (q, QC_SET_NUM_H);
      qc_send (q, val);
    }

  left = s->val[OPT_TL_X].w / 2;
  top = s->val[OPT_TL_Y].w;
  if (s->resolution == QC_RES_HIGH)
    {
      left /= 2;
      top /= 2;
    }

  DBG (3, "sane_start: top=%d, left=%d, white=%d, bright=%d, contr=%d\n",
       top, left, s->val[OPT_WHITE_LEVEL].w, s->val[OPT_BRIGHTNESS].w,
       s->val[OPT_CONTRAST].w);

  qc_send (q, QC_SET_LEFT);
  qc_send (q, left);

  qc_send (q, QC_SET_TOP);
  qc_send (q, top + 1);		/* not sure why this is so... ;-( */

  if (s->value_changed & (1 << OPT_WHITE_LEVEL))
    {
      s->value_changed &= ~(1 << OPT_WHITE_LEVEL);
      qc_send (q, QC_SET_WHITE);
      qc_send (q, s->val[OPT_WHITE_LEVEL].w);
    }

  DBG (2, "start: %s %d lines of %d pixels each (%ld bytes) => %dx%d\n",
       (q->port_mode == QC_BIDIR) ? "bidir" : "unidir",
       height, width, (long) s->bytes_per_frame,
       s->params.pixels_per_line, s->params.lines);

  /* send scan request to reader process: */
  qc_setscanmode (s, &req.mode);
  req.num_bytes = width * height;
  if (q->version == QC_COLOR)
    {
      if (s->resolution == QC_RES_LOW)
	req.num_bytes *= 3;
      else
	req.num_bytes *= 4;
    }
  req.resolution = s->resolution;
  req.params = s->params;
  req.despeckle = s->val[OPT_DESPECKLE].w;
  write (s->to_child, &req, sizeof (req));

  s->scanning = SANE_TRUE;
  s->deliver_eof = 0;

  DBG (5, "sane_start: exit\n");
  
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len,
	   SANE_Int * lenp)
{
  SANE_Status status;
  QC_Scanner *s = handle;
  QC_Device *q = s->hw;
  ssize_t nread;
  size_t len;

  DBG (5, "sane_read: enter\n");
  
  *lenp = 0;

  if (s->deliver_eof)
    {
      s->deliver_eof = 0;
      return SANE_STATUS_EOF;
    }

  if (!s->scanning)
    return SANE_STATUS_CANCELLED;

  len = max_len;
  if (s->num_bytes + len > s->bytes_per_frame)
    len = s->bytes_per_frame - s->num_bytes;

  DBG (8, "read(buf=%p,num_bytes=%ld,max_len=%d,len=%ld)\n",
       buf, (long) s->num_bytes, max_len, (long) len);

  nread = read (s->read_fd, buf, len);
  if (nread <= 0)
    {
      if (nread == 0 || errno == EAGAIN)
	{
	  DBG (3, "read: no more data available\n");
	  return SANE_STATUS_GOOD;
	}
      DBG (3, "read: short read (%s)\n", strerror (errno));
      return SANE_STATUS_IO_ERROR;
    }

  if (nread > 0 && s->holding_lock)
    {
      status = qc_unlock (q);	/* now we can unlock the camera */
      if (status != SANE_STATUS_GOOD)
	      DBG(3, "sane_read: qc_unlock error\n");
      s->holding_lock = SANE_FALSE;
    }

  s->num_bytes += nread;
  if (s->num_bytes >= s->bytes_per_frame)
    {
      s->scanning = SANE_FALSE;
      close (s->read_fd);
      s->read_fd = -1;
      s->deliver_eof = 1;
    }

  if (lenp)
    *lenp = nread;

  DBG (5, "sane_read: exit, read got %d bytes\n", *lenp);
  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  QC_Scanner *s = handle;
  SANE_Bool was_scanning;
  SANE_Status status;

  DBG (5, "sane_cancel: enter\n");
  
  was_scanning = s->scanning;
  s->scanning = SANE_FALSE;
  s->deliver_eof = 0;
  if (s->read_fd >= 0)
    {
      close (s->read_fd);
      s->read_fd = -1;
    }

  if (s->reader_pid >= 0 && was_scanning)
    {
      char buf[1024];
      ssize_t nread;
      int flags;

      DBG (1, "cancel: cancelling read request\n");

      kill (s->reader_pid, SIGINT);	/* tell reader to stop reading */

      /* save non-blocking i/o flags: */
      flags = fcntl (s->from_child, F_GETFL, 0);

      /* block until we read at least one byte: */
      read (s->from_child, buf, 1);

      /* put descriptor in non-blocking i/o: */
      fcntl (s->from_child, F_SETFL, O_NONBLOCK);

      /* read what's left over in the pipe/file buffer: */
      do
	{
	  while ((nread = read (s->from_child, buf, sizeof (buf))) > 0);
	  usleep (100000);
	  nread = read (s->from_child, buf, sizeof (buf));
	}
      while (nread > 0);

      /* now restore non-blocking i/o flag: */
      fcntl (s->from_child, F_SETFL, flags & O_NONBLOCK);

      waitpid (s->reader_pid, 0, 0);
      s->reader_pid = 0;

      DBG (1, "cancel: cancellation completed\n");
    }
  if (s->holding_lock)
    {
      status = qc_unlock (s->hw);
      if (status != SANE_STATUS_GOOD)
	      DBG(3, "sane_cancel: qc_unlock error\n");
      s->holding_lock = SANE_FALSE;
    }
  DBG (5, "sane_cancel: exit\n");
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  QC_Scanner *s = handle;

  DBG (5, "sane_set_io_mode: enter\n");
  
  if (!s->scanning)
    return SANE_STATUS_INVAL;

  if (fcntl (s->read_fd, F_SETFL, non_blocking ? O_NONBLOCK : 0) < 0)
    return SANE_STATUS_IO_ERROR;
  DBG (5, "sane_set_io_mode: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  QC_Scanner *s = handle;

  DBG (5, "sane_get_select_fd: enter\n");
  if (!s->scanning)
    return SANE_STATUS_INVAL;

  *fd = s->read_fd;
  DBG (5, "sane_get_select_fd: exit\n");
  return SANE_STATUS_GOOD;
}
