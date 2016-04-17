/* ---------------------------------------------------------------------- */

/* sane - Scanner Access Now Easy.

   umax-usb.c 

   (C) 2001-2002 Frank Zago

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

   This file implements a SANE backend for UMAX USB flatbed scanners.  */


/* ---------------------------------------------------------------------- */

#include "../include/sane/sanei_usb.h"

#include "../include/sane/sanei_pv8630.h"

/* USB specific parts */

/* Apparently this will recover from some errors. */
static void pv8630_mini_init_scanner(int fd)
{
	DBG(DBG_info, "mini_init_scanner\n");

	/* (re-)init the device (?) */
	sanei_pv8630_write_byte(fd, PV8630_UNKNOWN, 0x04 );
	sanei_pv8630_write_byte(fd, PV8630_RMODE, 0x02 );
	sanei_pv8630_write_byte(fd, PV8630_RMODE, 0x02 );

	sanei_pv8630_wait_byte(fd, PV8630_RSTATUS, 0xd0, 0xff, 1000);
}

/* Length of the CDB given the SCSI command. The last two are not
   correct (vendor reserved). */
static u_char cdb_sizes[8] = {
    6, 10, 10, 6, 16, 12, 0, 0
};
#define CDB_SIZE(opcode)	cdb_sizes[(((opcode) >> 5) & 7)]

/* Sends a CDB to the scanner. Also sends the parameters and receives
 * the data, if necessary. When this function returns with a
 * SANE_STATUS_GOOD, the SCSI command has been completed. 
 *
 * Note: I don't know about deferred commands.
 */
static SANE_Status sanei_umaxusb_cmd(int fd, const void *src, size_t src_size, void *dst, size_t * dst_size)
{
	unsigned char result;
	size_t cmd_size = CDB_SIZE (*(const char *) src);
	size_t param_size = src_size - cmd_size;
	const char * param_ptr = ((const char *) src) + cmd_size;
	size_t tmp_len;
	
	DBG(DBG_info, "Sending SCSI cmd 0x%02x cdb len %ld, param len %ld, result len %ld\n", ((const unsigned char *)src)[0], (long)cmd_size, (long)param_size, dst_size? (long)*dst_size:(long)0);

	/* This looks like some kind of pre-initialization. */
	sanei_pv8630_write_byte(fd, PV8630_UNKNOWN, 0x0c);
	sanei_pv8630_wait_byte(fd, PV8630_RSTATUS, 0xf0, 0xff, 1000);
	sanei_pv8630_write_byte(fd, PV8630_UNKNOWN, 0x04);

	/* Send the CDB and check it's been received OK. */
	sanei_pv8630_write_byte(fd, PV8630_RMODE, 0x16);
	sanei_pv8630_flush_buffer(fd);
	sanei_pv8630_prep_bulkwrite(fd, cmd_size);
	
	tmp_len = cmd_size;
	sanei_pv8630_bulkwrite(fd, src, &tmp_len);
	sanei_pv8630_wait_byte(fd, PV8630_RSTATUS, 0xf8, 0xff, 1000);

	sanei_pv8630_flush_buffer(fd);
	sanei_pv8630_prep_bulkread(fd, 1);

	result = 0xA5;				/* to be sure */
	tmp_len = 1;
	sanei_pv8630_bulkread(fd, &result, &tmp_len);
	if (result != 0) {
		DBG(DBG_info, "error in sanei_pv8630_bulkread (got %02x)\n", result);
		if (result == 8) {
			pv8630_mini_init_scanner(fd);
		}
		return(SANE_STATUS_IO_ERROR);
	}
	
	/* Send the parameters and check they've been received OK. */
	if (param_size) {
		sanei_pv8630_flush_buffer(fd);
		sanei_pv8630_prep_bulkwrite(fd, param_size);
		
		tmp_len = param_size;
		sanei_pv8630_bulkwrite(fd, param_ptr, &tmp_len);
		sanei_pv8630_wait_byte(fd, PV8630_RSTATUS, 0xf8, 0xff, 1000);
		
		sanei_pv8630_flush_buffer(fd);
		sanei_pv8630_prep_bulkread(fd, 1);
		
		result = 0xA5;				/* to be sure */
		tmp_len = 1;
		sanei_pv8630_bulkread(fd, &result, &tmp_len);
		if (result != 0) {
			DBG(DBG_info, "error in sanei_pv8630_bulkread (got %02x)\n", result);
			if (result == 8) {
				pv8630_mini_init_scanner(fd);
			}
			return(SANE_STATUS_IO_ERROR);
		}
	}

	/* If the SCSI command expect a return, get it. */
	if (dst_size != NULL && *dst_size != 0 && dst != NULL) {
		sanei_pv8630_flush_buffer(fd);
		sanei_pv8630_prep_bulkread(fd, *dst_size);
		sanei_pv8630_bulkread(fd, dst, dst_size);

		DBG(DBG_info, "  SCSI cmd returned %lu bytes\n", (u_long) *dst_size);

		sanei_pv8630_wait_byte(fd, PV8630_RSTATUS, 0xf8, 0xff, 1000);

		sanei_pv8630_flush_buffer(fd);
		sanei_pv8630_prep_bulkread(fd, 1);

		result = 0x5A;			/* just to be sure */
		tmp_len = 1;
		sanei_pv8630_bulkread(fd, &result, &tmp_len);
		if (result != 0) {
			DBG(DBG_info, "error in sanei_pv8630_bulkread (got %02x)\n", result);
			if (result == 8) {
				pv8630_mini_init_scanner(fd);
			}
			return(SANE_STATUS_IO_ERROR);
		}
	}

	sanei_pv8630_write_byte(fd, PV8630_UNKNOWN, 0x04);
	sanei_pv8630_write_byte(fd, PV8630_RMODE, 0x02);
	sanei_pv8630_write_byte(fd, PV8630_RMODE, 0x02);
	sanei_pv8630_wait_byte(fd, PV8630_RSTATUS, 0xd0, 0xff, 1000);

	DBG(DBG_info, "  SCSI command successfully executed\n");
 
	return(SANE_STATUS_GOOD);
}

