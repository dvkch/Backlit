#ifndef _HP5400_SANEI_H_
#define _HP5400_SANEI_H_

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



#define CMD_INITBULK1   0x0087	/* send 0x14 */
#define CMD_INITBULK2   0x0083	/* send 0x24 */
#define CMD_INITBULK3   0x0082	/* transfer length 0xf000 */



HP5400_SANE_STATIC void _UsbWriteControl (int fd, int iValue, int iIndex, void *pabData, int iSize);

HP5400_SANE_STATIC void hp5400_command_write_noverify (int fd, int iValue, void *pabData, int iSize);

HP5400_SANE_STATIC void _UsbReadControl (int fd, int iValue, int iIndex, void *pabData, int iSize);

HP5400_SANE_STATIC int hp5400_open (const char *filename);

HP5400_SANE_STATIC void hp5400_close (int iHandle);

/* returns value > 0 if verify ok */
HP5400_SANE_STATIC int hp5400_command_verify (int iHandle, int iCmd);

/* returns > 0 if command OK */
HP5400_SANE_STATIC int hp5400_command_read_noverify (int iHandle, int iCmd, int iLen, void *pbData);

/* returns > 0 if command OK */
HP5400_SANE_STATIC int hp5400_command_read (int iHandle, int iCmd, int iLen, void *pbData);

/* returns >0 if command OK */
HP5400_SANE_STATIC int hp5400_command_write (int iHandle, int iCmd, int iLen, void *pbData);

#ifdef STANDALONE
/* returns >0 if command OK */
HP5400_SANE_STATIC int hp5400_bulk_read (int iHandle, size_t len, int block, FILE * file);
#endif

/* returns >0 if command OK */
HP5400_SANE_STATIC int hp5400_bulk_read_block (int iHandle, int iCmd, void *cmd, int cmdlen,
			void *buffer, int len);

/* returns >0 if command OK */
HP5400_SANE_STATIC int hp5400_bulk_command_write (int iHandle, int iCmd, void *cmd, int cmdlen,
			   int datalen, int block, char *data);

/**
  ScannerIsOn
    retrieve on/off status from scanner
    @return 1 if is on 0 if is off -1 if is not reachable
*/
HP5400_SANE_STATIC int hp5400_isOn (int iHandle);

#endif
