/* sane - Scanner Access Now Easy.
   Copyright (C) 2003 James Perry
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

   This file implements the Mustek SCSI-over-parallel port protocol
   used by, for example, the Paragon 600 II EP
*/


/**************************************************************************/
#include "../include/sane/config.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <time.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_debug.h"
#include "../include/sane/sanei_pa4s2.h"

/*
 * Number of times to retry sending a SCSI command before giving up
 */
#define MUSTEK_SCSI_PP_NUM_RETRIES 4

/*
 * Internal middle-level API functionality
 */
static int mustek_scsi_pp_timeout = 5000;

/* FIXME: use same method as mustek.c ? */
static int
mustek_scsi_pp_get_time ()
{
  struct timeval tv;
  int retval;

  gettimeofday (&tv, 0);

  retval = tv.tv_sec * 1000 + tv.tv_usec / 1000;

  return retval;
}

static u_char mustek_scsi_pp_register = 0;


static SANE_Status
mustek_scsi_pp_select_register (int fd, u_char reg)
{
  DBG (5, "mustek_scsi_pp_select_register: selecting register %d on fd %d\n",
       reg, fd);

  mustek_scsi_pp_register = reg;

  return sanei_pa4s2_scsi_pp_reg_select (fd, reg);
}

static SANE_Status
mustek_scsi_pp_wait_for_valid_status (int fd)
{
  int start_time;
  u_char status;

  DBG (5, "mustek_scsi_pp_wait_for_valid_status: entering\n");

  start_time = mustek_scsi_pp_get_time ();

  do
    {
      if (sanei_pa4s2_scsi_pp_get_status (fd, &status) != SANE_STATUS_GOOD)
	{
	  DBG (2,
	       "mustek_scsi_pp_wait_for_valid_status: I/O error while getting status\n");
	  return SANE_STATUS_IO_ERROR;
	}

      status &= 0xf0;

      if ((status != 0xf0) && (!(status & 0x40)) && (status & 0x20))
	{
	  DBG (5,
	       "mustek_scsi_pp_wait_for_valid_status: returning success\n");
	  return SANE_STATUS_GOOD;
	}
    }
  while ((mustek_scsi_pp_get_time () - start_time) < mustek_scsi_pp_timeout);

  DBG (2, "mustek_scsi_pp_wait_for_valid_status: timed out\n");
  return SANE_STATUS_DEVICE_BUSY;
}

static SANE_Status
mustek_scsi_pp_wait_for_status_bit_5_set (int fd)
{
  int t;
  u_char status;

  DBG (5, "mustek_scsi_pp_wait_for_status_bit_5_set: entering\n");

  t = mustek_scsi_pp_get_time ();

  do
    {
      if (sanei_pa4s2_scsi_pp_get_status (fd, &status) != SANE_STATUS_GOOD)
	{
	  DBG (2, "mustek_scsi_pp_wait_for_status_bit_5_set: I/O error\n");
	  return SANE_STATUS_IO_ERROR;
	}
      if (status & 0x20)
	{
	  DBG (5,
	       "mustek_scsi_pp_wait_for_status_bit_5_set: returning success\n");
	  return SANE_STATUS_GOOD;
	}
    }
  while ((mustek_scsi_pp_get_time () - t) < mustek_scsi_pp_timeout);

  DBG (2, "mustek_scsi_pp_wait_for_status_bit_5_set: timed out\n");
  return SANE_STATUS_DEVICE_BUSY;
}

static SANE_Status
mustek_scsi_pp_wait_for_status_bit_5_clear (int fd)
{
  int t;
  u_char status;

  DBG (5, "mustek_scsi_pp_wait_for_status_bit_5_clear: entering\n");

  t = mustek_scsi_pp_get_time ();

  do
    {
      if (sanei_pa4s2_scsi_pp_get_status (fd, &status) != SANE_STATUS_GOOD)
	{
	  DBG (2, "mustek_scsi_pp_wait_for_status_bit_5_clear: I/O error\n");
	  return SANE_STATUS_IO_ERROR;
	}

      if (!(status & 0x20))
	{
	  DBG (5,
	       "mustek_scsi_pp_wait_for_status_bit_5_clear: returning success\n");
	  return SANE_STATUS_GOOD;
	}
    }
  while ((mustek_scsi_pp_get_time () - t) < mustek_scsi_pp_timeout);

  DBG (2, "mustek_scsi_pp_wait_for_status_bit_5_clear: timed out\n");
  return SANE_STATUS_DEVICE_BUSY;
}

