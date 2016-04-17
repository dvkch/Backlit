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

#include "../include/sane/config.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */

#include "byteorder.h"

#include "../include/sane/sanei_debug.h"
#include "../include/sane/sanei_usb.h"
#include "../include/_stdint.h"
#include "hp5590_low.h"

/* Debug levels */
#define DBG_err		0
#define	DBG_proc	10
#define DBG_usb		50

/* Custom assert() macro */
#define hp5590_low_assert(exp) if(!(exp)) { \
  DBG (DBG_err, "Assertion '%s' failed at %s:%u\n", #exp, __FILE__, __LINE__);\
  return SANE_STATUS_INVAL; \
}

/* Structure describing bulk transfer size */
struct bulk_size
{
  uint16_t	size;
  uint8_t 	unused;
} __attribute__ ((packed));

/* Structure describing bulk URB */
/* FIXME: Verify according to USB standard */
struct usb_in_usb_bulk_setup
{
  uint8_t	bRequestType;
  uint8_t	bRequest;
  uint8_t	bEndpoint;
  uint16_t	unknown;
  uint16_t	wLength;	/* MSB first */
  uint8_t 	pad;
} __attribute__ ((packed));

/* Structure describing control URB */
struct usb_in_usb_ctrl_setup {
  uint8_t  bRequestType;
  uint8_t  bRequest;
  uint16_t wValue;		/* MSB first */
  uint16_t wIndex;		/* MSB first */
  uint16_t wLength;		/* LSB first */
} __attribute__ ((packed));

/* CORE status flag - ready or not */
#define CORE_FLAG_NOT_READY		1 << 1

/* Bulk transfers are done in pages, below their respective sizes */
#define BULK_WRITE_PAGE_SIZE		0x0f000
#define BULK_READ_PAGE_SIZE		0x10000
#define ALLOCATE_BULK_READ_PAGES	16	/* 16 * 65536 = 1Mb */

/* Structure describing bulk read state, because bulk reads will be done in
 * pages, but function caller uses its own buffer, whose size is certainly
 * different. Also, each bulk read page is ACK'ed by special command
 * so total pages received should be tracked as well
 */
struct bulk_read_state
{
  unsigned char	*buffer;
  unsigned int	buffer_size;
  unsigned int 	bytes_available;
  unsigned char *buffer_out_ptr;
  unsigned char *buffer_in_ptr;
  unsigned int 	total_pages;
  unsigned char *buffer_end_ptr;
  unsigned int 	initialized;
};

/*******************************************************************************
 * USB-in-USB: get acknowledge for last USB-in-USB operation
 *
 * Parameters
 * dn - sanei_usb device descriptor
 *
 * Returns
 * SANE_STATUS_GOOD - if correct acknowledge was received
 * SANE_STATUS_DEVICE_BUSY - otherwise
 */
static SANE_Status
hp5590_get_ack (SANE_Int dn,
		enum proto_flags proto_flags)
{
  uint8_t 	status;
  SANE_Status	ret;

  /* Bypass reading acknowledge if the device doesn't need it */
  if (proto_flags & PF_NO_USB_IN_USB_ACK)
    return SANE_STATUS_GOOD;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  /* Check if USB-in-USB operation was accepted */
  ret = sanei_usb_control_msg (dn, USB_DIR_IN | USB_TYPE_VENDOR,
			       0x0c, 0x8e, 0x20,
			       sizeof (status), &status);
  if (ret != SANE_STATUS_GOOD)
    {
      DBG (DBG_err, "%s: USB-in-USB: error getting acknowledge\n",
	   __FUNCTION__);
      return ret;
    }

  DBG (DBG_usb, "%s: USB-in-USB: accepted\n", __FUNCTION__);

  /* Check if we received correct acknowledgement */
  if (status != 0x01)
    {
      DBG (DBG_err, "%s: USB-in-USB: not accepted (status %u)\n",
	   __FUNCTION__, status);
      return SANE_STATUS_DEVICE_BUSY;
    }

  return SANE_STATUS_GOOD;
}

