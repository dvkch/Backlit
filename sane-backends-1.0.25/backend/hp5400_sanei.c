/* sane - Scanner Access Now Easy.
   Copyright (C) 2003 Martijn van Oosterhout <kleptog@svana.org>
   Copyright (C) 2003 Thomas Soumarmon <thomas.soumarmon@cogitae.net>
   Copyright (c) 2003 Henning Meier-Geinitz, <henning@meier-geinitz.de>

   Originally copied from HP3300 testtools. Original notice follows:

   Copyright (C) 2001 Bertrik Sikken (bertrik@zonnet.nl)

   This file is part of the SANE package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

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

   Transport layer for communication with HP5400/5470 scanner.

   Implementation using sanei_usb

   Additions to support bulk data transport. Added debugging info - 19/02/2003 Martijn
   Changed to use sanei_usb instead of direct /dev/usb/scanner access - 15/04/2003 Henning
*/


#include "hp5400_xfer.h"
#include "hp5400_debug.h"
#include <stdio.h>
#include "../include/sane/sanei_usb.h"

#define CMD_INITBULK1   0x0087	/* send 0x14 */
#define CMD_INITBULK2   0x0083	/* send 0x24 */
#define CMD_INITBULK3   0x0082	/* transfer length 0xf000 */



static void
_UsbWriteControl (int fd, int iValue, int iIndex, void *pabData, int iSize)
{
  int requesttype = USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_DIR_OUT;
  int request = (iSize > 1) ? 0x04 : 0x0C;

  HP5400_DBG (DBG_MSG,
       "Write: reqtype = 0x%02X, req = 0x%02X, value = %04X, len = %d\n",
       requesttype, request, iValue, iSize);

  if (iSize > 0)
    {
      int i;
      HP5400_DBG (DBG_MSG, "  Data: ");
      for (i = 0; i < iSize && i < 8; i++)
	HP5400_DBG (DBG_MSG, "%02X ", ((unsigned char *) pabData)[i]);
      if (iSize > 8)
	HP5400_DBG (DBG_MSG, "...");
      HP5400_DBG (DBG_MSG, "\n");
    }

  if (fd != -1)
    {
      sanei_usb_control_msg (fd, requesttype, request, iValue, iIndex, iSize,
			     pabData);
    }
  /* No error checking? */

}

void
hp5400_command_write_noverify (int fd, int iValue, void *pabData, int iSize)
{
  _UsbWriteControl (fd, iValue, 0, pabData, iSize);
}

static void
_UsbReadControl (int fd, int iValue, int iIndex, void *pabData, int iSize)
{
  int requesttype = USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_DIR_IN;
  int request = (iSize > 1) ? 0x04 : 0x0C;

  HP5400_DBG (DBG_MSG, "Read: reqtype = 0x%02X, req = 0x%02X, value = %04X\n",
       requesttype, request, iValue);

  if (fd != -1)
    {
      sanei_usb_control_msg (fd, requesttype, request, iValue, iIndex, iSize,
			     pabData);
    }
}


HP5400_SANE_STATIC int
hp5400_open (const char *filename)
{
  int fd, iVendor, iProduct;
  SANE_Status status;

  if (!filename)
    filename = "/dev/usb/scanner0";

  status = sanei_usb_open (filename, &fd);
  if (status != SANE_STATUS_GOOD)
    {
      HP5400_DBG (DBG_MSG, "hp5400_open: open returned %s\n",
	   sane_strstatus (status));
      return -1;
    }
 
  status = sanei_usb_get_vendor_product (fd, &iVendor, &iProduct);
  if (status != SANE_STATUS_GOOD)
    {
      HP5400_DBG (DBG_MSG, "hp5400_open: can't get vendor/product ids: %s\n",
	   sane_strstatus (status));
      sanei_usb_close (fd);
      return -1;
    }

  if ((iVendor != 0x03F0) || ((iProduct != 0x1005) && (iProduct != 0x1105)))
    {
      HP5400_DBG (DBG_MSG, "hp5400_open: vendor/product 0x%04X-0x%04X is not "
	   "supported\n", iVendor, iProduct);
      sanei_usb_close (fd);
      return -1;
    }

  HP5400_DBG (DBG_MSG, "vendor/product 0x%04X-0x%04X opened\n", iVendor, iProduct);

  return fd;
}


void
hp5400_close (int iHandle)
{
  sanei_usb_close (iHandle);
}


/* returns value > 0 if verify ok */
int
hp5400_command_verify (int iHandle, int iCmd)
{
  unsigned char abData[4];
  int fd;

  if (iHandle < 0)
    {
      HP5400_DBG (DBG_ERR, "hp5400_command_verify: invalid handle\n");
      return -1;
    }
  fd = iHandle;

  /* command 0xc500: read back previous command */
  _UsbReadControl (fd, 0xc500, 0, (char *) abData, 2);

  if (abData[0] != (iCmd >> 8))
    {
      HP5400_DBG (DBG_ERR,
	   "hp5400_command_verify failed, expected 0x%02X%02X, got 0x%02X%02X\n",
	   (int) (iCmd >> 8), (int) (iCmd & 0xff), (int) abData[0],
	   (int) abData[1]);

      return -1;
    }

  if (abData[1] != 0)		/* Error code non-zero */
    {
      _UsbReadControl (fd, 0x0300, 0, (char *) abData, 3);
      HP5400_DBG (DBG_ERR, "  error response is: %02X %02X %02X\n", abData[0],
	   abData[1], abData[2]);

      return -1;
    }

  HP5400_DBG (DBG_MSG, "Command %02X verified\n", abData[0]);
  return 1;
}