static SANE_Status
mustek_scsi_pp_wait_for_status_bit_7_set (int fd)
{
  int t;
  u_char status;

  DBG (5, "mustek_scsi_pp_wait_for_status_bit_7_set: entering\n");

  t = mustek_scsi_pp_get_time ();
  do
    {
      if (sanei_pa4s2_scsi_pp_get_status (fd, &status) != SANE_STATUS_GOOD)
	{
	  DBG (2, "mustek_scsi_pp_wait_for_status_bit_7_set: I/O error\n");
	  return SANE_STATUS_IO_ERROR;
	}
      if (status & 0x80)
	{
	  DBG (5,
	       "mustek_scsi_pp_wait_for_status_bit_7_set: returning success\n");
	  return SANE_STATUS_GOOD;
	}
    }
  while ((mustek_scsi_pp_get_time () - t) < mustek_scsi_pp_timeout);
  mustek_scsi_pp_select_register (fd, 0);

  DBG (2, "mustek_scsi_pp_wait_for_status_bit_7_set: timed out\n");
  return SANE_STATUS_DEVICE_BUSY;
}

static SANE_Status
mustek_scsi_pp_wait_for_status_bit_7_clear (int fd)
{
  int t;
  u_char status;

  DBG (5, "mustek_scsi_pp_wait_for_status_bit_7_clear: entering\n");

  t = mustek_scsi_pp_get_time ();
  do
    {
      if (sanei_pa4s2_scsi_pp_get_status (fd, &status) != SANE_STATUS_GOOD)
	{
	  DBG (2, "mustek_scsi_pp_wait_for_status_bit_7_clear: I/O error\n");
	  return SANE_STATUS_IO_ERROR;
	}
      if (!(status & 0x80))
	{
	  DBG (5,
	       "mustek_scsi_pp_wait_for_status_bit_7_clear: returning success\n");
	  return SANE_STATUS_GOOD;
	}
    }
  while ((mustek_scsi_pp_get_time () - t) < mustek_scsi_pp_timeout);
  mustek_scsi_pp_select_register (fd, 0);

  DBG (2, "mustek_scsi_pp_wait_for_status_bit_7_clear: timed out\n");
  return SANE_STATUS_DEVICE_BUSY;
}

static SANE_Status
mustek_scsi_pp_wait_for_status_bit_4_set (int fd)
{
  int t;
  u_char status;

  DBG (5, "mustek_scsi_pp_wait_for_status_bit_4_set: entering\n");

  if (sanei_pa4s2_scsi_pp_get_status (fd, &status) != SANE_STATUS_GOOD)
    {
      DBG (2, "mustek_scsi_pp_wait_for_status_bit_4_set: I/O error\n");
      return SANE_STATUS_IO_ERROR;
    }

  if (status & 0x10)
    {
      DBG (5,
	   "mustek_scsi_pp_wait_for_status_bit_4_set: returning success\n");
      return SANE_STATUS_GOOD;
    }

  t = mustek_scsi_pp_get_time ();
  do
    {
      if (sanei_pa4s2_scsi_pp_get_status (fd, &status) != SANE_STATUS_GOOD)
	{
	  DBG (2, "mustek_scsi_pp_wait_for_status_bit_4_set: I/O error\n");
	  return SANE_STATUS_IO_ERROR;
	}
      if (status & 0x40)
	{
	  DBG (2, "mustek_scsi_pp_wait_for_status_bit_4_set: bit 6 set\n");
	  return SANE_STATUS_IO_ERROR;
	}
      if (status & 0x10)
	{
	  DBG (5,
	       "mustek_scsi_pp_wait_for_status_bit_4_set: returning success\n");
	  return SANE_STATUS_GOOD;
	}
    }
  while ((mustek_scsi_pp_get_time () - t) < mustek_scsi_pp_timeout);

  DBG (2, "mustek_scsi_pp_wait_for_status_bit_4_set: timed out\n");
  return SANE_STATUS_DEVICE_BUSY;
}