/*******************************************************************************
 * USB-in-USB: get device status
 *
 * Parameters
 * dn - sanei_usb device descriptor
 *
 * Returns
 * SANE_STATUS_GOOD - if correct status was received
 * SANE_STATUS_DEVICE_BUSY - otherwise
 */
static SANE_Status
hp5590_get_status (SANE_Int dn,
		   __sane_unused__ enum proto_flags proto_flags)
{
  uint8_t status;
  SANE_Status ret;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  ret = sanei_usb_control_msg (dn, USB_DIR_IN | USB_TYPE_VENDOR,
			       0x0c, 0x8e, 0x00,
			       sizeof (status), &status);
  if (ret != SANE_STATUS_GOOD)
    {
      DBG (DBG_err, "%s: USB-in-USB: error getting device status\n",
	   __FUNCTION__);
      return ret;
    }

  /* Check if we received correct status */
  if (status != 0x00)
    {
      DBG (DBG_err, "%s: USB-in-USB: got non-zero device status (status %u)\n",
	   __FUNCTION__, status);
      return SANE_STATUS_DEVICE_BUSY;
    }

  return SANE_STATUS_GOOD;
}

/*******************************************************************************
 * USB-in-USB: sends control message for IN or OUT operation
 *
 * Parameters
 * dn - sanei_usb device descriptor
 * requesttype, request, value, index - their meaninings are similar to
 * sanei_control_msg()
 * bytes - pointer to data buffer
 * size - size of data
 * core_flags - 
 *  CORE_NONE - no CORE operation will be performed
 *  CORE_DATA - operation on CORE data will be performed
 *  CORE_BULK_IN - preparation for bulk IN transfer (not used yet)
 *  CORE_BULK_OUT - preparation for bulk OUT trasfer
 *
 * Returns
 * SANE_STATUS_GOOD - control message was sent w/o any errors
 * all other SANE_Status values - otherwise
 */