/* Initialize the PowerVision 8630. */
static SANE_Status pv8630_init_umaxusb_scanner(int fd)
{
	DBG(DBG_info, "Initializing the PV8630\n");

	/* Init the device */
	sanei_pv8630_write_byte(fd, PV8630_UNKNOWN, 0x04);
	sanei_pv8630_write_byte(fd, PV8630_RMODE, 0x02);
	sanei_pv8630_write_byte(fd, PV8630_RMODE, 0x02);

	sanei_pv8630_wait_byte(fd, PV8630_RSTATUS, 0xd0, 0xff, 1000);

	sanei_pv8630_write_byte(fd, PV8630_UNKNOWN, 0x0c);
	sanei_pv8630_wait_byte(fd, PV8630_RSTATUS, 0xf0, 0xff, 1000);

	sanei_pv8630_write_byte(fd, PV8630_UNKNOWN, 0x04);
	sanei_pv8630_wait_byte(fd, PV8630_RSTATUS, 0xf0, 0xff, 1000);

	sanei_pv8630_write_byte(fd, PV8630_UNKNOWN, 0x0c);
	sanei_pv8630_wait_byte(fd, PV8630_RSTATUS, 0xf0, 0xff, 1000);
	sanei_pv8630_wait_byte(fd, PV8630_RSTATUS, 0xf8, 0xff, 1000);

	sanei_pv8630_write_byte(fd, PV8630_UNKNOWN, 0x04);

	sanei_pv8630_write_byte(fd, PV8630_RMODE, 0x02);
	sanei_pv8630_write_byte(fd, PV8630_RMODE, 0x02);
	sanei_pv8630_wait_byte(fd, PV8630_RSTATUS, 0xd0, 0xff, 1000);

	sanei_pv8630_write_byte(fd, PV8630_UNKNOWN, 0x0c);
	sanei_pv8630_wait_byte(fd, PV8630_RSTATUS, 0xf0, 0xff, 1000);

	sanei_pv8630_write_byte(fd, PV8630_UNKNOWN, 0x04);

	sanei_pv8630_write_byte(fd, PV8630_RMODE, 0x16);

	DBG(DBG_info, "PV8630 initialized\n");
	
	return(SANE_STATUS_GOOD);
}