static SANE_Status
mustek_scsi_pp_wait_for_status_bit_4_clear (int fd)
{
  int t;
  u_char status;

  DBG (5, "mustek_scsi_pp_wait_for_status_bit_4_clear: entering\n");

  if (sanei_pa4s2_scsi_pp_get_status (fd, &status) != SANE_STATUS_GOOD)
    {
      DBG (2, "mustek_scsi_pp_wait_for_status_bit_4_clear: I/O error\n");
      return SANE_STATUS_IO_ERROR;
    }

  if (!(status & 0x10))
    {
      DBG (5,
	   "mustek_scsi_pp_wait_for_status_bit_4_clear: returning success\n");
      return SANE_STATUS_GOOD;
    }

  t = mustek_scsi_pp_get_time ();
  do
    {
      if (sanei_pa4s2_scsi_pp_get_status (fd, &status) != SANE_STATUS_GOOD)
	{
	  DBG (2, "mustek_scsi_pp_wait_for_status_bit_4_clear: I/O error\n");
	  return SANE_STATUS_IO_ERROR;
	}
      if (status & 0x40)
	{
	  DBG (2, "mustek_scsi_pp_wait_for_status_bit_4_clear: bit 6 set\n");
	  return SANE_STATUS_IO_ERROR;
	}
      if (!(status & 0x10))
	{
	  DBG (5,
	       "mustek_scsi_pp_wait_for_status_bit_4_clear: returning success\n");
	  return SANE_STATUS_GOOD;
	}
    }
  while ((mustek_scsi_pp_get_time () - t) < mustek_scsi_pp_timeout);

  DBG (2, "mustek_scsi_pp_wait_for_status_bit_4_clear: timed out\n");
  return SANE_STATUS_DEVICE_BUSY;
}

static u_char mustek_scsi_pp_bit_4_state = 0;

static SANE_Status
mustek_scsi_pp_wait_for_status_bit_4_toggle (int fd)
{
  SANE_Status result;

  DBG (5, "mustek_scsi_pp_wait_for_status_bit_4_toggle: entering\n");

  mustek_scsi_pp_bit_4_state ^= 0xff;
  if (mustek_scsi_pp_bit_4_state)
    {
      DBG (5,
	   "mustek_scsi_pp_wait_for_status_bit_4_toggle: waiting for set\n");
      result = mustek_scsi_pp_wait_for_status_bit_4_set (fd);
      mustek_scsi_pp_timeout = 5000;
    }
  else
    {
      DBG (5,
	   "mustek_scsi_pp_wait_for_status_bit_4_toggle: waiting for clear\n");
      result = mustek_scsi_pp_wait_for_status_bit_4_clear (fd);
    }

  return result;
}

static SANE_Status
mustek_scsi_pp_send_command_byte (int fd, u_char cmd)
{
  DBG (5, "mustek_scsi_pp_send_command byte: sending 0x%02X\n", cmd);

  mustek_scsi_pp_select_register (fd, 0);

  if (mustek_scsi_pp_wait_for_status_bit_7_clear (fd) != SANE_STATUS_GOOD)
    {
      mustek_scsi_pp_select_register (fd, 0);
      return SANE_STATUS_IO_ERROR;
    }

  if (sanei_pa4s2_writebyte (fd, mustek_scsi_pp_register, cmd) !=
      SANE_STATUS_GOOD)
    {
      return SANE_STATUS_IO_ERROR;
    }

  mustek_scsi_pp_select_register (fd, 1);

  if (mustek_scsi_pp_wait_for_status_bit_7_set (fd) != SANE_STATUS_GOOD)
    {
      mustek_scsi_pp_select_register (fd, 0);
      return SANE_STATUS_IO_ERROR;
    }
  mustek_scsi_pp_select_register (fd, 0);

  DBG (5, "mustek_scsi_pp_send_command_byte: returning success\n");
  return SANE_STATUS_GOOD;
}

static u_char
mustek_scsi_pp_read_response (int fd)
{
  u_char result;

  DBG (5, "mustek_scsi_pp_read_response: entering\n");

  if (mustek_scsi_pp_wait_for_status_bit_7_set (fd) != SANE_STATUS_GOOD)
    {
      mustek_scsi_pp_select_register (fd, 0);
      return 0xff;
    }

  if (sanei_pa4s2_readbegin (fd, mustek_scsi_pp_register) != SANE_STATUS_GOOD)
    {
      return 0xff;
    }
  if (sanei_pa4s2_readbyte (fd, &result) != SANE_STATUS_GOOD)
    {
      return 0xff;
    }
  if (sanei_pa4s2_readend (fd) != SANE_STATUS_GOOD)
    {
      return 0xff;
    }

  mustek_scsi_pp_select_register (fd, 1);
  if (mustek_scsi_pp_wait_for_status_bit_7_clear (fd) != SANE_STATUS_GOOD)
    {
      result = 0xff;
    }
  mustek_scsi_pp_select_register (fd, 0);

  DBG (5, "mustek_scsi_pp_read_response: returning 0x%02X\n", result);
  return result;
}