static SANE_Status
hp5590_control_msg (SANE_Int dn,
		    enum proto_flags proto_flags,
		    int requesttype, int request,
		    int value, int index, unsigned char *bytes,
		    int size, int core_flags)
{
  struct usb_in_usb_ctrl_setup	ctrl;
  SANE_Status 			ret;
  unsigned int 			len;
  unsigned char 		*ptr;
  uint8_t 			ack;
  uint8_t 			response;
  unsigned int 			needed_response;

  DBG (DBG_proc, "%s: USB-in-USB: core data: %s\n",
       __FUNCTION__, core_flags & CORE_DATA ? "yes" : "no");

  hp5590_low_assert (bytes != NULL);

  /* IN (read) operation will be performed */
  if (requesttype & USB_DIR_IN)
    {
      /* Prepare USB-in-USB control message */
      memset (&ctrl, 0, sizeof (ctrl));
      ctrl.bRequestType = 0xc0;
      ctrl.bRequest = request;
      ctrl.wValue = htons (value);
      ctrl.wIndex = htons (index);
      ctrl.wLength = htole16 (size);

      DBG (DBG_usb, "%s: USB-in-USB: sending control msg\n", __FUNCTION__);
      /* Send USB-in-USB control message */
      ret = sanei_usb_control_msg (dn, USB_DIR_OUT | USB_TYPE_VENDOR,
				   0x04, 0x8f, 0x00,
				   sizeof (ctrl), (unsigned char *) &ctrl);
      if (ret != SANE_STATUS_GOOD)
	{
	  DBG (DBG_err, "%s: USB-in-USB: error sending control message\n",
	       __FUNCTION__);
	  return ret;
	}

      /* USB-in-USB: checking acknowledge for control message */
      ret = hp5590_get_ack (dn, proto_flags);
      if (ret != SANE_STATUS_GOOD)
	return ret;

      len = size;
      ptr = bytes;
      /* Data is read in 8 byte portions */
      while (len)
	{
	  unsigned int next_packet_size;
	  next_packet_size = 8;
	  if (len < 8)
	    next_packet_size = len;

	  /* Read USB-in-USB data */
	  ret = sanei_usb_control_msg (dn, USB_DIR_IN | USB_TYPE_VENDOR,
				       core_flags & CORE_DATA ? 0x0c : 0x04,
				       0x90, 0x00, next_packet_size, ptr);
	  if (ret != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_err, "%s: USB-in-USB: error reading data\n", __FUNCTION__);
	      return ret;
	    }

	  ptr += next_packet_size;
	  len -= next_packet_size;
	}

      /* Confirm data reception */
      ack = 0;
      ret = sanei_usb_control_msg (dn, USB_DIR_OUT | USB_TYPE_VENDOR,
				   0x0c, 0x8f, 0x00,
				   sizeof (ack), &ack);
      if (ret != SANE_STATUS_GOOD)
	{
	  DBG (DBG_err, "%s: USB-in-USB: error confirming data reception\n",
	       __FUNCTION__);
	  return -1;
	}

      /* USB-in-USB: checking if confirmation was acknowledged */
      ret = hp5590_get_ack (dn, proto_flags);
      if (ret != SANE_STATUS_GOOD)
	return ret;
    }

  /* OUT (write) operation will be performed */
  if (!(requesttype & USB_DIR_IN))
    {
      /* Prepare USB-in-USB control message */
      memset (&ctrl, 0, sizeof (ctrl));
      ctrl.bRequestType = 0x40;
      ctrl.bRequest = request;
      ctrl.wValue = htons (value);
      ctrl.wIndex = htons (index);
      ctrl.wLength = htole16 (size);

      DBG (DBG_usb, "%s: USB-in-USB: sending control msg\n", __FUNCTION__);
      /* Send USB-in-USB control message */
      ret = sanei_usb_control_msg (dn, USB_DIR_OUT | USB_TYPE_VENDOR,
				   0x04, 0x8f, 0x00,
				   sizeof (ctrl), (unsigned char *) &ctrl);
      if (ret != SANE_STATUS_GOOD)
	{
	  DBG (DBG_err, "%s: USB-in-USB: error sending control message\n",
	       __FUNCTION__);
	  return ret;
	}

      /* USB-in-USB: checking acknowledge for control message */
      ret = hp5590_get_ack (dn, proto_flags);
      if (ret != SANE_STATUS_GOOD)
	return ret;

      len = size;
      ptr = bytes;
      /* Data is sent in 8 byte portions */
      while (len)
	{
	  unsigned int next_packet_size;
	  next_packet_size = 8;
	  if (len < 8)
	    next_packet_size = len;

	  /* Send USB-in-USB data */
	  ret = sanei_usb_control_msg (dn, USB_DIR_OUT | USB_TYPE_VENDOR,
				       core_flags & CORE_DATA ? 0x04 : 0x0c,
				       0x8f, 0x00, next_packet_size, ptr);
	  if (ret != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_err, "%s: USB-in-USB: error sending data\n", __FUNCTION__);
	      return ret;
	    }

	  /* CORE data is acknowledged packet by packet */
	  if (core_flags & CORE_DATA)
	    {
	      /* USB-in-USB: checking if data was accepted */
	      ret = hp5590_get_ack (dn, proto_flags);
	      if (ret != SANE_STATUS_GOOD)
		return ret;
	    }

	  ptr += next_packet_size;
	  len -= next_packet_size;
	}

      /* Normal (non-CORE) data is acknowledged after its full transmission */
      if (!(core_flags & CORE_DATA))
	{
	  /* USB-in-USB: checking if data was accepted */
	  ret = hp5590_get_ack (dn, proto_flags);
	  if (ret != SANE_STATUS_GOOD)
	    return ret;
	}

      /* Getting  response after data transmission */
      DBG (DBG_usb, "%s: USB-in-USB: getting response\n", __FUNCTION__);
      ret = sanei_usb_control_msg (dn, USB_DIR_IN | USB_TYPE_VENDOR,
				   0x0c, 0x90, 0x00,
				   sizeof (response), &response);
      if (ret != SANE_STATUS_GOOD)
	{
	  DBG (DBG_err, "%s: USB-in-USB: error getting response\n", __FUNCTION__);
	  return ret;
	}

      /* Necessary response after normal (non-CORE) data is 0x00,
       * after bulk OUT preparation - 0x24
       */
      needed_response = core_flags & CORE_BULK_OUT ? 0x24 : 0x00;
      if (response == needed_response)
	DBG (DBG_usb, "%s: USB-in-USB: got correct response\n",
	     __FUNCTION__);

      if (response != needed_response)
	{
	  DBG (DBG_err,
	       "%s: USB-in-USB: invalid response received "
	       "(needed %04x, got %04x)\n",
	       __FUNCTION__, needed_response, response);
	  return SANE_STATUS_IO_ERROR;
	}

      /* Send bulk OUT flags is bulk OUT preparation is performed */
      if (core_flags & CORE_BULK_OUT)
	{
	  uint8_t bulk_flags = 0x24;
	  DBG (DBG_usb, "%s: USB-in-USB: sending bulk flags\n",
	       __FUNCTION__);

	  ret = sanei_usb_control_msg (dn, USB_DIR_OUT | USB_TYPE_VENDOR,
				       0x0c, 0x83, 0x00,
				       sizeof (bulk_flags), &bulk_flags);
	  if (ret != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_err, "%s: USB-in-USB: error sending bulk flags\n",
		   __FUNCTION__);
	      return ret;
	    }

	  /* USB-in-USB: checking confirmation for bulk flags */
	  ret = hp5590_get_ack (dn, proto_flags);
	  if (ret != SANE_STATUS_GOOD)
	    return ret;
	}
    }

  return SANE_STATUS_GOOD;
}