/* 
 * SCSI functions for the emulation.
 *
 * The following functions emulate their sanei_scsi_* counterpart.
 *
 */


/* 
 * sanei_umaxusb_req_wait() and sanei_umaxusb_req_enter()
 *
 * I don't know if it is possible to queue the reads to the
 * scanner. So The queing is disabled. The performance does not seems
 * to be bad anyway.
 */

static void *umaxusb_req_buffer; /* keep the buffer ptr as an ID */

static SANE_Status sanei_umaxusb_req_enter (int fd,
											const void *src, size_t src_size,
											void *dst, size_t * dst_size, void **idp)
{
	umaxusb_req_buffer = *idp = dst;
	return(sanei_umaxusb_cmd(fd, src, src_size, dst, dst_size));
}

static SANE_Status
sanei_umaxusb_req_wait (void *id)
{
	if (id != umaxusb_req_buffer) {
		DBG(DBG_info, "sanei_umaxusb_req_wait: AIE, invalid id\n");
		return(SANE_STATUS_IO_ERROR);
	}
	return(SANE_STATUS_GOOD);
}

/* Open the device. 
 */
static SANE_Status
sanei_umaxusb_open (const char *dev, int *fdp,
					SANEI_SCSI_Sense_Handler handler, void *handler_arg)
{
	SANE_Status status;

	handler = handler;			/* silence gcc */
	handler_arg = handler_arg;	/* silence gcc */

	status = sanei_usb_open (dev, fdp);
	if (status != SANE_STATUS_GOOD) {
		DBG (1, "sanei_umaxusb_open: open of `%s' failed: %s\n",
			 dev, sane_strstatus(status));
		return status;
    } else {
		SANE_Word vendor;
		SANE_Word product;

		/* We have openned the device. Check that it is a USB scanner. */
		if (sanei_usb_get_vendor_product (*fdp, &vendor, &product) != SANE_STATUS_GOOD) {
			/* This is not a USB scanner, or SANE or the OS doesn't support it. */
			sanei_usb_close(*fdp);
			*fdp = -1;
			return SANE_STATUS_UNSUPPORTED;
		}

		/* So it's a scanner. Does this backend support it?
		 * Only the UMAX 2200 USB is currently supported. */
		if ((vendor != 0x1606) || (product != 0x0230)) {
			sanei_usb_close(*fdp);
			*fdp = -1;
			return SANE_STATUS_UNSUPPORTED;
		}
		
		/* It's a good scanner. Initialize it.  
		 *
		 * Note: pv8630_init_umaxusb_scanner() is for the UMAX
		 * 2200. Other UMAX scanner might need a different
		 * initialization routine. */

		pv8630_init_umaxusb_scanner(*fdp); 
	}
	
	return(SANE_STATUS_GOOD);
}

/* sanei_umaxusb_open_extended() is just a passthrough for sanei_umaxusb_open(). */
static SANE_Status
sanei_umaxusb_open_extended (const char *dev, int *fdp,
					SANEI_SCSI_Sense_Handler handler, void *handler_arg, int *buffersize)
{
	buffersize = buffersize;
	return(sanei_umaxusb_open(dev, fdp, handler, handler_arg));
}

/* Close the scanner. */
static void
sanei_umaxusb_close (int fd)
{
	sanei_usb_close(fd);
}