static SANE_Status
mustek_scsi_pp_check_response (int fd)
{
  if (mustek_scsi_pp_wait_for_status_bit_5_clear (fd) != SANE_STATUS_GOOD)
    {
      return SANE_STATUS_IO_ERROR;
    }

  if (mustek_scsi_pp_read_response (fd) != 0xA5)
    {
      DBG (2, "mustek_scsi_pp_check_response: response!=0xA5\n");
      return SANE_STATUS_IO_ERROR;
    }

  DBG (5, "mustek_scsi_pp_check_response: returning success\n");
  return SANE_STATUS_GOOD;
}

static SANE_Status
mustek_scsi_pp_send_command (int fd, const u_char * cmd)
{
  int i;
  signed char checksum;

  DBG (5, "mustek_scsi_pp_send_command: sending SCSI command 0x%02X\n",
       cmd[0]);

  /* Set timeout depending on command type */
  switch (cmd[0])
    {
    case 0xf:
    case 0x8:
      mustek_scsi_pp_timeout = 1000;
      break;
    case 0x2:
      mustek_scsi_pp_timeout = 80;
      break;
    case 0x12:
    case 0x3:
    case 0x11:
      mustek_scsi_pp_timeout = 500;
      break;
    default:
      mustek_scsi_pp_timeout = 1000;
      break;
    }

  if (mustek_scsi_pp_wait_for_status_bit_5_set (fd) != SANE_STATUS_GOOD)
    {
      DBG (2,
	   "mustek_scsi_pp_send_command: timed out waiting for bit 5 to set\n");
      return SANE_STATUS_DEVICE_BUSY;
    }

  checksum = 0;
  for (i = 0; i < 6; i++)
    {
      if (mustek_scsi_pp_send_command_byte (fd, cmd[i]) != SANE_STATUS_GOOD)
	{
	  DBG (2,
	       "mustek_scsi_pp_send_command: error sending byte %d (0x%02X)\n",
	       i, cmd[i]);
	  return SANE_STATUS_IO_ERROR;
	}
      checksum += cmd[i];
    }
  if (mustek_scsi_pp_send_command_byte (fd, -checksum) != SANE_STATUS_GOOD)
    {
      DBG (2,
	   "mustek_scsi_pp_send_command: error sending checksum (0x%02X)\n",
	   -checksum);
      return SANE_STATUS_IO_ERROR;
    }
  return mustek_scsi_pp_check_response (fd);
}

static SANE_Status
mustek_scsi_pp_send_data_block (int fd, const u_char * data, int len)
{
  int i;
  signed char checksum;

  DBG (5, "mustek_scsi_pp_send_data_block: sending block of length %d\n",
       len);

  if (mustek_scsi_pp_wait_for_status_bit_5_set (fd) != SANE_STATUS_GOOD)
    {
      DBG (2,
	   "mustek_scsi_pp_send_data_block: timed out waiting for bit 5 to set\n");
      return SANE_STATUS_DEVICE_BUSY;
    }

  checksum = 0;
  for (i = 0; i < len; i++)
    {
      if (mustek_scsi_pp_send_command_byte (fd, data[i]) != SANE_STATUS_GOOD)
	{
	  DBG (2,
	       "mustek_scsi_pp_send_data_block: error sending byte %d (0x%02X)\n",
	       i, data[i]);
	  return SANE_STATUS_IO_ERROR;
	}
      checksum += data[i];
    }
  if (mustek_scsi_pp_send_command_byte (fd, -checksum) != SANE_STATUS_GOOD)
    {
      DBG (2,
	   "mustek_scsi_pp_send_data_block: error sending checksum (0x%02X)\n",
	   -checksum);
      return SANE_STATUS_IO_ERROR;
    }
  return mustek_scsi_pp_check_response (fd);
}

