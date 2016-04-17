/* sane - Scanner Access Now Easy.
   Copyright (C) 2001, 2002 Henning Meier-Geinitz
   Copyright (C) 2003, 2005 Rene Rebe (sanei_read_int,sanei_set_timeout)
   Copyright (C) 2008 m. allan noah (sanei_usb_clear_halt)
   Copyright (C) 2011 Reinhold Kainhofer (sanei_usb_set_endpoint)
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

/** @file sanei_usb.h
 * This file provides a generic USB interface.
 *
 * Currently, two access methods to USB devices are provided:
 * - Access to device
 * files as used by the Linux kernel USB scanner driver is supported. FreeBSD
 * and OpenBSD with their uscanner drivers also work this way. However,
 * detection and control messages aren't supported on these platforms.
 * - Access using libusb (where available).
 *
 * A general remark: Do not mix sanei_usb functions with "normal" file-related
 * libc functions like open() or close.  The device numbers used in sanei_usb
 * are not file descriptors.
 *
 * @sa sanei_lm983x.h, sanei_pa4s2.h, sanei_pio.h, sanei_scsi.h, and <a
 * href="http://www.sane-project.org/man/sane-usb.5.html">man sane-usb(5)</a>
 * for user-oriented documentation
 */

#ifndef sanei_usb_h
#define sanei_usb_h

#include "../include/sane/config.h"
#include "../include/sane/sane.h"

#include <stdlib.h> /* for size_t */

/* USB spec defines */
#ifndef USB_CLASS_PER_INTERFACE
/* Also defined in libusb */
/** @name Device and/or interface class codes */
/* @{ */
#define USB_CLASS_PER_INTERFACE         0x00
#define USB_CLASS_AUDIO                 0x01
#define USB_CLASS_COMM                  0x02
#define USB_CLASS_HID                   0x03
#define USB_CLASS_PRINTER               0x07
#define USB_CLASS_MASS_STORAGE          0x08
#define USB_CLASS_HUB                   0x09
#define USB_CLASS_DATA                  0x0a
#define USB_CLASS_VENDOR_SPEC           0xff
/* @} */

/** @name USB descriptor types */
/* @{ */
#define USB_DT_DEVICE                   0x01
#define USB_DT_CONFIG                   0x02
#define USB_DT_STRING                   0x03
#define USB_DT_INTERFACE                0x04
#define USB_DT_ENDPOINT                 0x05
#define USB_DT_HID                      0x21
#define USB_DT_REPORT                   0x22
#define USB_DT_PHYSICAL                 0x23
#define USB_DT_HUB                      0x29
/* @} */

/** @name Descriptor sizes per descriptor type */
/* @{ */
#define USB_DT_DEVICE_SIZE              18
#define USB_DT_CONFIG_SIZE              9
#define USB_DT_INTERFACE_SIZE           9
#define USB_DT_ENDPOINT_SIZE            7
#define USB_DT_ENDPOINT_AUDIO_SIZE      9
#define USB_DT_HUB_NONVAR_SIZE          7
/* @} */

/** @name Endpoint descriptors */
/* @{ */
#define USB_ENDPOINT_ADDRESS_MASK       0x0f
#define USB_ENDPOINT_DIR_MASK           0x80
#define USB_ENDPOINT_TYPE_MASK          0x03
#define USB_ENDPOINT_TYPE_CONTROL       0
#define USB_ENDPOINT_TYPE_ISOCHRONOUS   1
#define USB_ENDPOINT_TYPE_BULK          2
#define USB_ENDPOINT_TYPE_INTERRUPT     3
/* @} */

/** @name Standard requests */
/* @{ */
#define USB_REQ_GET_STATUS              0x00
#define USB_REQ_CLEAR_FEATURE           0x01
#define USB_REQ_SET_FEATURE             0x03
#define USB_REQ_SET_ADDRESS             0x05
#define USB_REQ_GET_DESCRIPTOR          0x06
#define USB_REQ_SET_DESCRIPTOR          0x07
#define USB_REQ_GET_CONFIGURATION       0x08
#define USB_REQ_SET_CONFIGURATION       0x09
#define USB_REQ_GET_INTERFACE           0x0A
#define USB_REQ_SET_INTERFACE           0x0B
#define USB_REQ_SYNCH_FRAME             0x0C
/* @} */

/** @name USB types */
/* @{ */
#define USB_TYPE_STANDARD               (0x00 << 5)
#define USB_TYPE_CLASS                  (0x01 << 5)
#define USB_TYPE_VENDOR                 (0x02 << 5)
#define USB_TYPE_RESERVED               (0x03 << 5)
/* @} */

/** @name USB recipients */
/* @{ */
#define USB_RECIP_DEVICE                0x00
#define USB_RECIP_INTERFACE             0x01
#define USB_RECIP_ENDPOINT              0x02
#define USB_RECIP_OTHER                 0x03
/* @} */

#endif /* not USB_CLASS_PER_INTERFACE */

