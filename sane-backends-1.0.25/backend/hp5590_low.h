/* sane - Scanner Access Now Easy.
   Copyright (C) 2007 Ilia Sotnikov <hostcc@gmail.com>
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

   This file is part of a SANE backend for
   HP ScanJet 4500C/4570C/5500C/5550C/5590/7650 Scanners
*/

#ifndef HP5590_LOW_H
#define HP5590_LOW_H

#include "../include/sane/sane.h"

enum proto_flags {
  PF_NONE	 	= 0,
  PF_NO_USB_IN_USB_ACK	= 1 << 0	/* Getting acknowledge after USB-in-USB command
  					 * will be skipped */
};

/* Flags for hp5590_cmd() */
#define CMD_IN			1 << 0	/* Indicates IN direction, otherwise - OUT */
#define CMD_VERIFY		1 << 1	/* Requests last command verification */

/* Core flags for hp5590_cmd() - they indicate so called CORE commands */
#define CORE_NONE		     0	/* No CORE operation */
#define CORE_DATA		1 << 0	/* Operate on CORE data */
#define CORE_BULK_IN		1 << 1	/* CORE bulk operation - prepare for bulk IN 
					 * transfer (not used yet)
					 */
#define CORE_BULK_OUT		1 << 2	/* CORE bulk operation - prepare for bulk OUT
					 * transfer
					 */
static SANE_Status hp5590_cmd (SANE_Int dn,
			enum proto_flags proto_flags,
			unsigned int flags,
			unsigned int cmd, unsigned char *data,
			unsigned int size, unsigned int core_flags);
static SANE_Status hp5590_bulk_read (SANE_Int dn,
			      enum proto_flags proto_flags,
			      unsigned char *bytes,
			      unsigned int size, void *state);
static SANE_Status hp5590_bulk_write (SANE_Int dn,
			       enum proto_flags proto_flags,
			       int cmd,
			       unsigned char *bytes,
			       unsigned int size);
static SANE_Status hp5590_get_status (SANE_Int dn,
				      enum proto_flags proto_flags);
static SANE_Status hp5590_low_init_bulk_read_state (void **state);
static SANE_Status hp5590_low_free_bulk_read_state (void **state);
#endif /* HP5590_LOW_H */
/* vim: sw=2 ts=8
 */