/*******************************************************************************
 * USB-in-USB: verifies last command
 *
 * Parameters
 * dn - sanei_usb device descriptor
 * cmd - command to verify
 *
 * Returns
 * SANE_STATUS_GOOD - command verified successfully and CORE is ready
 * SANE_STATUS_IO_ERROR - command verification failed
 * SANE_STATUS_DEVICE_BUSY - command verified successfully but CORE isn't ready
 * all other SANE_Status values - otherwise
 */
static SANE_Status
hp5590_verify_last_cmd (SANE_Int dn,
			enum proto_flags proto_flags,
			unsigned int cmd)
{
  uint16_t	verify_cmd;
  unsigned int	last_cmd;
  unsigned int	core_status;
  SANE_Status 	ret;

  DBG (3, "%s: USB-in-USB: command verification requested\n",
       __FUNCTION__);

  /* Read last command along with CORE status */
  ret = hp5590_control_msg (dn,
  			    proto_flags,
  			    USB_DIR_IN,
			    0x04, 0xc5, 0x00,
			    (unsigned char *) &verify_cmd,
			    sizeof (verify_cmd), CORE_NONE);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  verify_cmd = le16toh (verify_cmd); /* Response is LSB first */

  /* Last command - minor byte */
  last_cmd = verify_cmd & 0xff;
  /* CORE status - major byte */
  core_status = (verify_cmd & 0xff00) >> 8;

  /* Verify last command */
  DBG (DBG_usb, "%s: USB-in-USB: command verification %04x, "
       "last command: %04x, core status: %04x\n",
       __FUNCTION__, verify_cmd, last_cmd, core_status);
  if ((cmd & 0x00ff) != last_cmd)
    {
      DBG (DBG_err, "%s: USB-in-USB: command verification failed: "
	   "expected 0x%04x, got 0x%04x\n",
	   __FUNCTION__, cmd, last_cmd);
      return SANE_STATUS_IO_ERROR;
    }

  DBG (DBG_usb, "%s: USB-in-USB: command verified successfully\n",
       __FUNCTION__);

  /* Return value depends on CORE status */
  return core_status & CORE_FLAG_NOT_READY ?
    SANE_STATUS_DEVICE_BUSY : SANE_STATUS_GOOD;
}

/*******************************************************************************
 * USB-in-USB: send command (convenience wrapper around hp5590_control_msg())
 *
 * Parameters
 * dn - sanei_usb device descriptor
 * requesttype, request, value, index - their meaninings are similar to
 * sanei_control_msg()
 * bytes - pointer to data buffer
 * size - size of data
 * core_flags - 
 *  CORE_NONE - no CORE operation will be performed
 *  CORE_DATA - operation on CORE data will be performed
 *  CORE_BULK_IN - preparation for bulk IN transfer (not used yet)
 *  CORE_BULK_OUT - preparation for bulk OUT trasfer
 *
 * Returns
 * SANE_STATUS_GOOD - command was sent (and possible verified) w/o any errors
 * all other SANE_Status values - otherwise
 */
