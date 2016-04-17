/* sane - Scanner Access Now Easy.

   pieusb_usb.h

   Copyright (C) 2012-2015 Jan Vleeshouwers, Michael Rickmann, Klaus Kaempf

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
   If you do not wish that, delete this exception notice.  */

#ifndef PIEUSB_USB_H
#define	PIEUSB_USB_H

#define PIEUSB_WAIT_BUSY 1 /* seconds to wait on busy condition */

#define SCSI_COMMAND_LEN 6

SANE_Status sanei_pieusb_usb_reset(SANE_Int device_number);

SANE_Status sanei_pieusb_convert_status(PIEUSB_Status status);

SANE_String sanei_pieusb_decode_sense(struct Pieusb_Sense* sense, PIEUSB_Status *status);

PIEUSB_Status sanei_pieusb_command(SANE_Int device_number, SANE_Byte command[], SANE_Byte data[], SANE_Int size);

/* =========================================================================
 *
 * Pieusb scanner commands
 *
 * ========================================================================= */

/* Standard SCSI command codes */
#define SCSI_TEST_UNIT_READY    0x00
#define SCSI_REQUEST_SENSE      0x03
#define SCSI_READ               0x08
#define SCSI_WRITE              0x0A
#define SCSI_PARAM              0x0F
#define SCSI_INQUIRY            0x12
#define SCSI_MODE_SELECT        0x15
#define SCSI_COPY               0x18
#define SCSI_MODE_SENSE         0x1A
#define SCSI_SCAN               0x1B

/* Non-standard SCSI command codes */
#define SCSI_SLIDE              0xD1
#define SCSI_SET_SCAN_HEAD      0xD2
#define SCSI_READ_GAIN_OFFSET   0xD7
#define SCSI_WRITE_GAIN_OFFSET  0xDC
#define SCSI_READ_STATE         0xDD

/* Additional SCSI READ/WRITE codes, |0x80 for Read */
#define SCSI_POWER_SAVE_CONTROL 0x01
#define SCSI_GAMMA_TABLE        0x10
#define SCSI_HALFTONE_PATTERN   0x11
#define SCSI_SCAN_FRAME         0x12
#define SCSI_EXPOSURE           0x13
#define SCSI_HIGHLIGHT_SHADOW   0x14
#define SCSI_CALIBRATION_INFO   0x15
#define SCSI_CAL_DATA           0x16
#define SCSI_CMD_17             0x17 /* used by CyberView */

/* Standard SCSI Sense keys, see http://www.t10.org/lists/2sensekey.htm */
#define SCSI_SENSE_NO_SENSE        0x00
#define SCSI_SENSE_RECOVERED_ERROR 0x01
#define SCSI_SENSE_NOT_READY       0x02
#define SCSI_SENSE_MEDIUM_ERROR    0x03
#define SCSI_SENSE_HARDWARE_ERROR  0x04
#define SCSI_SENSE_ILLEGAL_REQUEST 0x05
#define SCSI_SENSE_UNIT_ATTENTION  0x06
#define SCSI_SENSE_DATA_PROTECT    0x07
#define SCSI_SENSE_BLANK_CHECK     0x08
#define SCSI_SENSE_VENDOR_SPECIFIC 0x09
#define SCSI_SENSE_COPY_ABORTED    0x0A
#define SCSI_SENSE_ABORTED_COMMAND 0x0B
#define SCSI_SENSE_EQUAL           0x0C
#define SCSI_SENSE_VOLUME_OVERFLOW 0x0D
#define SCSI_SENSE_MISCOMPARE      0x0E
#define SCSI_SENSE_COMPLETED       0x0F

#endif	/* PIEUSB_USB_H */