/* returns > 0 if command OK */
int
hp5400_command_read_noverify (int iHandle, int iCmd, int iLen, void *pbData)
{
  int fd;

  if (iHandle < 0)
    {
      HP5400_DBG (DBG_ERR, "hp5400_command_read: invalid handle\n");
      return -1;
    }
  fd = iHandle;

  _UsbReadControl (fd, iCmd, 0, pbData, iLen);

  return 1;
}

/* returns > 0 if command OK */
int
hp5400_command_read (int iHandle, int iCmd, int iLen, void *pbData)
{
  hp5400_command_read_noverify (iHandle, iCmd, iLen, pbData);

  return hp5400_command_verify (iHandle, iCmd);
}


/* returns >0 if command OK */
int
hp5400_command_write (int iHandle, int iCmd, int iLen, void *pbData)
{
  int fd;

  if (iHandle < 0)
    {
      HP5400_DBG (DBG_ERR, "hp5400_command_write: invalid handle\n");
      return -1;
    }
  fd = iHandle;

  _UsbWriteControl (fd, iCmd, 0, (char *) pbData, iLen);

  return hp5400_command_verify (iHandle, iCmd);
}

#ifdef STANDALONE
/* returns >0 if command OK */
int
hp5400_bulk_read (int iHandle, size_t len, int block, FILE * file)
{
  SANE_Int fd;
  char x1 = 0x14, x2 = 0x24;
  short buf[4] = { 0, 0, 0, 0 };
  SANE_Byte *buffer;
  size_t res = 0;

  buf[2] = block;

  if (iHandle < 0)
    {
      HP5400_DBG (DBG_ERR, "hp5400_command_read: invalid handle\n");
      return -1;
    }
  fd = iHandle;

  buffer = (unsigned char*) malloc (block);

  _UsbWriteControl (fd, CMD_INITBULK1, 0, &x1, 1);
  _UsbWriteControl (fd, CMD_INITBULK2, 0, &x2, 1);

  while (len > 0)
    {
      _UsbWriteControl (fd, CMD_INITBULK3, 0, (unsigned char *) &buf,
			sizeof (buf));
      res = block;
      sanei_usb_read_bulk (fd, buffer, &res);
      HP5400_DBG (DBG_MSG, "Read bulk returned %lu, %lu remain\n",
		  (u_long) res, (u_long) len);
      if (res > 0)
	{
	  fwrite (buffer, (len < res) ? len : res, 1, file);
	}
      len -= block;
    }
  /* error handling? */
  return 0;
}
#endif

/* returns >0 if command OK */
int
hp5400_bulk_read_block (int iHandle, int iCmd, void *cmd, int cmdlen,
			void *buffer, int len)
{
  SANE_Int fd;
  size_t res = 0;

  if (iHandle < 0)
    {
      HP5400_DBG (DBG_ERR, "hp5400_command_read_block: invalid handle\n");
      return -1;
    }
  fd = iHandle;

  _UsbWriteControl (fd, iCmd, 0, cmd, cmdlen);
  res = len;
  sanei_usb_read_bulk (fd, (SANE_Byte *) buffer, &res);
  HP5400_DBG (DBG_MSG, "Read block returned %lu when reading %d\n", 
	      (u_long) res, len);
  return res;
}

/* returns >0 if command OK */
int
hp5400_bulk_command_write (int iHandle, int iCmd, void *cmd, int cmdlen,
			   int datalen, int block, char *data)
{
  SANE_Int fd;
  size_t res = 0, offset = 0;

  if (iHandle < 0)
    {
      HP5400_DBG (DBG_ERR, "hp5400_bulk_command_write: invalid handle\n");
      return -1;
    }
  fd = iHandle;

  HP5400_DBG (DBG_MSG, "bulk_command_write(%04X,<%d bytes>,<%d bytes>)\n", iCmd,
       cmdlen, datalen);

  _UsbWriteControl (fd, iCmd, 0, cmd, cmdlen);

  while (datalen > 0)
    {
      {
	int i;
	HP5400_DBG (DBG_MSG, "  Data: ");
	for (i = 0; i < datalen && i < block && i < 8; i++)
	  HP5400_DBG (DBG_MSG, "%02X ", ((unsigned char *) data + offset)[i]);
	if (i >= 8)
	  HP5400_DBG (DBG_MSG, "...");
	HP5400_DBG (DBG_MSG, "\n");
      }
      res = (datalen < block) ? datalen : block;
      sanei_usb_write_bulk (fd, (SANE_Byte *) (data + offset), &res);
      HP5400_DBG (DBG_MSG, "Write returned %lu, %d remain\n", (u_long) res, datalen);
      datalen -= block;
      offset += block;
    }

  return hp5400_command_verify (iHandle, iCmd);
}

#ifdef STANDALONE
/**
  ScannerIsOn
    retrieve on/off status from scanner
    @return 1 if is on 0 if is off -1 if is not reachable
*/
int
hp5400_isOn (int iHandle)
{
  unsigned char text2400[3];

  hp5400_command_read (iHandle, 0x2400, 0x03, text2400);

  /* byte 0 indicates if is on or off if 0x02 */
  /* byte 1 indicates time since is on */
  /* byte 2 indicates time since is power plugged */

  if (text2400[0] & 0x02)
    {
      return 1;			/* is on */
    }

  return 0;			/* is off */
}
#endif

