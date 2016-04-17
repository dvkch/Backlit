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

   This file implements the SCSI-over-parallel port protocol used in,
   for example, the Paragon 600 II EP
*/

#ifndef mustek_scsi_pp_h
#define mustek_scsi_pp_h

static int mustek_scsi_pp_get_time (void);

/**
 * Open the connection to a Mustek SCSI-over-pp device.
 *
 * @param dev Port address as text.
 * @param fd  Information about port address and I/O method. fd is not a file
 *            descriptor. The name and type are used for compatibility reasons.
 *
 * @return
 * - SANE_STATUS_GOOD - on success
 * - SANE_STATUS_INVAL - if the port address can't be interpreted
 * - SANE_STATUS_IO_ERROR - if the device file for a port couldn't be accessed
 */
static SANE_Status mustek_scsi_pp_open (const char *dev, int *fd);

/**
 * Close the connection to a Mustek SCSI-over-PP device.
 *
 * @param fd  Information about port address and I/O method.
 *
 */
static void mustek_scsi_pp_close (int fd);

/**
 * Exit Mustek SCSI-over-PP.
 */
static void mustek_scsi_pp_exit (void);

/**
 * Find out if the device is ready to accept new commands.
 *
 * @param fd  Information about port address and I/O method.
 *
 * @return
 * - SANE_STATUS_GOOD - if the device is ready
 * - SANE_STATUS_DEVICE_BUSY if the device is still busy (try again later)
 */
static SANE_Status mustek_scsi_pp_test_ready (int fd);

/**
 * Send a command to the Mustek SCSI-over-pp device.
 *
 * @param fd  Information about port address and I/O method.
 * @param src Data to be sent to the device.
 * @param src_size Size of data to be sent to the device.
 * @param dst Data to be received from the device.
 * @param dst_size Size of data to be received from the device
 *
 * @return
 * - SANE_STATUS_GOOD - on success
 * - SANE_STATUS_IO_ERROR - if an error occured during the dialog with the
 *   device
 */
static SANE_Status mustek_scsi_pp_cmd (int fd, const void *src, size_t src_size,
				       void *dst, size_t * dst_size);

/**
 * Read scanned image data.
 *
 * @param fd  Information about port address and I/O method.
 * @param planes Bytes per pixel (3 for color, 1 for all other modes)
 * @param buf Buffer for image data.
 * @param lines Number of lines
 * @param bpl Bytes per line
 *
 * @return
 * - SANE_STATUS_GOOD - on success
 * - SANE_STATUS_IO_ERROR - if an error occured during the dialog with the 
 *   device
 */
static SANE_Status mustek_scsi_pp_rdata (int fd, int planes,
					 SANE_Byte * buf, int lines, int bpl);


#endif /* mustek_scsi_pp_h */