/* Not defined in libsub */
/** @name USB Masks */
/* @{ */
#define USB_TYPE_MASK                   (0x03 << 5)
#define USB_RECIP_MASK                  0x1f
/* @} */

/** @name USB directions */
/* @{ */
#define USB_DIR_OUT                     0x00
#define USB_DIR_IN                      0x80
/* @} */

/** */
struct sanei_usb_dev_descriptor
{
	SANE_Byte    desc_type;
	unsigned int bcd_usb;
	unsigned int bcd_dev;
	SANE_Byte    dev_class;
	SANE_Byte    dev_sub_class;
	SANE_Byte    dev_protocol;
	SANE_Byte    max_packet_size;
};

/** Initialize sanei_usb.
 *
 * Call this before any other sanei_usb function.
 */
extern void sanei_usb_init (void);

/** End sanei_usb use, freeing resources when needed.
 *
 * When the use count of sanei_usb reach 0, free resources and end
 * sanei_usb use.
 */
extern void sanei_usb_exit (void);

/** Search for USB devices.
 *
 * Search USB busses for scanner devices.
 */
extern void sanei_usb_scan_devices (void);

/** Get the vendor and product ids by device name.
 *
 * @param devname
 * @param vendor vendor id
 * @param product product id
 *
 * @return
 * - SANE_STATUS_GOOD - if the ids could be determined
 * - SANE_STATUS_INVAL - if the device is not found
 * - SANE_STATUS_UNSUPPORTED - if this method is not supported with the current
 *   access method
 */
SANE_Status
sanei_usb_get_vendor_product_byname (SANE_String_Const devname,
				     SANE_Word * vendor, SANE_Word * product);

/** Get the vendor and product ids.
 *
 * Currently, only libusb devices and scanners supported by the Linux USB
 * scanner module can be found.  For the latter method, the Linux version
 * must be 2.4.8 or higher.
 *
 * @param dn device number of an already sanei_usb_opened device
 * @param vendor vendor id
 * @param product product id
 *
 * @return
 * - SANE_STATUS_GOOD - if the ids could be determined
 * - SANE_STATUS_UNSUPPORTED - if the OS doesn't support detection of ids
 */
extern SANE_Status
sanei_usb_get_vendor_product (SANE_Int dn, SANE_Word * vendor,
			      SANE_Word * product);

/** Find devices that match given vendor and product ids.
 *
 * For limitations, see function sanei_usb_get_vendor_product().
 * The function attach is called for every device which has been found.
 *
 * @param vendor vendor id
 * @param product product id
 * @param attach attach function
 *
 * @return SANE_STATUS_GOOD - on success (even if no scanner was found)
 */
extern SANE_Status
sanei_usb_find_devices (SANE_Int vendor, SANE_Int product,
			SANE_Status (*attach) (SANE_String_Const devname));

/** Open a USB device.
 *
 * The device is opened by its name devname and the device number is
 * returned in dn on success.
 *
 * Device names can be either device file names for direct access over
 * kernel drivers (like /dev/usb/scanner) or libusb names. The libusb format
 * looks like this: "libusb:bus-id:device-id". Bus-id and device-id are
 * platform-dependent. An example could look like this: "libusb:001:002"
 * (Linux).
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
extern SANE_Status sanei_usb_open (SANE_String_Const devname, SANE_Int * dn);

/** Set the endpoint for the USB communication
 *
 * Allows to switch to a different endpoint for the USB communication than
 * the default (auto-detected) endpoint. This function can only be called
 * after sanei_usb_open.
 *
 * @param dn device number
 * @param ep_type type of endpoint to set (bitwise or of USB_DIR_IN/OUT and
 *                USB_ENDPOINT_TYPE_BULK/CONTROL/INTERRUPT/ISOCHRONOUS
 * @param ep endpoint to use for the given type
 *
 */
extern void sanei_usb_set_endpoint (SANE_Int dn, SANE_Int ep_type, SANE_Int ep);

/** Retrieve the endpoint used for the USB communication
 *
 * Returns the endpoint used for the USB communication of the given type.
 * This function can only be called after sanei_usb_open.
 *
 * @param dn device number
 * @param ep_type type of endpoint to retrieve (bitwise or of USB_DIR_IN/OUT
 *                and USB_ENDPOINT_TYPE_BULK/CONTROL/INTERRUPT/ISOCHRONOUS
 * @return endpoint used for the given type
 *
 */
extern SANE_Int sanei_usb_get_endpoint (SANE_Int dn, SANE_Int ep_type);

/** Close a USB device.
 *
 * @param dn device number
 */
extern void sanei_usb_close (SANE_Int dn);

/** Set the libusb timeout for bulk and interrupt reads.
 *
 * @param timeout the new timeout in ms
 */
extern void sanei_usb_set_timeout (SANE_Int timeout);

/** Check if sanei_usb_set_timeout() is available.
 */
#define HAVE_SANEI_USB_SET_TIMEOUT

/** Clear halt condition on bulk endpoints
 *
 * @param dn device number
 */
extern SANE_Status sanei_usb_clear_halt (SANE_Int dn);