static SANE_Status
hp5590_cmd (SANE_Int dn,
	    enum proto_flags proto_flags,
	    unsigned int flags,
	    unsigned int cmd, unsigned char *data, unsigned int size,
	    unsigned int core_flags)
{
  SANE_Status ret;

  DBG (3, "%s: USB-in-USB: command : %04x\n", __FUNCTION__, cmd);

  ret = hp5590_control_msg (dn,
  			    proto_flags,
			    flags & CMD_IN ? USB_DIR_IN : USB_DIR_OUT,
			    0x04, cmd, 0x00, data, size, core_flags);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  ret = SANE_STATUS_GOOD;
  /* Verify last command if requested */
  if (flags & CMD_VERIFY)
    {
      ret = hp5590_verify_last_cmd (dn, proto_flags, cmd);
    }

  return ret;
}

/*******************************************************************************
 * USB-in-USB: initialized bulk read state
 *
 * Parameters
 * state - pointer to a pointer for initialized state
 *
 * Returns
 * SANE_STATUS_GOOD - if state was initialized successfully
 * SANE_STATUS_NO_MEM - memory allocation failed
 */
static SANE_Status
hp5590_low_init_bulk_read_state (void **state)
{
  struct bulk_read_state *bulk_read_state;

  DBG (3, "%s: USB-in-USB: initializing bulk read state\n", __FUNCTION__);

  hp5590_low_assert (state != NULL);

  bulk_read_state = malloc (sizeof (struct bulk_read_state));
  if (!bulk_read_state)
    return SANE_STATUS_NO_MEM;
  memset (bulk_read_state, 0, sizeof (struct bulk_read_state));

  bulk_read_state->buffer = malloc (ALLOCATE_BULK_READ_PAGES
				    * BULK_READ_PAGE_SIZE);
  if (!bulk_read_state->buffer)
    {
      DBG (DBG_err, "%s: Memory allocation failed for %u bytes\n",
	   __FUNCTION__, ALLOCATE_BULK_READ_PAGES * BULK_READ_PAGE_SIZE);
      return SANE_STATUS_NO_MEM;
    }
  bulk_read_state->buffer_size = ALLOCATE_BULK_READ_PAGES
				 * BULK_READ_PAGE_SIZE;
  bulk_read_state->bytes_available = 0;
  bulk_read_state->buffer_out_ptr = bulk_read_state->buffer;
  bulk_read_state->buffer_in_ptr = bulk_read_state->buffer;
  bulk_read_state->total_pages = 0;
  bulk_read_state->buffer_end_ptr = bulk_read_state->buffer
				    + bulk_read_state->buffer_size;
  bulk_read_state->initialized = 1;

  *state = bulk_read_state;
  return SANE_STATUS_GOOD;
}

/*******************************************************************************
 * USB-in-USB: free bulk read state
 *
 * Parameters
 * state - pointer to a pointer to bulk read state
 *
 * Returns
 * SANE_STATUS_GOOD - bulk read state freed successfully
 */
static SANE_Status
hp5590_low_free_bulk_read_state (void **state)
{
  struct bulk_read_state *bulk_read_state;

  DBG (3, "%s\n", __FUNCTION__);

  hp5590_low_assert (state != NULL);
  /* Just return if NULL bulk read state was given */
  if (*state == NULL)
    return SANE_STATUS_GOOD;

  bulk_read_state = *state;

  DBG (3, "%s: USB-in-USB: freeing bulk read state\n", __FUNCTION__);

  free (bulk_read_state->buffer);
  bulk_read_state->buffer = NULL;
  free (bulk_read_state);
  *state = NULL;

  return SANE_STATUS_GOOD;
}

/* FIXME: perhaps needs to be converted to use hp5590_control_msg() */
/*******************************************************************************
 * USB-in-USB: bulk read
 *
 * Parameters
 * dn - sanei_usb device descriptor
 * bytes - pointer to data buffer
 * size - size of data to read
 * state - pointer to initialized bulk read state structure
 */
