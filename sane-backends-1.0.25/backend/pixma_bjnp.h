/* SANE - Scanner Access Now Easy.

   Copyright (C) 2008 by Louis Lagendijk
   based on sane_usb.h:
   Copyright (C) 2003, 2005 Rene Rebe (sanei_read_int,sanei_set_timeout)
   Copyright (C) 2001, 2002 Henning Meier-Geinitz

   This file is part of the SANE package.

   SANE is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   SANE is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with sane; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

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
*/
/** @file sanei_bjnp.h
 * This file provides a generic BJNP interface.  
 */

#ifndef sanei_bjnp_h
#define sanei_bjnp_h

#include "../include/sane/config.h"
#include "../include/sane/sane.h"
#include "pixma.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>		/* for size_t */
#endif

/** Initialize sanei_bjnp.
 *
 * Call this before any other sanei_bjnp function.
 */
extern void sanei_bjnp_init (void);

/** Find scanners responding to a BJNP broadcast.
 *
 * The function attach is called for every device which has been found.
 * Serial is the address of the scanner in human readable form of max
 * SHORT_HOSTNAME_MAX characters
 * @param conf_devices list of pre-configures device URI's to attach
 * @param attach attach function
 * @param pixma_devices device informatio needed by attach function
 *
 * @return SANE_STATUS_GOOD - on success (even if no scanner was found)
 */

#define SHORT_HOSTNAME_MAX 16

extern SANE_Status
sanei_bjnp_find_devices (const char **conf_devices,
                         SANE_Status (*attach_bjnp)
			 (SANE_String_Const devname,
			  SANE_String_Const makemodel,
			  SANE_String_Const serial,
			  const struct pixma_config_t *
			  const pixma_devices[]),
			 const struct pixma_config_t *const pixma_devices[]);

/** Open a BJNP device.
 *
 * The device is opened by its name devname and the device number is
 * returned in dn on success.  
 *
 * Device names consist of an URI                                     
 * Where:
 * method = bjnp
 * hostname = resolvable name or IP-address
 * port = 8612 for a bjnp scanner, 8610 for a mfnp device
 * An example could look like this: bjnp://host.domain:8612
 *
 * @param devname name of the device to open
 * @param dn device number
 *
 * @return
 * - SANE_STATUS_GOOD - on success
 * - SANE_STATUS_ACCESS_DENIED - if the file couldn't be accessed due to
 *   permissions
 * - SANE_STATUS_INVAL - on every other error
 */
extern SANE_Status sanei_bjnp_open (SANE_String_Const devname, SANE_Int * dn);

/** Close a BJNP device.
 * 
 * @param dn device number
 */

extern void sanei_bjnp_close (SANE_Int dn);

/** Activate a BJNP device connection
 *
 *  @param dn device number
 */

extern SANE_Status sanei_bjnp_activate (SANE_Int dn);

/** De-activate a BJNP device connection
 *
 * @param dn device number
 */

extern SANE_Status sanei_bjnp_deactivate (SANE_Int dn);

/** Set the libbjnp timeout for bulk and interrupt reads.
 * 
 * @param devno device number
 * @param timeout the new timeout in ms
 */
extern void sanei_bjnp_set_timeout (SANE_Int devno, SANE_Int timeout);

/** Check if sanei_bjnp_set_timeout() is available.
 */
#define HAVE_SANEI_BJNP_SET_TIMEOUT

/** Initiate a bulk transfer read.
 *
 * Read up to size bytes from the device to buffer. After the read, size
 * contains the number of bytes actually read.
 *
 * @param dn device number
 * @param buffer buffer to store read data in
 * @param size size of the data
 *
 * @return 
 * - SANE_STATUS_GOOD - on succes
 * - SANE_STATUS_EOF - if zero bytes have been read
 * - SANE_STATUS_IO_ERROR - if an error occured during the read
 * - SANE_STATUS_INVAL - on every other error
 *
 */
extern SANE_Status
sanei_bjnp_read_bulk (SANE_Int dn, SANE_Byte * buffer, size_t * size);

/** Initiate a bulk transfer write.
 *
 * Write up to size bytes from buffer to the device. After the write size
 * contains the number of bytes actually written.
 *
 * @param dn device number
 * @param buffer buffer to write to device
 * @param size size of the data
 *
 * @return 
 * - SANE_STATUS_GOOD - on succes
 * - SANE_STATUS_IO_ERROR - if an error occured during the write
 * - SANE_STATUS_INVAL - on every other error
 */
extern SANE_Status
sanei_bjnp_write_bulk (SANE_Int dn, const SANE_Byte * buffer, size_t * size);

/** Initiate a interrupt transfer read.
 *
 * Read up to size bytes from the interrupt endpoint from the device to
 * buffer. After the read, size contains the number of bytes actually read.
 *
 * @param dn device number
 * @param buffer buffer to store read data in
 * @param size size of the data
 *
 * @return 
 * - SANE_STATUS_GOOD - on succes
 * - SANE_STATUS_EOF - if zero bytes have been read
 * - SANE_STATUS_IO_ERROR - if an error occured during the read
 * - SANE_STATUS_INVAL - on every other error
 *
 */

extern SANE_Status
sanei_bjnp_read_int (SANE_Int dn, SANE_Byte * buffer, size_t * size);

/*------------------------------------------------------*/
#endif /* sanei_bjnp_h */