static SANE_Status
mustek_scsi_pp_read_data_block (int fd, u_char * buffer, int len)
{
  int i;
  signed char checksum;

  DBG (5, "mustek_scsi_pp_read_data_block: reading block of length %d\n",
       len);

  if (mustek_scsi_pp_wait_for_status_bit_5_clear (fd) != SANE_STATUS_GOOD)
    {
      DBG (2,
	   "mustek_scsi_pp_read_data_block: timed out waiting for bit 5 to clear\n");
      return SANE_STATUS_DEVICE_BUSY;
    }

  checksum = 0;
  for (i = 0; i < len; i++)
    {
      buffer[i] = mustek_scsi_pp_read_response (fd);
      checksum += buffer[i];
    }
  if ((signed char) mustek_scsi_pp_read_response (fd) != (-checksum))
    {
      mustek_scsi_pp_send_command_byte (fd, 0xff);
      DBG (2, "mustek_scsi_pp_read_data_block: checksums do not match\n");
      return SANE_STATUS_IO_ERROR;
    }
  if (mustek_scsi_pp_wait_for_status_bit_5_set (fd) != SANE_STATUS_GOOD)
    {
      DBG (2,
	   "mustek_scsi_pp_read_data_block: error waiting for bit 5 to set\n");
      return SANE_STATUS_IO_ERROR;
    }
  if (mustek_scsi_pp_send_command_byte (fd, 0) != SANE_STATUS_GOOD)
    {
      mustek_scsi_pp_send_command_byte (fd, 0xff);
      DBG (2, "mustek_scsi_pp_read_data_block: error sending final 0 byte\n");
      return SANE_STATUS_IO_ERROR;
    }

  DBG (5, "mustek_scsi_pp_read_data_block: returning success\n");
  return SANE_STATUS_GOOD;
}



/*
 * Externally visible functions
 */
SANE_Status
mustek_scsi_pp_open (const char *dev, int *fd)
{
  SANE_Status status;

  status = sanei_pa4s2_scsi_pp_open (dev, fd);
  if (status == SANE_STATUS_GOOD)
    {
      DBG (5, "mustek_scsi_pp_open: device %s opened as fd %d\n", dev, *fd);
    }
  else
    {
      DBG (2, "mustek_scsi_pp_open: error opening device %s\n", dev);
    }
  return status;
}

static void
mustek_scsi_pp_close (int fd)
{
  DBG (5, "mustek_scsi_pp_close: closing fd %d\n", fd);
  sanei_pa4s2_close (fd);
}

static void
mustek_scsi_pp_exit (void)
{
  DBG (5, "mustek_scsi_pp_exit: entering\n");
}

static SANE_Status
mustek_scsi_pp_test_ready (int fd)
{
  u_char status;
  SANE_Status retval;

  DBG (5, "mustek_scsi_pp_test_ready: entering with fd=%d\n", fd);

  if (sanei_pa4s2_enable (fd, SANE_TRUE) != SANE_STATUS_GOOD)
    {
      DBG (2, "mustek_scsi_pp_test_ready: error enabling scanner\n");
      return SANE_STATUS_IO_ERROR;
    }

  if (sanei_pa4s2_scsi_pp_get_status (fd, &status) != SANE_STATUS_GOOD)
    {
      DBG (2, "mustek_scsi_pp_test_ready: error getting status\n");
      sanei_pa4s2_enable (fd, SANE_FALSE);
      return SANE_STATUS_INVAL;
    }

  retval = SANE_STATUS_GOOD;

  status &= 0xf0;

  if (status == 0xf0)
    {
      retval = SANE_STATUS_DEVICE_BUSY;
    }
  if (status & 0x40)
    {
      retval = SANE_STATUS_DEVICE_BUSY;
    }
  if (!(status & 0x20))
    {
      retval = SANE_STATUS_DEVICE_BUSY;
    }

  if (sanei_pa4s2_enable (fd, SANE_FALSE) != SANE_STATUS_GOOD)
    {
      DBG (2, "mustek_scsi_pp_test_ready: error disabling scanner\n");
      return SANE_STATUS_IO_ERROR;
    }

  if (retval == SANE_STATUS_GOOD)
    {
      DBG (5, "mustek_scsi_pp_test_ready: returning SANE_STATUS_GOOD\n");
    }
  else
    {
      DBG (5,
	   "mustek_scsi_pp_test_ready: returning SANE_STATUS_DEVICE_BUSY\n");
    }

  return retval;
}