static SANE_Status
hp5590_bulk_read (SANE_Int dn,
		  enum proto_flags proto_flags,
		  unsigned char *bytes, unsigned int size,
		  void *state)
{
  struct usb_in_usb_bulk_setup	ctrl;
  SANE_Status 			ret;
  unsigned int 			next_pages;
  uint8_t 			bulk_flags;
  size_t 			next_portion;
  struct bulk_read_state	*bulk_read_state;
  unsigned int 			bytes_until_buffer_end;

  DBG (3, "%s\n", __FUNCTION__);

  hp5590_low_assert (state != NULL);
  hp5590_low_assert (bytes != NULL);

  bulk_read_state = state;
  if (bulk_read_state->initialized == 0)
    {
      DBG (DBG_err, "%s: USB-in-USB: bulk read state not initialized\n",
	   __FUNCTION__);
      return SANE_STATUS_INVAL;
    }

  memset (bytes, 0, size);

  /* Check if requested data would fit into the buffer */
  if (size > bulk_read_state->buffer_size)
    {
      DBG (DBG_err, "Data requested won't fit in the bulk read buffer "
	   "(requested: %u, buffer size: %u\n", size,
	   bulk_read_state->buffer_size);
      return SANE_STATUS_NO_MEM;
    }

  /* Read data until requested size of data will be received */
  while (bulk_read_state->bytes_available < size)
    {
      DBG (DBG_usb, "%s: USB-in-USB: not enough data in buffer available "
	   "(available: %u, requested: %u)\n",
	   __FUNCTION__, bulk_read_state->bytes_available, size);

      /* IMPORTANT! 'next_pages' means 'request and receive next_pages pages in
       * one bulk transfer request '. Windows driver uses 4 pages between each
       * request.  The more pages are received between requests the less the
       * scanner does scan head re-positioning thus improving scanning speed.
       * On the other hand, scanner expects that all of the requested pages
       * will be received immediately after the request. In case when a
       * frontend will have a delay between reads we will get bulk transfer
       * timeout sooner or later.
       * Having next_pages = 1 is the most safe case.
       */
      next_pages = 1;
      /* Count all received pages to calculate when we will need to send
       * another bulk request
       */
      bulk_read_state->total_pages++;
      DBG (DBG_usb, "%s: USB-in-USB: total pages done: %u\n",
	   __FUNCTION__, bulk_read_state->total_pages);

      /* Send another bulk request for 'next_pages' before first
       * page or next necessary one
       */
      if (   bulk_read_state->total_pages == 1
	  || bulk_read_state->total_pages % next_pages == 0)
	{
	  /* Send bulk flags */
	  DBG (DBG_usb, "%s: USB-in-USB: sending USB-in-USB bulk flags\n",
	       __FUNCTION__);
	  bulk_flags = 0x24;
	  ret = sanei_usb_control_msg (dn, USB_DIR_OUT | USB_TYPE_VENDOR,
				       0x0c, 0x83, 0x00,
				       sizeof (bulk_flags), &bulk_flags);
	  if (ret != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_err, "%s: USB-in-USB: error sending bulk flags\n",
		   __FUNCTION__);
	      return ret;
	    }

	  /* USB-in-USB: checking confirmation for bulk flags\n" */
	  ret = hp5590_get_ack (dn, proto_flags);
	  if (ret != SANE_STATUS_GOOD)
	    return ret;

	  /* Prepare bulk read request */
	  memset (&ctrl, 0, sizeof (ctrl));
	  ctrl.bRequestType = 0x00;
	  ctrl.bEndpoint = 0x82;
	  ctrl.wLength = htons (next_pages);

	  /* Send bulk read request */
	  DBG (DBG_usb, "%s: USB-in-USB: sending control msg for bulk\n",
	       __FUNCTION__);
	  ret = sanei_usb_control_msg (dn, USB_DIR_OUT | USB_TYPE_VENDOR,
				       0x04, 0x82, 0x00,
				       sizeof (ctrl),
				       (unsigned char *) &ctrl);
	  if (ret != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_err, "%s: USB-in-USB: error sending control msg\n",
		   __FUNCTION__);
	      return ret;
	    }

	  /* USB-in-USB: checking if control msg was accepted */
	  ret = hp5590_get_ack (dn, proto_flags); 
	  if (ret != SANE_STATUS_GOOD)
	    return ret;
	}

      next_portion = BULK_READ_PAGE_SIZE;
      /* Check if next page will fit into the buffer */
      if (bulk_read_state->buffer_size
	  - bulk_read_state->bytes_available < next_portion)
	{
	  DBG (DBG_err, "%s: USB-in-USB: buffer too small\n", __FUNCTION__);
	  return SANE_STATUS_NO_MEM;
	}

      /* Bulk read next page */
      DBG (DBG_usb, "%s: USB-in-USB: bulk reading %lu bytes\n",
	   __FUNCTION__, (u_long) next_portion);
      ret = sanei_usb_read_bulk (dn,
				 bulk_read_state->buffer_in_ptr,
				 &next_portion);
      if (ret != SANE_STATUS_GOOD)
	{
	  if (ret == SANE_STATUS_EOF)
	    return ret;
	  DBG (DBG_err, "%s: USB-in-USB: error during bulk read: %s\n",
	       __FUNCTION__, sane_strstatus (ret));
	  return ret;
	}

      /* Check if we received the same amount of data as requsted */
      if (next_portion != BULK_READ_PAGE_SIZE)
	{
	  DBG (DBG_err, "%s: USB-in-USB: incomplete bulk read "
	       "(requested %u bytes, got %lu bytes)\n",
	       __FUNCTION__, BULK_READ_PAGE_SIZE, (u_long) next_portion);
	  return SANE_STATUS_IO_ERROR;
	}

      /* Move pointers to the next position */
      bulk_read_state->buffer_in_ptr += next_portion;

      /* Check for the end of the buffer */
      if (bulk_read_state->buffer_in_ptr > bulk_read_state->buffer_end_ptr)
	{
	  DBG (DBG_err,
	       "%s: USB-in-USB: attempted to access over the end of buffer "
	       "(in_ptr: %p, end_ptr: %p, ptr: %p, buffer size: %u\n",
	       __FUNCTION__, bulk_read_state->buffer_in_ptr,
	       bulk_read_state->buffer_end_ptr, bulk_read_state->buffer,
	       bulk_read_state->buffer_size);
	  return SANE_STATUS_NO_MEM;
	}

      /* Check for buffer pointer wrapping */
      if (bulk_read_state->buffer_in_ptr == bulk_read_state->buffer_end_ptr)
	{
	  DBG (DBG_usb, "%s: USB-in-USB: buffer wrapped while writing\n",
	       __FUNCTION__);
	  bulk_read_state->buffer_in_ptr = bulk_read_state->buffer;
	}

      /* Count the amount of data we read */
      bulk_read_state->bytes_available += next_portion;
    }

  /* Transfer requested amount of data to the caller */
  DBG (DBG_usb, "%s: USB-in-USB: data in bulk buffer is available "
       "(requested %u bytes, available %u bytes)\n",
       __FUNCTION__, size, bulk_read_state->bytes_available);

  /* Check for buffer pointer wrapping */
  bytes_until_buffer_end = bulk_read_state->buffer_end_ptr
    - bulk_read_state->buffer_out_ptr;
  if (bytes_until_buffer_end <= size)
    {
      /* First buffer part */
      DBG (DBG_usb, "%s: USB-in-USB: reached bulk read buffer end\n", __FUNCTION__);
      memcpy (bytes, bulk_read_state->buffer_out_ptr, bytes_until_buffer_end);
      bulk_read_state->buffer_out_ptr = bulk_read_state->buffer;
      /* And second part (if any) */
      if (bytes_until_buffer_end < size)
	{
	  DBG (DBG_usb, "%s: USB-in-USB: giving 2nd buffer part\n", __FUNCTION__);
	  memcpy (bytes + bytes_until_buffer_end,
		  bulk_read_state->buffer_out_ptr,
		  size - bytes_until_buffer_end);
	  bulk_read_state->buffer_out_ptr += size - bytes_until_buffer_end;
	}
    }
  else
    {
      /* The data is in one buffer part (w/o wrapping) */
      memcpy (bytes, bulk_read_state->buffer_out_ptr, size);
      bulk_read_state->buffer_out_ptr += size;
      if (bulk_read_state->buffer_out_ptr == bulk_read_state->buffer_end_ptr)
	{
	  DBG (DBG_usb, "%s: USB-in-USB: buffer wrapped while reading\n",
	       __FUNCTION__);
	  bulk_read_state->buffer_out_ptr = bulk_read_state->buffer;
	}
    }

  /* Count the amount of data transferred to the caller */
  bulk_read_state->bytes_available -= size;

  return SANE_STATUS_GOOD;
}