/** Check if sanei_usb_clear_halt() is available.
 */
#define HAVE_SANEI_USB_CLEAR_HALT

/** Reset device
 *
 * @param dn device number
 */
extern SANE_Status sanei_usb_reset (SANE_Int dn);

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
sanei_usb_read_bulk (SANE_Int dn, SANE_Byte * buffer, size_t * size);

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
sanei_usb_write_bulk (SANE_Int dn, const SANE_Byte * buffer, size_t * size);

/** Send/receive a control message to/from a USB device.
 *
 * This function is only supported for libusb devices and kernel acces with
 * Linux 2.4.13 and newer.
 * For a detailed explanation of the parameters, have a look at the USB
 * specification at the <a href="http://www.usb.org/developers/docs/">
 * www.usb.org developers information page</a>.
 *
 * @param dn device number
 * @param rtype specifies the characteristics of the request (e.g. data
 *    direction)
 * @param req actual request
 * @param value parameter specific to the request
 * @param index parameter specific to the request (often used to select
 *     endpoint)
 * @param len length of data to send/receive
 * @param data buffer to send/receive data
 *
 * @return
 * - SANE_STATUS_GOOD - on success
 * - SANE_STATUS_IO_ERROR - on error
 * - SANE_STATUS_UNSUPPORTED - if the feature is not supported by the OS or
 *   SANE.
 */
extern SANE_Status
sanei_usb_control_msg (SANE_Int dn, SANE_Int rtype, SANE_Int req,
		       SANE_Int value, SANE_Int index, SANE_Int len,
		       SANE_Byte * data);

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
sanei_usb_read_int (SANE_Int dn, SANE_Byte * buffer, size_t * size);

/** Expand device name patterns into a list of devices.
 *
 * Apart from a normal device name (such as /dev/usb/scanner0 or
 * libusb:002:003), this function currently supports USB device
 * specifications of the form:
 *
 *	usb VENDOR PRODUCT
 *
 * VENDOR and PRODUCT are non-negative integer numbers in decimal or
 * hexadecimal format. A similar function for SCSI devices can be found
 * in include/sane/config.h.
 *
 * @param name device name pattern
 * @param attach attach function
 *
 */
extern void
sanei_usb_attach_matching_devices (const char *name,
				   SANE_Status (*attach) (const char *dev));

/** Initiate set configuration.
 *
 * Change set configuration
 *
 * @param dn device number
 * @param configuration, configuration nummber
 *
 * @return
 * - SANE_STATUS_GOOD - on succes
 * - SANE_STATUS_EOF - if zero bytes have been read
 * - SANE_STATUS_IO_ERROR - if an error occured during the read
 * - SANE_STATUS_INVAL - on every other error
 *
 */

extern SANE_Status
sanei_usb_set_configuration (SANE_Int dn, SANE_Int configuration);

/** Initiate claim interface.
 *
 * Change claim interface
 *
 * @param dn device number
 * @param interface_number interface number
 *
 * @return
 * - SANE_STATUS_GOOD - on succes
 * - SANE_STATUS_EOF - if zero bytes have been read
 * - SANE_STATUS_IO_ERROR - if an error occured during the read
 * - SANE_STATUS_INVAL - on every other error
 *
 */

extern SANE_Status
sanei_usb_claim_interface (SANE_Int dn, SANE_Int interface_number);

/** Initiate release interface.
 *
 * Change release interface
 *
 * @param dn device number
 * @param interface_number interface number
 *
 * @return
 * - SANE_STATUS_GOOD - on succes
 * - SANE_STATUS_EOF - if zero bytes have been read
 * - SANE_STATUS_IO_ERROR - if an error occured during the read
 * - SANE_STATUS_INVAL - on every other error
 *
 */

extern SANE_Status
sanei_usb_release_interface (SANE_Int dn, SANE_Int interface_number);

/** Initiate a set altinterface.
 *
 * Change set alternate
 *
 * @param dn device number
 * @param alternate, alternate nummber
 *
 * @return
 * - SANE_STATUS_GOOD - on succes
 * - SANE_STATUS_EOF - if zero bytes have been read
 * - SANE_STATUS_IO_ERROR - if an error occured during the read
 * - SANE_STATUS_INVAL - on every other error
 *
 */

extern SANE_Status
sanei_usb_set_altinterface (SANE_Int dn, SANE_Int alternate);

/** Get some information from the device descriptor
 *
 * Sometimes it's useful to know something about revisions and
 * other stuff reported by the USB system
 *
 * @param dn device number
 * @param desc where to put the information to
 *
 * @return
 * - SANE_STATUS_GOOD - on succes
 * - SANE_STATUS_UNSUPPORTED - if the feature is not supported by the OS or
 *   SANE.
 * - SANE_STATUS_INVAL - on every other error
 *
 */

extern SANE_Status
sanei_usb_get_descriptor( SANE_Int dn, struct sanei_usb_dev_descriptor *desc );

/*------------------------------------------------------*/
#endif /* sanei_usb_h */