static SANE_Status
mustek_scsi_pp_cmd (int fd, const void *src, size_t src_size,
		    void *dst, size_t * dst_size)
{
  SANE_Status stat;
  int num_tries = 0;
  static u_char scan_options = 0;
  const u_char *cmd;
  u_char stop_cmd[6] = { 0x1b, 0, 0, 0, 0, 0 };
  int max_tries;

  max_tries = MUSTEK_SCSI_PP_NUM_RETRIES;

  cmd = (const u_char *) src;

  DBG (5, "mustek_scsi_pp_cmd: sending command 0x%02X to device %d\n",
       cmd[0], fd);

  if (sanei_pa4s2_enable (fd, SANE_TRUE) != SANE_STATUS_GOOD)
    {
      DBG (2, "mustek_scsi_pp_cmd: error enabling scanner\n");
      return SANE_STATUS_IO_ERROR;
    }

  if (cmd[0] == 0x1b)
    {
      if (!(cmd[4] & 0x1))
	{
	  unsigned char c;
	  int i;

	  DBG (5, "mustek_scsi_pp_cmd: doing stop-specific stuff\n");

	  /*
	   * Remembers what flags were sent with a 'start' command, and
	   * replicate them with a stop command.
	   */
	  stop_cmd[4] = scan_options & 0xfe;
	  cmd = &stop_cmd[0];

	  /*
	   * In color mode at least, the scanner doesn't seem to like stopping at
	   * the end. It's a bit of a horrible hack, but reading loads of bytes and
	   * allowing 20 tries for the stop command is the only way I've found that
	   * solves the problem.
	   */
	  max_tries = 20;

	  if (sanei_pa4s2_readbegin (fd, mustek_scsi_pp_register) !=
	      SANE_STATUS_GOOD)
	    {
	      DBG (2, "mustek_scsi_pp_cmd: error in readbegin for stop\n");
	    }

	  for (i = 0; i < 10000; i++)
	    {
	      if (sanei_pa4s2_readbyte (fd, &c) != SANE_STATUS_GOOD)
		{
		  DBG (2,
		       "mustek_scsi_pp_cmd: error reading byte for stop\n");
		  break;
		}
	      DBG (5, "mustek_scsi_pp_cmd: successfully read byte %d\n", i);
	    }
	  if (sanei_pa4s2_readend (fd) != SANE_STATUS_GOOD)
	    {
	      DBG (2, "mustek_scsi_pp_cmd: error in readend for stop\n");
	    }
	}
    }

  if (cmd[0] == 0x08)
    {
      DBG (5, "mustek_scsi_pp_cmd: doing read-specific stuff\n");
      mustek_scsi_pp_timeout = 30000;
      mustek_scsi_pp_bit_4_state = 0xff;
    }

  /*
   * Send the command itself in one block, then any extra input data in a second
   * block. Not sure if that's necessary.
   */
  if (src_size < 6)
    {
      sanei_pa4s2_enable (fd, SANE_FALSE);
      DBG (2, "mustek_scsi_pp_cmd: source size is only %lu (<6)\n", (u_long) src_size);
      return SANE_STATUS_INVAL;
    }

  /*
   * Retry the command several times, as occasionally it doesn't
   * work first time. 
   */
  do
    {
      stat = mustek_scsi_pp_send_command (fd, cmd);
      num_tries++;
    }
  while ((stat != SANE_STATUS_GOOD) && (num_tries < max_tries));

  if (stat != SANE_STATUS_GOOD)
    {
      sanei_pa4s2_enable (fd, SANE_FALSE);
      DBG (2, "mustek_scsi_pp_cmd: sending command failed\n");
      return stat;
    }

  if (src_size > 6)
    {
      DBG (5, "mustek_scsi_pp_cmd: sending data block of length %lu\n",
	   (u_long) (src_size - 6));

      stat =
	mustek_scsi_pp_send_data_block (fd, ((const u_char *) src) + 6,
					src_size - 6);
      if (stat != SANE_STATUS_GOOD)
	{
	  sanei_pa4s2_enable (fd, SANE_FALSE);
	  DBG (2, "mustek_scsi_pp_cmd: sending data block failed\n");
	  return stat;
	}
    }


  if (dst)
    {
      unsigned int length;

      /* check buffer is big enough to receive data */
      length = (cmd[3] << 8) | cmd[4];

      DBG (5, "mustek_scsi_pp_cmd: reading %d bytes\n", length);

      if (length > *dst_size)
	{
	  sanei_pa4s2_enable (fd, SANE_FALSE);
	  DBG (2,
	       "mustek_scsi_pp_cmd: buffer (size %lu) not big enough for data (size %d)\n",
	       (u_long) *dst_size, length);
	  return SANE_STATUS_INVAL;
	}

      stat = mustek_scsi_pp_read_data_block (fd, dst, length);
      if (stat != SANE_STATUS_GOOD)
	{
	  DBG (2, "mustek_scsi_pp_cmd: error reading data block\n");
	}
    }

  if (cmd[0] == 0x1b)
    {
      if (cmd[4] & 0x1)
	{
	  DBG (5, "mustek_scsi_pp_cmd: doing start-specific stuff\n");

	  scan_options = cmd[4];

	  /* 'Start' command - wait for valid status */
	  mustek_scsi_pp_timeout = 70000;
	  stat = mustek_scsi_pp_wait_for_valid_status (fd);

	  if (stat != SANE_STATUS_GOOD)
	    {
	      DBG (2,
		   "mustek_scsi_pp_cmd: error waiting for valid status after start\n");
	    }
	}
    }

  if (sanei_pa4s2_enable (fd, SANE_FALSE) != SANE_STATUS_GOOD)
    {
      DBG (2, "mustek_scsi_pp_cmd: error disabling scanner\n");
      return SANE_STATUS_IO_ERROR;
    }

  if (stat == SANE_STATUS_GOOD)
    {
      DBG (5, "mustek_scsi_pp_cmd: returning success\n");
    }

  return stat;
}