/*******************************************************************************
 * USB-in-USB: bulk write
 *
 * Parameters
 * dn - sanei_usb device descriptor
 * cmd - command for bulk write operation
 * bytes - pointer to data buffer
 * size - size of data
 *
 * Returns
 * SANE_STATUS_GOOD - all data transferred successfully
 * all other SANE_Status value - otherwise 
 */
static SANE_Status
hp5590_bulk_write (SANE_Int dn,
		   enum proto_flags proto_flags,
		   int cmd, unsigned char *bytes,
		   unsigned int size)
{
  struct usb_in_usb_bulk_setup	ctrl;
  SANE_Status 			ret;
  struct bulk_size		bulk_size;

  unsigned int len;
  unsigned char *ptr;
  size_t next_portion;

  DBG (3, "%s: USB-in-USB: command: %04x, size %u\n", __FUNCTION__, cmd,
       size);

  hp5590_low_assert (bytes != NULL);

  /* Prepare bulk write request */ 
  memset (&bulk_size, 0, sizeof (bulk_size));
  /* Counted in page size */
  bulk_size.size = size / BULK_WRITE_PAGE_SIZE;

  /* Send bulk write request */
  DBG (3, "%s: USB-in-USB: total %u pages (each of %u bytes)\n",
       __FUNCTION__, bulk_size.size, BULK_WRITE_PAGE_SIZE);
  ret = hp5590_control_msg (dn,
  			    proto_flags,
  			    USB_DIR_OUT,
			    0x04, cmd, 0,
			    (unsigned char *) &bulk_size, sizeof (bulk_size),
			    CORE_DATA | CORE_BULK_OUT);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  len = size;
  ptr = bytes;

  /* Send all data in pages */
  while (len)
    {
      next_portion = BULK_WRITE_PAGE_SIZE;
      if (len < next_portion)
	next_portion = len;

      DBG (3, "%s: USB-in-USB: next portion %lu bytes\n",
	   __FUNCTION__, (u_long) next_portion);

      /* Prepare bulk write request */
      memset (&ctrl, 0, sizeof (ctrl));
      ctrl.bRequestType = 0x01;
      ctrl.bEndpoint = 0x82;
      ctrl.wLength = htons (next_portion);

      /* Send bulk write request */
      ret = sanei_usb_control_msg (dn, USB_DIR_OUT | USB_TYPE_VENDOR,
				   0x04, 0x82, 0,
				   sizeof (ctrl), (unsigned char *) &ctrl);
      if (ret != SANE_STATUS_GOOD)
	return ret;

      /* USB-in-USB: checking if command was accepted */
      ret = hp5590_get_ack (dn, proto_flags);
      if (ret != SANE_STATUS_GOOD)
	return ret;

      /* Write bulk data */
      DBG (3, "%s: USB-in-USB: bulk writing %lu bytes\n",
	   __FUNCTION__, (u_long) next_portion);
      ret = sanei_usb_write_bulk (dn, ptr, &next_portion);
      if (ret != SANE_STATUS_GOOD)
	{
	  /* Treast EOF as successful result */
	  if (ret == SANE_STATUS_EOF)
	    break;
	  DBG (DBG_err, "%s: USB-in-USB: error during bulk write: %s\n",
	       __FUNCTION__, sane_strstatus (ret));
	  return ret;
	}

      /* Move to the next page */
      len -= next_portion;
      ptr += next_portion;
    }

  /* Verify bulk command */
  return hp5590_verify_last_cmd (dn, proto_flags, cmd);
}
/* vim: sw=2 ts=8
 */