static SANE_Status
mustek_scsi_pp_rdata (int fd, int planes, SANE_Byte * buf, int lines, int bpl)
{
  int i, j;

  DBG (5,
       "mustek_scsi_pp_rdata: reading %d lines at %d bpl, %d planes from %d\n",
       lines, bpl, planes, fd);

  if ((planes != 1) && (planes != 3))
    {
      DBG (2, "mustek_scsi_pp_rdata: invalid number of planes (%d)\n",
	   planes);
      return SANE_STATUS_INVAL;
    }

  if (sanei_pa4s2_enable (fd, SANE_TRUE) != SANE_STATUS_GOOD)
    {
      DBG (2, "mustek_scsi_pp_rdata: error enabling scanner\n");
      return SANE_STATUS_IO_ERROR;
    }

  for (i = 0; i < lines; i++)
    {
      if (planes == 3)
	{
	  if (mustek_scsi_pp_wait_for_status_bit_4_toggle (fd) !=
	      SANE_STATUS_GOOD)
	    {
	      sanei_pa4s2_enable (fd, SANE_FALSE);
	      DBG (2,
		   "mustek_scsi_pp_rdata: error waiting for bit 4 toggle for red, line %d\n",
		   i);
	      return SANE_STATUS_IO_ERROR;
	    }
	  if (sanei_pa4s2_readbegin (fd, mustek_scsi_pp_register) !=
	      SANE_STATUS_GOOD)
	    {
	      sanei_pa4s2_enable (fd, SANE_FALSE);
	      DBG (2,
		   "mustek_scsi_pp_rdata: error in readbegin for red, line %d\n",
		   i);
	      return SANE_STATUS_IO_ERROR;
	    }
	  for (j = 0; j < (bpl / 3); j++)
	    {
	      if (sanei_pa4s2_readbyte (fd, &buf[j]) != SANE_STATUS_GOOD)
		{
		  sanei_pa4s2_readend (fd);
		  sanei_pa4s2_enable (fd, SANE_FALSE);
		  DBG (2,
		       "mustek_scsi_pp_rdata: error reading red byte, line %d, byte %d\n",
		       i, j);
		  return SANE_STATUS_IO_ERROR;
		}
	    }
	  if (sanei_pa4s2_readend (fd) != SANE_STATUS_GOOD)
	    {
	      sanei_pa4s2_enable (fd, SANE_FALSE);
	      DBG (2,
		   "mustek_scsi_pp_rdata: error in readend for red, line %d\n",
		   i);
	      return SANE_STATUS_IO_ERROR;
	    }


	  if (mustek_scsi_pp_wait_for_status_bit_4_toggle (fd) !=
	      SANE_STATUS_GOOD)
	    {
	      sanei_pa4s2_enable (fd, SANE_FALSE);
	      DBG (2,
		   "mustek_scsi_pp_rdata: error waiting for bit 4 toggle for green, line %d\n",
		   i);
	      return SANE_STATUS_IO_ERROR;
	    }
	  if (sanei_pa4s2_readbegin (fd, mustek_scsi_pp_register) !=
	      SANE_STATUS_GOOD)
	    {
	      sanei_pa4s2_enable (fd, SANE_FALSE);
	      DBG (2,
		   "mustek_scsi_pp_rdata: error in readbegin for green, line %d\n",
		   i);
	      return SANE_STATUS_IO_ERROR;
	    }
	  for (j = 0; j < (bpl / 3); j++)
	    {
	      if (sanei_pa4s2_readbyte (fd, &buf[j + (bpl / 3)]) !=
		  SANE_STATUS_GOOD)
		{
		  sanei_pa4s2_readend (fd);
		  sanei_pa4s2_enable (fd, SANE_FALSE);
		  DBG (2,
		       "mustek_scsi_pp_rdata: error reading green byte, line %d, byte %d\n",
		       i, j);
		  return SANE_STATUS_IO_ERROR;
		}
	    }
	  if (sanei_pa4s2_readend (fd) != SANE_STATUS_GOOD)
	    {
	      sanei_pa4s2_enable (fd, SANE_FALSE);
	      DBG (2,
		   "mustek_scsi_pp_rdata: error in readend for green, line %d\n",
		   i);
	      return SANE_STATUS_IO_ERROR;
	    }


	  if (mustek_scsi_pp_wait_for_status_bit_4_toggle (fd) !=
	      SANE_STATUS_GOOD)
	    {
	      sanei_pa4s2_enable (fd, SANE_FALSE);
	      DBG (2,
		   "mustek_scsi_pp_rdata: error waiting for bit 4 toggle for blue, line %d\n",
		   i);
	      return SANE_STATUS_IO_ERROR;
	    }
	  if (sanei_pa4s2_readbegin (fd, mustek_scsi_pp_register) !=
	      SANE_STATUS_GOOD)
	    {
	      sanei_pa4s2_enable (fd, SANE_FALSE);
	      DBG (2,
		   "mustek_scsi_pp_rdata: error in readbegin for blue, line %d\n",
		   i);
	      return SANE_STATUS_IO_ERROR;
	    }
	  for (j = 0; j < (bpl / 3); j++)
	    {
	      if (sanei_pa4s2_readbyte (fd, &buf[j + (2 * (bpl / 3))]) !=
		  SANE_STATUS_GOOD)
		{
		  sanei_pa4s2_readend (fd);
		  sanei_pa4s2_enable (fd, SANE_FALSE);
		  DBG (2,
		       "mustek_scsi_pp_rdata: error reading blue byte, line %d, byte %d\n",
		       i, j);
		  return SANE_STATUS_IO_ERROR;
		}
	    }
	  if (sanei_pa4s2_readend (fd) != SANE_STATUS_GOOD)
	    {
	      sanei_pa4s2_enable (fd, SANE_FALSE);
	      DBG (2,
		   "mustek_scsi_pp_rdata: error in readend for blue, line %d\n",
		   i);
	      return SANE_STATUS_IO_ERROR;
	    }
	}
      else
	{
	  if (mustek_scsi_pp_wait_for_status_bit_4_toggle (fd) !=
	      SANE_STATUS_GOOD)
	    {
	      sanei_pa4s2_enable (fd, SANE_FALSE);
	      DBG (2,
		   "mustek_scsi_pp_rdata: error waiting for bit 4 toggle, line %d\n",
		   i);
	      return SANE_STATUS_IO_ERROR;
	    }
	  if (sanei_pa4s2_readbegin (fd, mustek_scsi_pp_register) !=
	      SANE_STATUS_GOOD)
	    {
	      sanei_pa4s2_enable (fd, SANE_FALSE);
	      DBG (2, "mustek_scsi_pp_rdata: error in readbegin, line %d\n",
		   i);
	      return SANE_STATUS_IO_ERROR;
	    }

	  for (j = 0; j < bpl; j++)
	    {
	      if (sanei_pa4s2_readbyte (fd, &buf[j]) != SANE_STATUS_GOOD)
		{
		  sanei_pa4s2_readend (fd);
		  sanei_pa4s2_enable (fd, SANE_FALSE);
		  DBG (2,
		       "mustek_scsi_pp_rdata: error reading byte, line %d, byte %d\n",
		       i, j);
		  return SANE_STATUS_IO_ERROR;
		}
	    }

	  if (sanei_pa4s2_readend (fd) != SANE_STATUS_GOOD)
	    {
	      sanei_pa4s2_enable (fd, SANE_FALSE);
	      DBG (2, "mustek_scsi_pp_rdata: error in readend, line %d\n", i);
	      return SANE_STATUS_IO_ERROR;
	    }
	}
      buf += bpl;
    }

  if (sanei_pa4s2_enable (fd, SANE_FALSE) != SANE_STATUS_GOOD)
    {
      DBG (2, "mustek_scsi_pp_rdata: error enabling scanner\n");
      return SANE_STATUS_IO_ERROR;
    }

  DBG (5, "mustek_scsi_pp_rdata: returning success\n");
  return SANE_STATUS_GOOD;
}
