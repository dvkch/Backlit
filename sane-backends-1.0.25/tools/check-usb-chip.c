/*
   check-usb-chip.c -- Find out what USB scanner chipset is used
   
   Copyright (C) 2003-2005 Henning Meier-Geinitz <henning@meier-geinitz.de>
   Copyright (C) 2003 Gerhard Jaeger <gerhard@gjaeger.de>
                      for LM983x tests
   Copyright (C) 2003 Gerard Klaver <gerard at gkall dot hobby dot nl>
                      for ICM532B tests

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
*/


#include "../include/sane/config.h"

#ifdef HAVE_LIBUSB

#include "../include/sane/sane.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_LUSB0_USB_H
#include <lusb0_usb.h>
#else
#include <usb.h>
#endif

#include "../include/_stdint.h"

static int verbose = 0;
static SANE_Bool no_chipset_access;
#define TIMEOUT 1000

extern char *check_usb_chip (struct usb_device *dev, int verbosity,
			     SANE_Bool from_file);

static int
prepare_interface (struct usb_device *dev, usb_dev_handle ** handle)
{
  int result;

  if (no_chipset_access)
    return 0;

  *handle = usb_open (dev);
  if (*handle == 0)
    {
      if (verbose > 1)
	printf ("    Couldn't open device: %s\n", usb_strerror ());
      return 0;
    }

  result =
    usb_set_configuration (*handle, dev->config[0].bConfigurationValue);
  if (result < 0)
    {
      if (verbose > 1)
	printf ("  Couldn't set configuration: %s\n", usb_strerror ());
      usb_close (*handle);
      return 0;
    }

  result = usb_claim_interface (*handle, 0);
  if (result < 0)
    {
      if (verbose > 1)
	printf ("    Couldn't claim interface: %s\n", usb_strerror ());
      usb_close (*handle);
      return 0;
    }
  return 1;
}

static void
finish_interface (usb_dev_handle * handle)
{
  usb_release_interface (handle, 0);
  usb_close (handle);
}

/* Check for Grandtech GT-6801 */
static char *
check_gt6801 (struct usb_device *dev)
{
  char req[64];
  usb_dev_handle *handle;
  int result;

  if (verbose > 2)
    printf ("    checking for GT-6801 ...\n");

  /* Check device descriptor */
  if (dev->descriptor.bDeviceClass != USB_CLASS_VENDOR_SPEC)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6801 (bDeviceClass = %d)\n",
		dev->descriptor.bDeviceClass);
      return 0;
    }
  if (dev->descriptor.bcdUSB != 0x110)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6801 (bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6801 (bDeviceSubClass = 0x%x)\n",
		dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if (dev->descriptor.bDeviceProtocol != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6801 (bDeviceProtocol = 0x%x)\n",
		dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 1)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6801 (bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x81)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GT-6801 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }

  /* Now we send a control message */
  result = prepare_interface (dev, &handle);
  if (!result)
    return "GT-6801?";

  memset (req, 0, 64);
  req[0] = 0x2e;		/* get identification information */
  req[1] = 0x01;

  result =
    usb_control_msg (handle, 0x40, 0x01, 0x2010, 0x3f40, req, 64, TIMEOUT);
  if (result <= 0)
    {
      if (verbose > 2)
	printf ("    Couldn't send write control message (%s)\n",
		strerror (errno));
      finish_interface (handle);
      return 0;
    }
  result =
    usb_control_msg (handle, 0xc0, 0x01, 0x2011, 0x3f00, req, 64, TIMEOUT);
  if (result <= 0)
    {
      if (verbose > 2)
	printf ("    Couldn't send read control message (%s)\n",
		strerror (errno));
      finish_interface (handle);
      return 0;
    }
  if (req[0] != 0 || (req[1] != 0x2e && req[1] != 0))
    {
      if (verbose > 2)
	printf ("    Unexpected result from control message (%0x/%0x)\n",
		req[0], req[1]);
      finish_interface (handle);
      return 0;
    }
  finish_interface (handle);
  return "GT-6801";
}

/* Check for Grandtech GT-6816 */
static char *
check_gt6816 (struct usb_device *dev)
{
  char req[64];
  usb_dev_handle *handle;
  int result;
  int i;

  if (verbose > 2)
    printf ("    checking for GT-6816 ...\n");

  /* Check device descriptor */
  if ((dev->descriptor.bDeviceClass != USB_CLASS_PER_INTERFACE)
      || (dev->config[0].interface[0].altsetting[0].bInterfaceClass !=
	  USB_CLASS_VENDOR_SPEC))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GT-6816 (bDeviceClass = %d, bInterfaceClass = %d)\n",
	   dev->descriptor.bDeviceClass,
	   dev->config[0].interface[0].altsetting[0].bInterfaceClass);
      return 0;
    }
  if (dev->descriptor.bcdUSB != 0x110)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6816 (bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0x00)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6816 (bDeviceSubClass = 0x%x)\n",
		dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if (dev->descriptor.bDeviceProtocol != 0x00)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6816 (bDeviceProtocol = 0x%x)\n",
		dev->descriptor.bDeviceProtocol);
      return 0;
    }

  if (dev->config[0].bNumInterfaces != 0x01)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6816 (bNumInterfaces = 0x%x)\n",
		dev->config[0].bNumInterfaces);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 2)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6816 (bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x81)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GT-6816 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[1].
       bEndpointAddress != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GT-6816 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval);
      return 0;

    }

  /* Now we send a control message */
  result = prepare_interface (dev, &handle);
  if (!result)
    return "GT-6816?";

  memset (req, 0, 64);
  for (i = 0; i < 8; i++)
    {
      req[8 * i + 0] = 0x73;	/* check firmware */
      req[8 * i + 1] = 0x01;
    }

  result =
    usb_control_msg (handle, 0x40, 0x04, 0x2012, 0x3f40, req, 64, TIMEOUT);
  if (result <= 0)
    {
      if (verbose > 2)
	printf ("    Couldn't send write control message (%s)\n",
		strerror (errno));
      finish_interface (handle);
      return 0;
    }
  result =
    usb_control_msg (handle, 0xc0, 0x01, 0x2013, 0x3f00, req, 64, TIMEOUT);
  if (result <= 0)
    {
      /* Before firmware upload, 64 bytes are returned. Some libusb
	 implementations/operating systems can't seem to cope with short
	 packets. */
      result =
	usb_control_msg (handle, 0xc0, 0x01, 0x2013, 0x3f00, req, 8, TIMEOUT);
      if (result <= 0)
	{
	  if (verbose > 2)
	    printf ("    Couldn't send read control message (%s)\n",
		    strerror (errno));
	  finish_interface (handle);
	  return 0;
	}
    }

  if (req[0] != 0)
    {
      if (verbose > 2)
	printf ("    Unexpected result from control message (%0x/%0x)\n",
		req[0], req[1]);
      finish_interface (handle);
      return 0;
    }
  finish_interface (handle);
  return "GT-6816";
}

/* Check for Grandtech GT-8911 */
static char *
check_gt8911 (struct usb_device *dev)
{
  char req[64];
  usb_dev_handle *handle;
  int result;

  if (verbose > 2)
    printf ("    checking for GT-8911 ...\n");

  /* Check device descriptor */
  if ((dev->descriptor.bDeviceClass != USB_CLASS_PER_INTERFACE)
      || (dev->config[0].interface[0].altsetting[0].bInterfaceClass !=
	  USB_CLASS_VENDOR_SPEC))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GT-8911 (check 1, bDeviceClass = %d, bInterfaceClass = %d)\n",
	   dev->descriptor.bDeviceClass,
	   dev->config[0].interface[0].altsetting[0].bInterfaceClass);
      return 0;
    }
  if (dev->descriptor.bcdUSB != 0x110)
    {
      if (verbose > 2)
	printf ("    this is not a GT-8911 (check 2, bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0x00)
    {
      if (verbose > 2)
	printf
	  ("    this is not a GT-8911 (check 3, bDeviceSubClass = 0x%x)\n",
	   dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if (dev->descriptor.bDeviceProtocol != 0x00)
    {
      if (verbose > 2)
	printf
	  ("    this is not a GT-8911 (check 4, bDeviceProtocol = 0x%x)\n",
	   dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 2)
    {
      if (verbose > 2)
	printf ("    this is not a GT-8911 (check 5, bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }
  /* Check first endpoint descriptor block */
  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x81)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GT-8911 (check 6, bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[1].
       bEndpointAddress != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GT-8911 (check 7, bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval);
      return 0;

    }
  if (dev->config[0].bNumInterfaces < 2)
    {
      if (verbose > 2)
	printf ("    this is not a GT-8911 (check 8, bNumInterfaces = %d)\n",
		dev->config[0].bNumInterfaces);
      return 0;
    }
  if (dev->config[0].interface[1].num_altsetting < 3)
    {
      if (verbose > 2)
	printf ("    this is not a GT-8911 (check 9, num_altsetting = %d)\n",
		dev->config[0].interface[1].num_altsetting);
      return 0;
    }

  /* Check fourth endpoint descriptor block */
  if ((dev->config[0].interface[1].altsetting[2].endpoint[0].
       bEndpointAddress != 0x83)
      || (dev->config[0].interface[1].altsetting[2].endpoint[0].
	  bmAttributes != 0x01)
      || (dev->config[0].interface[1].altsetting[2].endpoint[0].
	  wMaxPacketSize != 0x01d0)
      || (dev->config[0].interface[1].altsetting[2].endpoint[0].bInterval !=
	  0x01))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GT-8911 (check 10, bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[1].altsetting[2].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[1].altsetting[2].endpoint[0].bmAttributes,
	   dev->config[0].interface[1].altsetting[2].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[1].altsetting[2].endpoint[0].bInterval);
      return 0;
    }
  if ((dev->config[0].interface[1].altsetting[2].endpoint[1].
       bEndpointAddress != 0x04)
      || (dev->config[0].interface[1].altsetting[2].endpoint[1].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[1].altsetting[2].endpoint[1].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[1].altsetting[2].endpoint[1].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GT-8911 (check 11, bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[1].altsetting[2].endpoint[1].
	   bEndpointAddress,
	   dev->config[0].interface[1].altsetting[2].endpoint[1].bmAttributes,
	   dev->config[0].interface[1].altsetting[2].endpoint[1].
	   wMaxPacketSize,
	   dev->config[0].interface[1].altsetting[2].endpoint[1].bInterval);
      return 0;

    }

  /* Now we send a control message */
  result = prepare_interface (dev, &handle);
  if (!result)
    return "GT-8911?";

  memset (req, 0, 8);
  req[0] = 0x55;
  req[1] = 0x66;

  result =
    usb_control_msg (handle, 0xc0, 0x10, 0x41, 0x0000, req, 64, TIMEOUT);
  if (result <= 0)
    {
      if (verbose > 2)
	printf
	  ("    this is not a GT-8911 (check 12, couldn't send read control message (%s))\n",
	   strerror (errno));
      finish_interface (handle);
      return 0;
    }
  result =
    usb_control_msg (handle, 0xc0, 0x10, 0x05, 0x0000, req, 64, TIMEOUT);
  if (result <= 0)
    {
      if (verbose > 2)
	printf
	  ("    this is not a GT-8911 (check 13, couldn't send read control message (%s)\n",
	   strerror (errno));
      finish_interface (handle);
      return 0;
    }
  /* tested on model hardware version 0xffffffc0, firmware version 0x10)) */
  if (verbose > 2)
    printf
      ("    Check 14, control message (hardware version %0x / firmware version %0x)\n",
       req[0], req[1]);

  finish_interface (handle);
  return "GT-8911";
}

/* Check for Mustek MA-1017 */
static char *
check_ma1017 (struct usb_device *dev)
{
  char req[2];
  usb_dev_handle *handle;
  int result;
  char res;

  if (verbose > 2)
    printf ("    checking for MA-1017 ...\n");

  /* Check device descriptor */
  if ((dev->descriptor.bDeviceClass != USB_CLASS_PER_INTERFACE)
      || (dev->config[0].interface[0].altsetting[0].bInterfaceClass !=
	  USB_CLASS_PER_INTERFACE))
    {
      if (verbose > 2)
	printf
	  ("    this is not a MA-1017 (bDeviceClass = %d, bInterfaceClass = %d)\n",
	   dev->descriptor.bDeviceClass,
	   dev->config[0].interface[0].altsetting[0].bInterfaceClass);
      return 0;
    }
  if (dev->descriptor.bcdUSB != 0x100)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1017 (bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0x00)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1017 (bDeviceSubClass = 0x%x)\n",
		dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if (dev->descriptor.bDeviceProtocol != 0x00)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1017 (bDeviceProtocol = 0x%x)\n",
		dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 3)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1017 (bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x01)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  wMaxPacketSize != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a MA-1017 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[1].
       bEndpointAddress != 0x82)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a MA-1017 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[2].
       bEndpointAddress != 0x83)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  bmAttributes != 0x03)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  wMaxPacketSize != 0x01)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval !=
	  0x01))
    {
      if (verbose > 2)
	printf
	  ("    this is not a MA-1017 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval);
      return 0;
    }

  /* read a register value */
  result = prepare_interface (dev, &handle);
  if (!result)
    return "MA-1017?";

  req[0] = 0x00;
  req[1] = 0x02 | 0x20;
  result = usb_bulk_write (handle, 0x01, req, 2, 1000);
  if (result <= 0)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1017 (Error during bulk write)\n");
      finish_interface (handle);
      return 0;
    }
  result = usb_bulk_read (handle, 0x82, &res, 1, 1000);
  if (result <= 0)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1017 (Error during bulk read)\n");
      finish_interface (handle);
      return 0;
    }
  /* Read one byte again to work around a bug in the MA-1017 chipset that 
     appears when an odd number of bytes is read or written. */
  result = usb_bulk_write (handle, 0x01, req, 2, 1000);
  result = usb_bulk_read (handle, 0x82, &res, 1, 1000);
  finish_interface (handle);
  return "MA-1017";
}

/* Check for Mustek MA-1015 */
static char *
check_ma1015 (struct usb_device *dev)
{
  char req[8];
  usb_dev_handle *handle;
  int result;
  unsigned char res;

  if (verbose > 2)
    printf ("    checking for MA-1015 ...\n");

  /* Check device descriptor */
  if (dev->descriptor.bDeviceClass != USB_CLASS_VENDOR_SPEC)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1015 (bDeviceClass = %d)\n",
		dev->descriptor.bDeviceClass);
      return 0;
    }
  if (dev->descriptor.bcdUSB != 0x100)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1015 (bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1015 (bDeviceSubClass = 0x%x)\n",
		dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if (dev->descriptor.bDeviceProtocol != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1015 (bDeviceProtocol = 0x%x)\n",
		dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 2)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1015 (bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x81)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a MA-1015 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[1].
       bEndpointAddress != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  wMaxPacketSize != 0x08)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a MA-1015 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval);
      return 0;
    }

  /* Now we read register 0 to find out if this is really a MA-1015 */
  result = prepare_interface (dev, &handle);
  if (!result)
    return 0;

  memset (req, 0, 8);
  req[0] = 33;
  req[1] = 0x00;
  result = usb_bulk_write (handle, 0x02, req, 8, TIMEOUT);
  if (result <= 0)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1015 (Error during bulk write)\n");
      finish_interface (handle);
      return 0;
    }
  result = usb_bulk_read (handle, 0x81, (char *) &res, 1, TIMEOUT);
  if (result <= 0)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1015 (Error during bulk read)\n");
      finish_interface (handle);
      return 0;
    }
  if (res != 0xa5)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1015 (got 0x%x, expected 0xa5)\n", res);
      finish_interface (handle);
      return 0;
    }
  finish_interface (handle);
  return "MA-1015";
}

/* Check for Mustek MA-1509 */
static char *
check_ma1509 (struct usb_device *dev)
{
  char req[8];
  char inquiry[0x60];
  usb_dev_handle *handle;
  int result;

  if (verbose > 2)
    printf ("    checking for MA-1509 ...\n");

  /* Check device descriptor */
  if (dev->descriptor.bDeviceClass != USB_CLASS_VENDOR_SPEC)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1509 (bDeviceClass = %d)\n",
		dev->descriptor.bDeviceClass);
      return 0;
    }
  if (dev->descriptor.bcdUSB != 0x110)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1509 (bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1509 (bDeviceSubClass = 0x%x)\n",
		dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if (dev->descriptor.bDeviceProtocol != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1509 (bDeviceProtocol = 0x%x)\n",
		dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 3)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1509 (bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x81)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  wMaxPacketSize != 0x40))
    {
      if (verbose > 2)
	printf
	  ("    this is not a MA-1509 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[1].
       bEndpointAddress != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  wMaxPacketSize != 0x08))
    {
      if (verbose > 2)
	printf
	  ("    this is not a MA-1509 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   wMaxPacketSize);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[2].
       bEndpointAddress != 0x83)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  bmAttributes != 0x03)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  wMaxPacketSize != 0x08)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval !=
	  0x10))
    {
      if (verbose > 2)
	printf
	  ("    this is not a MA-1509 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval);
      return 0;
    }

  /* This is a SCSI-over-USB chip, we'll read the inquiry */
  result = prepare_interface (dev, &handle);
  if (!result)
    return "MA-1509?";

  memset (req, 0, 8);
  req[0] = 0x12;
  req[1] = 1;
  req[6] = 0x60;

  result = usb_bulk_write (handle, 0x02, req, 8, TIMEOUT);
  if (result <= 0)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1509 (Error during bulk write)\n");
      finish_interface (handle);
      return 0;
    }
  memset (inquiry, 0, 0x60);
  result = usb_bulk_read (handle, 0x81, (char *) inquiry, 0x60, TIMEOUT);
  if (result != 0x60)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1509 (Error during bulk read: %d)\n",
		result);
      finish_interface (handle);
      return 0;
    }
  if ((inquiry[0] & 0x1f) != 0x06)
    {
      if (verbose > 2)
	printf ("    this is not a MA-1509 (inquiry [0] = %d)\n", inquiry[0]);
      finish_interface (handle);
      return 0;
    }
  if (strncmp (inquiry + 8, "SCANNER ", 8) != 0)
    {
      inquiry[16] = 0;
      if (verbose > 2)
	printf ("    this is not a MA-1509 (vendor=%s)\n", inquiry + 8);
      finish_interface (handle);
      return 0;
    }
  inquiry[36] = 0;
  if (verbose > 2)
    printf ("    MA-1509 version %s\n", inquiry + 32);

  finish_interface (handle);
  return "MA-1509";
}

/********** the lm983x section **********/

static int
lm983x_wb (usb_dev_handle * handle, unsigned char reg, unsigned char val)
{
  unsigned char buf[5];
  int result;

  buf[0] = 0;
  buf[1] = reg;
  buf[2] = 0;
  buf[3] = 1;
  buf[4] = val;

  result = usb_bulk_write (handle, 3, (char *) buf, 5, TIMEOUT);
  if (result != 5)
    return 0;

  return 1;
}

static int
lm983x_rb (usb_dev_handle * handle, unsigned char reg, unsigned char *val)
{
  unsigned char buf[5];
  int result;

  buf[0] = 1;
  buf[1] = reg;
  buf[2] = 0;
  buf[3] = 1;

  result = usb_bulk_write (handle, 3, (char *) buf, 4, TIMEOUT);
  if (result != 4)
    return 0;


  result = usb_bulk_read (handle, 2, (char *) val, 1, TIMEOUT);
  if (result != 1)
    return 0;

  return 1;
}

static char *
check_merlin (struct usb_device *dev)
{
  unsigned char val;
  int result;
  usb_dev_handle *handle;

  if (verbose > 2)
    printf ("    checking for LM983[1,2,3] ...\n");

  /* Check device descriptor */
  if (((dev->descriptor.bDeviceClass != USB_CLASS_VENDOR_SPEC)
       && (dev->descriptor.bDeviceClass != 0))
      || (dev->config[0].interface[0].altsetting[0].bInterfaceClass !=
	  USB_CLASS_VENDOR_SPEC))
    {
      if (verbose > 2)
	printf
	  ("    this is not a LM983x (bDeviceClass = %d, bInterfaceClass = %d)\n",
	   dev->descriptor.bDeviceClass,
	   dev->config[0].interface[0].altsetting[0].bInterfaceClass);
      return 0;
    }
  if ((dev->descriptor.bcdUSB != 0x110)
      && (dev->descriptor.bcdUSB != 0x101)
      && (dev->descriptor.bcdUSB != 0x100))
    {
      if (verbose > 2)
	printf ("    this is not a LM983x (bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0x00)
    {
      if (verbose > 2)
	printf ("    this is not a LM983x (bDeviceSubClass = 0x%x)\n",
		dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if ((dev->descriptor.bDeviceProtocol != 0) &&
      (dev->descriptor.bDeviceProtocol != 0xff))
    {
      if (verbose > 2)
	printf ("    this is not a LM983x (bDeviceProtocol = 0x%x)\n",
		dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 3)
    {
      if (verbose > 2)
	printf ("    this is not a LM983x (bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x81)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x03)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  wMaxPacketSize != 0x1)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval !=
	  0x10))
    {
      if (verbose > 2)
	printf
	  ("    this is not a LM983x (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[1].
       bEndpointAddress != 0x82)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  wMaxPacketSize != 0x40)
      /* Currently disabled as we have some problems in detection here ! */
      /*|| (dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval != 0) */
    )
    {
      if (verbose > 2)
	printf
	  ("    this is not a LM983x (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[2].
       bEndpointAddress != 0x03)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  wMaxPacketSize != 0x40)
      /* Currently disabled as we have some problems in detection here ! */
      /* || (dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval != 0) */
    )
    {
      if (verbose > 2)
	printf
	  ("    this is not a LM983x (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval);
      return 0;
    }

  result = prepare_interface (dev, &handle);
  if (!result)
    return "LM983x?";

  result = lm983x_wb (handle, 0x07, 0x00);
  if (1 == result)
    result = lm983x_wb (handle, 0x08, 0x02);
  if (1 == result)
    result = lm983x_rb (handle, 0x07, &val);
  if (1 == result)
    result = lm983x_rb (handle, 0x08, &val);
  if (1 == result)
    result = lm983x_rb (handle, 0x69, &val);

  if (0 == result)
    {
      if (verbose > 2)
	printf ("  Couldn't access LM983x registers.\n");
      finish_interface (handle);
      return 0;
    }

  finish_interface (handle);

  switch (val)
    {
    case 4:
      return "LM9832/3";
      break;
    case 3:
      return "LM9831";
      break;
    case 2:
      return "LM9830";
      break;
    default:
      return "LM983x?";
      break;
    }
}

/********** the gl646 section **********/


static int
gl646_write_reg (usb_dev_handle * handle, unsigned char reg,
		 unsigned char val)
{
  int result;

  result =
    usb_control_msg (handle, 0x00, 0x00, 0x83, 0x00, (char *) &reg, 0x01,
		     TIMEOUT);
  if (result < 0)
    return 0;

  result =
    usb_control_msg (handle, 0x00, 0x00, 0x85, 0x00, (char *) &val, 0x01,
		     TIMEOUT);
  if (result < 0)
    return 0;

  return 1;
}

static int
gl646_read_reg (usb_dev_handle * handle, unsigned char reg,
		unsigned char *val)
{
  int result;

  result =
    usb_control_msg (handle, 0x00, 0x00, 0x83, 0x00, (char *) &reg, 0x01,
		     TIMEOUT);
  if (result < 0)
    return 0;

  result =
    usb_control_msg (handle, 0x80, 0x00, 0x84, 0x00, (char *) val, 0x01,
		     TIMEOUT);
  if (result < 0)
    return 0;

  return 1;
}


static char *
check_gl646 (struct usb_device *dev)
{
  unsigned char val;
  int result;
  usb_dev_handle *handle;

  if (verbose > 2)
    printf ("    checking for GL646 ...\n");

  /* Check device descriptor */
  if ((dev->descriptor.bDeviceClass != USB_CLASS_PER_INTERFACE)
      || (dev->config[0].interface[0].altsetting[0].bInterfaceClass != 0x10))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL646 (bDeviceClass = %d, bInterfaceClass = %d)\n",
	   dev->descriptor.bDeviceClass,
	   dev->config[0].interface[0].altsetting[0].bInterfaceClass);
      return 0;
    }
  if (dev->descriptor.bcdUSB != 0x110)
    {
      if (verbose > 2)
	printf ("    this is not a GL646 (bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0x00)
    {
      if (verbose > 2)
	printf ("    this is not a GL646 (bDeviceSubClass = 0x%x)\n",
		dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if (dev->descriptor.bDeviceProtocol != 0)
    {
      if (verbose > 2)
	printf ("    this is not a GL646 (bDeviceProtocol = 0x%x)\n",
		dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 3)
    {
      if (verbose > 2)
	printf ("    this is not a GL646 (bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x81)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval !=
	  0x0))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL646 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[1].
       bEndpointAddress != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval !=
	  0))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL646 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[2].
       bEndpointAddress != 0x83)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  bmAttributes != 0x03)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  wMaxPacketSize != 0x1)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval !=
	  8))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL646 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval);
      return 0;
    }

  result = prepare_interface (dev, &handle);
  if (!result)
    return "GL646?";

  result = gl646_write_reg (handle, 0x38, 0x15);
  if (!result)
    {
      if (verbose > 2)
	printf ("    this is not a GL646 (writing register failed)\n");
      finish_interface (handle);
      return 0;
    }

  result = gl646_read_reg (handle, 0x4e, &val);
  if (!result)
    {
      if (verbose > 2)
	printf ("    this is not a GL646 (reading register failed)\n");
      finish_interface (handle);
      return 0;
    }

  if (val != 0x15)
    {
      if (verbose > 2)
	printf ("    this is not a GL646 (reg 0x4e != reg 0x38)\n");
      finish_interface (handle);
      return 0;
    }
  finish_interface (handle);
  return "GL646";
}

/* Same as check_gl646, except that sanity check are different. */
static char *
check_gl646_hp (struct usb_device *dev)
{
  unsigned char val;
  int result;
  usb_dev_handle *handle;

  if (verbose > 2)
    printf ("    checking for GL646_HP ...\n");

  /* Check device descriptor */
  if ((dev->descriptor.bDeviceClass != 0xff)
      || (dev->config[0].interface[0].altsetting[0].bInterfaceClass != 0xff))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL646_HP (bDeviceClass = %d, bInterfaceClass = %d)\n",
	   dev->descriptor.bDeviceClass,
	   dev->config[0].interface[0].altsetting[0].bInterfaceClass);
      return 0;
    }
  if (dev->descriptor.bcdUSB != 0x110)
    {
      if (verbose > 2)
	printf ("    this is not a GL646_HP (bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a GL646_HP (bDeviceSubClass = 0x%x)\n",
		dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if (dev->descriptor.bDeviceProtocol != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a GL646_HP (bDeviceProtocol = 0x%x)\n",
		dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 3)
    {
      if (verbose > 2)
	printf ("    this is not a GL646_HP (bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x81)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval !=
	  0x0))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL646_HP (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[1].
       bEndpointAddress != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval !=
	  0))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL646_HP (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[2].
       bEndpointAddress != 0x83)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  bmAttributes != 0x03)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  wMaxPacketSize != 0x1)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval !=
	  8))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL646_HP (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval);
      return 0;
    }

  result = prepare_interface (dev, &handle);
  if (!result)
    return "GL646_HP?";

  result = gl646_write_reg (handle, 0x38, 0x15);
  if (!result)
    {
      if (verbose > 2)
	printf ("    this is not a GL646_HP (writing register failed)\n");
      finish_interface (handle);
      return 0;
    }

  result = gl646_read_reg (handle, 0x4e, &val);
  if (!result)
    {
      if (verbose > 2)
	printf ("    this is not a GL646_HP (reading register failed)\n");
      finish_interface (handle);
      return 0;
    }

  if (val != 0x15)
    {
      if (verbose > 2)
	printf ("    this is not a GL646_HP (reg 0x4e != reg 0x38)\n");
      finish_interface (handle);
      return 0;
    }

  finish_interface (handle);

  return "GL646_HP";
}

/* check for the combination of gl660 and gl646 */
static char *
check_gl660_gl646 (struct usb_device *dev)
{
  unsigned char val;
  int result;
  usb_dev_handle *handle;

  if (verbose > 2)
    printf ("    checking for GL660+GL646 ...\n");

  /* Check device descriptor */
  if ((dev->descriptor.bDeviceClass != USB_CLASS_VENDOR_SPEC)
      || (dev->config[0].interface[0].altsetting[0].bInterfaceClass !=
	  USB_CLASS_PER_INTERFACE))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL660+GL646 (bDeviceClass = %d, bInterfaceClass = %d)\n",
	   dev->descriptor.bDeviceClass,
	   dev->config[0].interface[0].altsetting[0].bInterfaceClass);
      return 0;
    }
  if (dev->descriptor.bcdUSB != 0x200)
    {
      if (verbose > 2)
	printf ("    this is not a GL660+GL646 (bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a GL660+GL646 (bDeviceSubClass = 0x%x)\n",
		dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if (dev->descriptor.bDeviceProtocol != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a GL660+GL646 (bDeviceProtocol = 0x%x)\n",
		dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 3)
    {
      if (verbose > 2)
	printf ("    this is not a GL660+GL646 (bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x81)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x02)
      ||
      ((dev->config[0].interface[0].altsetting[0].endpoint[0].
	wMaxPacketSize != 0x40)
       && (dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize != 0x200))
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval !=
	  0x0))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL660+GL646 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[1].
       bEndpointAddress != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  bmAttributes != 0x02)
      ||
      ((dev->config[0].interface[0].altsetting[0].endpoint[1].
	wMaxPacketSize != 0x40)
       && (dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize != 0x200))
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval !=
	  0))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL660+GL646 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[2].
       bEndpointAddress != 0x83)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  bmAttributes != 0x03)
      ||
      ((dev->config[0].interface[0].altsetting[0].endpoint[2].
	wMaxPacketSize != 0x1)
       && (dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize != 0x200))
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval !=
	  8))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL660+GL646 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval);
      return 0;
    }

  result = prepare_interface (dev, &handle);
  if (!result)
    return "GL660+GL646?";

  result = gl646_write_reg (handle, 0x38, 0x15);
  if (!result)
    {
      if (verbose > 2)
	printf ("    this is not a GL660+GL646 (writing register failed)\n");
      finish_interface (handle);
      return 0;
    }

  result = gl646_read_reg (handle, 0x4e, &val);
  if (!result)
    {
      if (verbose > 2)
	printf ("    this is not a GL660+GL646 (reading register failed)\n");
      finish_interface (handle);
      return 0;
    }

  if (val != 0x15)
    {
      if (verbose > 2)
	printf ("    this is not a GL660+GL646 (reg 0x4e != reg 0x38)\n");
      finish_interface (handle);
      return 0;
    }
  finish_interface (handle);
  return "GL660+GL646";
}


/********** the gl841 section **********/

/* the various incarnations could be distinguished by the 
 * bcdDevice entry:
 *    0x701 --> GL124
 *    0x700 --> ?
 *    0x605 --> GL845
 *    0x603 --> GL847
 *    0x601 --> GL846 
 *    0x500 --> GL843 
 *    0x300 --> GL842 (perhaps only >= 0x303 ?)
 *    0x200 --> GL841
 */
static char *
check_gl841 (struct usb_device *dev)
{
  unsigned char val;
  int result;
  usb_dev_handle *handle;

  if (verbose > 2)
    printf ("    checking for GL84x ...\n");

  /* Check device descriptor */
  if ((dev->descriptor.bDeviceClass != USB_CLASS_VENDOR_SPEC)
      || (dev->config[0].interface[0].altsetting[0].bInterfaceClass !=
	  USB_CLASS_VENDOR_SPEC))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL84x (bDeviceClass = %d, bInterfaceClass = %d)\n",
	   dev->descriptor.bDeviceClass,
	   dev->config[0].interface[0].altsetting[0].bInterfaceClass);
      return 0;
    }
  if (dev->descriptor.bcdUSB != 0x200)
    {
      if (verbose > 2)
	printf ("    this is not a GL84x (bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a GL84x (bDeviceSubClass = 0x%x)\n",
		dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if (dev->descriptor.bDeviceProtocol != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a GL84x (bDeviceProtocol = 0x%x)\n",
		dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 3)
    {
      if (verbose > 2)
	printf ("    this is not a GL84x (bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x81)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x02)
      || ((dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize != 0x40) &&
	  (dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize != 0x200))
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval !=
	  0x0))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL84x (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[1].
       bEndpointAddress != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  bmAttributes != 0x02)
      || ((dev->config[0].interface[0].altsetting[0].endpoint[1].
	   wMaxPacketSize != 0x40) &&
	  (dev->config[0].interface[0].altsetting[0].endpoint[1].
	   wMaxPacketSize != 0x200))
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval !=
	  0))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL84x (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[2].
       bEndpointAddress != 0x83)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  bmAttributes != 0x03)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  wMaxPacketSize != 0x1)
      ||
      ((dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval != 8)
       && (dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval !=
	   16)))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL84x (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval);
      return 0;
    }

  result = prepare_interface (dev, &handle);
  if (!result) {
    if (dev->descriptor.bcdDevice == 0x702)
        return "GL124?";
    if (dev->descriptor.bcdDevice == 0x701)
        return "GL124?";
    if (dev->descriptor.bcdDevice >= 0x700)
	return "GL848+?";
    if (dev->descriptor.bcdDevice >= 0x603)
	return "GL847?";
    if (dev->descriptor.bcdDevice >= 0x600)
	return "GL846?";
    if (dev->descriptor.bcdDevice >= 0x500)
	return "GL845?";
    if (dev->descriptor.bcdDevice >= 0x400)
	return "GL843?";
    if (dev->descriptor.bcdDevice >= 0x300)
	return "GL842?";
    else
	return "GL841?";
  }

  result = gl646_write_reg (handle, 0x38, 0x15);
  if (!result)
    {
      if (verbose > 2)
	printf ("    this is not a GL84x (writing register failed)\n");
      finish_interface (handle);
      return 0;
    }

  result = gl646_read_reg (handle, 0x38, &val);
  if (!result)
    {
      if (verbose > 2)
	printf ("    this is not a GL84x (reading register failed)\n");
      finish_interface (handle);
      return 0;
    }

  if (val != 0x15)
    {
      if (verbose > 2)
	printf ("    this is not a GL84x (reg 0x38 != 0x15)\n");
      finish_interface (handle);
      return 0;
    }
  finish_interface (handle);

  if (dev->descriptor.bcdDevice == 0x702)
    return "GL128";
  if (dev->descriptor.bcdDevice == 0x701)
    return "GL124";
  if (dev->descriptor.bcdDevice >= 0x700)
    return "GL848+";
  if (dev->descriptor.bcdDevice >= 0x605)
    return "GL845";
  if (dev->descriptor.bcdDevice >= 0x603)
    return "GL847";
  if (dev->descriptor.bcdDevice >= 0x600)
    return "GL846";
  if (dev->descriptor.bcdDevice >= 0x500)
    return "GL843";
  if (dev->descriptor.bcdDevice >= 0x300)
    return "GL842";
  return "GL841";
}


/********** the icm532b section version 0.2 **********/
/*          no write and read test registers yet     */

static char *
check_icm532b (struct usb_device *dev)
{
  int result;
  usb_dev_handle *handle;

  if (verbose > 2)
    printf ("    checking for ICM532B ...\n");

  /* Check device descriptor */
  if ((dev->descriptor.bDeviceClass != USB_CLASS_VENDOR_SPEC)
      || (dev->config[0].interface[0].altsetting[0].bInterfaceClass != 0xff))
    {
      if (verbose > 2)
	printf
	  ("    this is not a ICM532B (check 1, bDeviceClass = %d, bInterfaceClass = %d)\n",
	   dev->descriptor.bDeviceClass,
	   dev->config[0].interface[0].altsetting[0].bInterfaceClass);
      return 0;
    }
  if (dev->descriptor.bcdUSB != 0x110)
    {
      if (verbose > 2)
	printf ("    this is not a ICM532B (check 2, bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0xff)
    {
      if (verbose > 2)
	printf
	  ("    this is not a ICM532B (check 3, bDeviceSubClass = 0x%x)\n",
	   dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if (dev->descriptor.bDeviceProtocol != 0xff)
    {
      if (verbose > 2)
	printf
	  ("    this is not a ICM532B (check 4, bDeviceProtocol = 0x%x)\n",
	   dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 0x01)
    {
      if (verbose > 2)
	printf ("    this is not a ICM532B (check 5, bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }
  /* Check bEndpointAddress */
  if (dev->config[0].interface[0].altsetting[0].endpoint[0].
      bEndpointAddress != 0x81)
    {
      if (verbose > 2)
	printf
	  ("    this is not a ICM532B (check 6, bEndpointAddress = %d)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress);
      return 0;
    }
  /* Check bmAttributes */
  if (dev->config[0].interface[0].altsetting[0].endpoint[0].
      bmAttributes != 0x01)
    {
      if (verbose > 2)
	printf
	  ("    this is not a ICM532B (check 7, bEndpointAddress = %d)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bmAttributes);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].bAlternateSetting != 0x00)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  wMaxPacketSize != 0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a ICM532B (check 8, bAlternateSetting = 0x%x, wMaxPacketSize = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].bAlternateSetting,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[1].bAlternateSetting != 0x01)
      || (dev->config[0].interface[0].altsetting[1].endpoint[0].
	  wMaxPacketSize != 0x100))
    {
      if (verbose > 2)
	printf
	  ("    this is not a ICM532B (check 9, bAlternatesetting = 0x%x, wMaxPacketSize = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[1].bAlternateSetting,
	   dev->config[0].interface[0].altsetting[1].endpoint[0].
	   wMaxPacketSize);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[2].bAlternateSetting != 0x02)
      || (dev->config[0].interface[0].altsetting[2].endpoint[0].
	  wMaxPacketSize != 0x180))
    {
      if (verbose > 2)
	printf
	  ("    this is not a ICM532B (check 10, bAlternatesetting = 0x%x, wMaxPacketSize = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[2].bAlternateSetting,
	   dev->config[0].interface[0].altsetting[2].endpoint[0].
	   wMaxPacketSize);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[3].bAlternateSetting != 0x03)
      || (dev->config[0].interface[0].altsetting[3].endpoint[0].
	  wMaxPacketSize != 0x200))
    {
      if (verbose > 2)
	printf
	  ("    this is not a ICM532B (check 11, bAlternatesetting = 0x%x, wMaxPacketSize = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[3].bAlternateSetting,
	   dev->config[0].interface[0].altsetting[3].endpoint[0].
	   wMaxPacketSize);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[4].bAlternateSetting != 0x04)
      || (dev->config[0].interface[0].altsetting[4].endpoint[0].
	  wMaxPacketSize != 0x280))
    {
      if (verbose > 2)
	printf
	  ("    this is not a ICM532B (check 12, bAlternatesetting = 0x%x, wMaxPacketSize = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[4].bAlternateSetting,
	   dev->config[0].interface[0].altsetting[4].endpoint[0].
	   wMaxPacketSize);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[5].bAlternateSetting != 0x05)
      || (dev->config[0].interface[0].altsetting[5].endpoint[0].
	  wMaxPacketSize != 0x300))
    {
      if (verbose > 2)
	printf
	  ("    this is not a ICM532B (check 13, bAlternatesetting = 0x%x, wMaxPacketSize = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[5].bAlternateSetting,
	   dev->config[0].interface[0].altsetting[5].endpoint[0].
	   wMaxPacketSize);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[6].bAlternateSetting != 0x06)
      || (dev->config[0].interface[0].altsetting[6].endpoint[0].
	  wMaxPacketSize != 0x380))
    {
      if (verbose > 2)
	printf
	  ("    this is not a ICM532B (check 14, bAlternatesetting = 0x%x, wMaxPacketSize = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[6].bAlternateSetting,
	   dev->config[0].interface[0].altsetting[6].endpoint[0].
	   wMaxPacketSize);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[7].bAlternateSetting != 0x07)
      || (dev->config[0].interface[0].altsetting[7].endpoint[0].
	  wMaxPacketSize != 0x3ff))
    {
      if (verbose > 2)
	printf
	  ("    this is not a ICM532B (check 15, bAlternatesetting = 0x%x, wMaxPacketSize = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[7].bAlternateSetting,
	   dev->config[0].interface[0].altsetting[7].endpoint[0].
	   wMaxPacketSize);
      return 0;
    }

  result = prepare_interface (dev, &handle);
  if (!result)
    return "ICM532B?";

  finish_interface (handle);
  return "ICM532B";
}
/* ====================================== end of icm532b ==================*/


/* Check for the combination of a PowerVision PV8630 (USB->parport bridge)
   and National Semiconductor LM9830 */
static char *
check_pv8630_lm9830 (struct usb_device *dev)
{
  usb_dev_handle *handle;
  int result;
  char data;

  if (verbose > 2)
    printf ("    checking for PV8630/LM9830 ...\n");

  /* Check device descriptor */
  if (dev->descriptor.bDeviceClass != USB_CLASS_PER_INTERFACE)
    {
      if (verbose > 2)
	printf ("    this is not a PV8630/LM9830 (bDeviceClass = %d)\n",
		dev->descriptor.bDeviceClass);
      return 0;
    }
  if (dev->descriptor.bcdUSB != 0x100)
    {
      if (verbose > 2)
	printf ("    this is not a PV8630/LM9830 (bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0x00)
    {
      if (verbose > 2)
	printf ("    this is not a PV8630/LM9830 (bDeviceSubClass = 0x%x)\n",
		dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if (dev->descriptor.bDeviceProtocol != 0x00)
    {
      if (verbose > 2)
	printf ("    this is not a PV8630/LM9830 (bDeviceProtocol = 0x%x)\n",
		dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 3)
    {
      if (verbose > 2)
	printf ("    this is not a PV8630/LM9830 (bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }
  /* Endpoint 0 */
  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x01)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a PV8630/LM9830 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }
  /* Endpoint 1 */
  if ((dev->config[0].interface[0].altsetting[0].endpoint[1].
       bEndpointAddress != 0x82)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a PV8630/LM9830 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval);
      return 0;
    }
  /* Endpoint 2 */
  if ((dev->config[0].interface[0].altsetting[0].endpoint[2].
       bEndpointAddress != 0x83)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  bmAttributes != 0x03)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  wMaxPacketSize != 0x01)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval !=
	  0x01))
    {
      if (verbose > 2)
	printf
	  ("    this is not a PV8630/LM9830 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval);
      return 0;
    }

  /* Now we write/read a register (red offset) */
  result = prepare_interface (dev, &handle);
  if (!result)
    return "PV8630/LM9830?";

  result = 
    usb_control_msg (handle, 0x40, 0x01, 0x38, 0x01, NULL, 0, TIMEOUT);
  if (result < 0)
    {
      if (verbose > 2)
	printf ("    Couldn't send write register number (%s)\n",
		usb_strerror ());
      finish_interface (handle);
      return 0;
    }

  result =
    usb_control_msg (handle, 0x40, 0x01, 0x0f, 0x00,  NULL, 0, TIMEOUT);
  if (result < 0)
    {
      if (verbose > 2)
	printf ("    Couldn't send register data (%s)\n",
		usb_strerror ());
      finish_interface (handle);
      return 0;
    }

  result = 
    usb_control_msg (handle, 0x40, 0x01, 0x38, 0x01, NULL, 0, TIMEOUT);
  if (result < 0)
    {
      if (verbose > 2)
	printf ("    Couldn't send read register number (%s)\n",
		usb_strerror ());
      finish_interface (handle);
      return 0;
    }

  result =
    usb_control_msg (handle, 0xc0, 0x00, 0, 0x00, &data, 1, TIMEOUT);
  if (result <= 0)
    {
      if (verbose > 2)
	printf ("    Couldn't read register data (%s)\n",
		usb_strerror ());
      finish_interface (handle);
      return 0;
    }

  if (data != 0x0f)
    {
      if (verbose > 2)
	printf ("    Data read != data written (%d/%d)\n", data, 0x0f);
      finish_interface (handle);
      return 0;
    }

  finish_interface (handle);
  return "PV8630/LM9830";
}


/* Check for Toshiba M011 */
static char *
check_m011 (struct usb_device *dev)
{
  usb_dev_handle *handle;
  int result;
  char data;

  if (verbose > 2)
    printf ("    checking for M011 ...\n");

  /* Check device descriptor */
  if (dev->descriptor.bDeviceClass != USB_CLASS_VENDOR_SPEC)
    {
      if (verbose > 2)
	printf ("    this is not a M011 (bDeviceClass = %d)\n",
		dev->descriptor.bDeviceClass);
      return 0;
    }
  if (dev->descriptor.bcdUSB != 0x100)
    {
      if (verbose > 2)
	printf ("    this is not a M011 (bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != USB_CLASS_VENDOR_SPEC)
    {
      if (verbose > 2)
	printf ("    this is not a M011 (bDeviceSubClass = 0x%x)\n",
		dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if (dev->descriptor.bDeviceProtocol != USB_CLASS_VENDOR_SPEC)
    {
      if (verbose > 2)
	printf ("    this is not a M011 (bDeviceProtocol = 0x%x)\n",
		dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 1)
    {
      if (verbose > 2)
	printf ("    this is not a M011 (bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }
  /* Endpoint 0 */
  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x82)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a M011 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }

  /* Now we write/read a register (red offset) */
  result = prepare_interface (dev, &handle);
  if (!result)
    return "M011?";

  data = 0x63;

  result = 
    usb_control_msg (handle, 0x40, 0x08, 0x34, 0x00, &data, 1, TIMEOUT);
  if (result < 0)
    {
      if (verbose > 2)
	printf ("    Couldn't write register (%s)\n",
		usb_strerror ());
      finish_interface (handle);
      return 0;
    }
  data = 0;

  result =
    usb_control_msg (handle, 0xc0, 0x00, 0x34, 0x00, &data, 1, TIMEOUT);
  if (result <= 0)
    {
      if (verbose > 2)
	printf ("    Couldn't read register (%s)\n",
		usb_strerror ());
      finish_interface (handle);
      return 0;
    }

  if (data != 0x63)
    {
      if (verbose > 2)
	printf ("    Data read != data written (%d/%d)\n", data, 0x63);
      finish_interface (handle);
      return 0;
    }

  finish_interface (handle);
  return "M011";
}


/* Check for Realtek rts88xx */
static int
rts88xx_read_reg (usb_dev_handle * handle, unsigned char *req, unsigned char *res, int size)
{
  int result;

  result =
    usb_bulk_write (handle, 0x02, (char *)req, 0x04, TIMEOUT);
  if (result < 0)
    return 0;

  result =
    usb_bulk_read (handle,  0x81, (char *)res, size, TIMEOUT);
  if (result < 0)
    return 0;

  return 1;
}

static char *
check_rts8858c (struct usb_device *dev)
{
  unsigned char req[4];
  unsigned char res[10];
  usb_dev_handle *handle;
  int result;

  if (verbose > 2)
    printf ("    checking for rts8858c ...\n");

  /* Check device descriptor */
  if (dev->descriptor.bDeviceClass != 0)
    {
      if (verbose > 2)
	printf ("    this is not a rts8858c (bDeviceClass = %d)\n",
		dev->descriptor.bDeviceClass);
      return 0;
    }
  if (dev->descriptor.bcdUSB != 0x110)
    {
      if (verbose > 2)
	printf ("    this is not a rts8858c (bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0)
    {
      if (verbose > 2)
	printf ("    this is not a rts8858c (bDeviceSubClass = 0x%x)\n",
		dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if (dev->descriptor.bDeviceProtocol != 0)
    {
      if (verbose > 2)
	printf ("    this is not a rts8858c (bDeviceProtocol = 0x%x)\n",
		dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 3)
    {
      if (verbose > 2)
	printf ("    this is not a rts8858c (bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x81)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a rts8858c (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[1].
       bEndpointAddress != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  wMaxPacketSize != 0x08)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a rts8858c (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[2].
       bEndpointAddress != 0x83)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  bmAttributes != 0x03)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  wMaxPacketSize != 0x01)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval !=
	  0xFA))
    {
      if (verbose > 2)
	printf
	  ("    this is not a rts8858c (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval);
      return 0;
    }

  /* Now we read 10 registers */
  result = prepare_interface (dev, &handle);
  if (!result)
    return "rts8858c?";

  memset (req, 0, 4);
  req[0] = 0x80;		/* get registers 0x12-0x1c */
  req[1] = 0x12;
  req[2] = 0x00;
  req[3] = 0x0a;

  result = rts88xx_read_reg(handle,req,res,req[3]);
  if (result <= 0)
    {
      if (verbose > 2)
	printf ("    Couldn't read registers\n");
      finish_interface (handle);
      return 0;
    }

  if (res[1] != 0x03 || res[2] != 0x00)
    {
      if (verbose > 2)
	printf ("    Unexpected result from register reading (0x%0x/0x%0x)\n",
		res[1], res[2]);
      finish_interface (handle);
      return 0;
    }
  finish_interface (handle);
  return "rts8858c";
}	/* end of rts8858 detection */

static char *
check_rts88x1 (struct usb_device *dev)
{
  unsigned char req[4];
  unsigned char res[243];
  usb_dev_handle *handle;
  int result;

  if (verbose > 2)
    printf ("    checking for rts8801/rts8891 ...\n");

  /* Check device descriptor */
  if (dev->descriptor.bDeviceClass != 0)
    {
      if (verbose > 2)
	printf ("    this is not a rts8801/rts8891 (bDeviceClass = %d)\n",
		dev->descriptor.bDeviceClass);
      return 0;
    }
  if (dev->descriptor.bcdUSB != 0x110)
    {
      if (verbose > 2)
	printf ("    this is not a rts8801/rts8891 (bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0)
    {
      if (verbose > 2)
	printf ("    this is not a rts8801/rts8891 (bDeviceSubClass = 0x%x)\n",
		dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if (dev->descriptor.bDeviceProtocol != 0)
    {
      if (verbose > 2)
	printf ("    this is not a rts8801/rts8891 (bDeviceProtocol = 0x%x)\n",
		dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 3)
    {
      if (verbose > 2)
	printf ("    this is not a rts8801/rts8891 (bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x81)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  wMaxPacketSize != 0x40)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a rts8801/rts8891 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[1].
       bEndpointAddress != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  wMaxPacketSize != 0x08)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a rts8801/rts8891 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[2].
       bEndpointAddress != 0x83)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  bmAttributes != 0x03)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  wMaxPacketSize != 0x01)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval !=
	  0xFA))
    {
      if (verbose > 2)
	printf
	  ("    this is not a rts8801/rts8891 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval);
      return 0;
    }

  /* Now we read 10 registers */
  result = prepare_interface (dev, &handle);
  if (!result)
    return "rts8801/rts8891?";

  memset (req, 0, 4);
  req[0] = 0x80;		/* get registers 0x00-0xF2 */
  req[1] = 0x00;
  req[2] = 0x00;
  req[3] = 0xf3;

  result = rts88xx_read_reg(handle,req,res,req[3]);
  if (result <= 0)
    {
      if (verbose > 2)
	printf ("    Couldn't read registers\n");
      finish_interface (handle);
      return 0;
    }

  /* test CCD and link registers */
  if (res[0xb0] != 0x80 || ((res[0] & 0x0f)!=0x05))
    {
      if (verbose > 2)
	printf ("    Unexpected result from register reading (0x%0x/0x%0x)\n",
		res[0xb0], res[2]);
      finish_interface (handle);
      return 0;
    }
  finish_interface (handle);
  return "rts8801/rts8891";
}	/* end of rts8801/rts8891 detection */


/* Check for Service & Quality SQ113 */
static char *
check_sq113 (struct usb_device *dev)
{
  usb_dev_handle *handle;
  int result;
  unsigned char data;
  unsigned char buffer[4];

  if (verbose > 2)
    printf ("    checking for SQ113 ...\n");

  /* Check device descriptor */
  if (dev->descriptor.bDeviceClass != 0)
    {
      if (verbose > 2)
	printf ("    this is not a SQ113 (bDeviceClass = %d)\n",
		dev->descriptor.bDeviceClass);
      return 0;
    }
  if (dev->descriptor.bcdUSB != 0x200)
    {
      if (verbose > 2)
	printf ("    this is not a SQ113 (bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0)
    {
      if (verbose > 2)
	printf ("    this is not a SQ113 (bDeviceSubClass = 0x%x)\n",
		dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if (dev->descriptor.bDeviceProtocol != 0)
    {
      if (verbose > 2)
	printf ("    this is not a SQ113 (bDeviceProtocol = 0x%x)\n",
		dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check interface */
  if (dev->config[0].interface[0].altsetting[0].bInterfaceClass != 255)
    {
      if (verbose > 2)
	printf ("    this is not a SQ113 (bInterfaceClass = %d)\n",
		dev->config[0].interface[0].altsetting[0].bInterfaceClass);
      return 0;
    }

  if (dev->config[0].interface[0].altsetting[0].bInterfaceSubClass != 255)
    {
      if (verbose > 2)
	printf ("    this is not a SQ113 (bInterfaceSubClass = %d)\n",
		dev->config[0].interface[0].altsetting[0].bInterfaceSubClass);
      return 0;
    }
  if (dev->config[0].interface[0].altsetting[0].bInterfaceProtocol != 255)
    {
      if (verbose > 2)
	printf ("    this is not a SQ113 (bInterfaceProtocol = %d)\n",
		dev->config[0].interface[0].altsetting[0].bInterfaceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 3)
    {
      if (verbose > 2)
	printf ("    this is not a SQ113 (bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }
  /* Endpoint 0 */
  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x01)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x02)
      || ((dev->config[0].interface[0].altsetting[0].endpoint[0].
	  wMaxPacketSize != 0x40)
	  && (dev->config[0].interface[0].altsetting[0].endpoint[0].
	      wMaxPacketSize != 0x200))
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a SQ113 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }

  /* Endpoint 1 */
  if ((dev->config[0].interface[0].altsetting[0].endpoint[1].
       bEndpointAddress != 0x82)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  bmAttributes != 0x02)
      || ((dev->config[0].interface[0].altsetting[0].endpoint[1].
	  wMaxPacketSize != 0x40)
	  && (dev->config[0].interface[0].altsetting[0].endpoint[1].
	      wMaxPacketSize != 0x200))
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a SQ113 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval);
      return 0;
    }
  /* Endpoint 2 */
  if ((dev->config[0].interface[0].altsetting[0].endpoint[2].
       bEndpointAddress != 0x83)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  bmAttributes != 0x03)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  wMaxPacketSize != 0x1)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval !=
	  0x03))
    {
      if (verbose > 2)
	printf
	  ("    this is not a SQ113 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval);
      return 0;
    }

  /* Now we read the status register */
  result = prepare_interface (dev, &handle);
  if (!result)
    return "SQ113?";
  
  buffer [0] = 0x5f;
  buffer [1] = 0x00;
  buffer [2] = 0x5f;
  buffer [3] = 0x00;

  result = 
    usb_control_msg (handle, 0x40, 0x01, 0xb0, 0, (char *) buffer, 4, TIMEOUT);
  if (result < 0)
    {
      if (verbose > 2)
	printf ("    Couldn't set bank (%s)\n",
		usb_strerror ());
      finish_interface (handle);
      return 0;
    }

  data = 0x00;

  buffer [0] = 0x8b;
  buffer [1] = data;
  buffer [2] = 0x8b;
  buffer [3] = data;

  result = 
    usb_control_msg (handle, 0x40, 0x01, 0xb0, 0, (char *) buffer, 4, TIMEOUT);
  if (result < 0)
    {
      if (verbose > 2)
	printf ("    Couldn't write register (%s)\n",
		usb_strerror ());
      finish_interface (handle);
      return 0;
    }

  buffer [0] = 0x8b;
  buffer [1] = 0x8b;
  buffer [2] = 0x8b;
  buffer [3] = 0x8b;
  result = 
    usb_control_msg (handle, 0x40, 0x01, 0x04, 0x8b, (char *) buffer, 4, TIMEOUT);
  if (result < 0)
    {
      if (verbose > 2)
	printf ("    Couldn't set read register address (%s)\n",
		usb_strerror ());
      finish_interface (handle);
      return 0;
    }

  result = 
    usb_control_msg (handle, 0xc0, 0x01, 0x07, 0, (char *) buffer, 4, TIMEOUT);
  if (result < 0)
    {
      if (verbose > 2)
	printf ("    Couldn't read register (%s)\n",
		usb_strerror ());
      finish_interface (handle);
      return 0;
    }

  if ((buffer[0] & 0x10) != 0x10)
    {
      if (verbose > 2)
	printf ("    Sensor not home? (0x%02x)\n", buffer[0]);
      finish_interface (handle);
      return 0;
    }

  finish_interface (handle);
  return "SQ113";
}

/* Check for Realtek RTS8822 chipset*/
static char *
check_rts8822 (struct usb_device *dev)
{
  char data[2];
  usb_dev_handle *handle;
  int result;

  if (verbose > 2)
    printf ("    checking for RTS8822 ...\n");

  /* Check device descriptor */
  if (dev->descriptor.bDeviceClass != 0)
    {
      if (verbose > 2)
	printf ("    this is not a RTS8822 (bDeviceClass = %d)\n",
		dev->descriptor.bDeviceClass);
      return 0;
    }
  if ((dev->descriptor.bcdUSB != 0x200)&&(dev->descriptor.bcdUSB != 0x110))
    {
      if (verbose > 2)
	printf ("    this is not a RTS8822 (bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0)
    {
      if (verbose > 2)
	printf ("    this is not a RTS8822 (bDeviceSubClass = 0x%x)\n",
		dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if (dev->descriptor.bDeviceProtocol != 0)
    {
      if (verbose > 2)
	printf ("    this is not a RTS8822 (bDeviceProtocol = 0x%x)\n",
		dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 3)
    {
      if (verbose > 2)
	printf ("    this is not a RTS8822 (bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x81)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  wMaxPacketSize != 0x200)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a RTS8822 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[1].
       bEndpointAddress != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  wMaxPacketSize != 0x200)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a RTS8822 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[2].
       bEndpointAddress != 0x83)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  bmAttributes != 0x03)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  wMaxPacketSize != 0x01)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval !=
	  0x0c))
    {
      if (verbose > 2)
	printf
	  ("    this is not a RTS8822 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval);
      return 0;
    }

  /* Now we read 1 register */
  result = prepare_interface (dev, &handle);
  if (!result)
    return "RTS8822?";

  memset (data, 0, 2);

  result =
    usb_control_msg(handle, 0xc0, 0x04, 0xfe11, 0x100, data, 0x02, TIMEOUT);

  if (result <= 0)
    {
      if (verbose > 2)
	printf ("    Couldn't send read control message (%s)\n",
		strerror (errno));
      finish_interface (handle);
      return 0;
    }

  if ((data[0] == 0)&&(data[1] == 0))
    {
      if (verbose > 2)
	printf ("    Unexpected result from register 0xfe11 : 0x%0x%0x\n",
		data[1], data[0]);
      finish_interface (handle);
      return 0;
    }
  finish_interface (handle);
  return "RTS8822";
}	/* end of RTS8822 detection */

/* Check for Silitek chipset found in HP ScanJet 4500C/4570C/5500C/5550C/5590/7650 scanners */
static char *
check_hp5590 (struct usb_device *dev)
{
  usb_dev_handle	*handle;
  int			result;
  uint8_t		status;
  struct usb_ctrl_setup ctrl;
  uint8_t		data[0x32];
  uint8_t		ack;
  uint8_t		*ptr;
  int			next_packet_size;
  unsigned int		len;
#define HP5590_NAMES	"HP4500C/4570C/5500C/5550C/5590/7650"

  if (verbose > 2)
    printf ("    checking for " HP5590_NAMES " chipset ...\n");

  /* Check device descriptor */
  if (dev->descriptor.bDeviceClass != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a " HP5590_NAMES " chipset (bDeviceClass = %d)\n",
		dev->descriptor.bDeviceClass);
      return 0;
    }
  if (dev->descriptor.bcdUSB != 0x200)
    {
      if (verbose > 2)
	printf ("    this is not a " HP5590_NAMES " chipset (bcdUSB = 0x%x)\n",
		dev->descriptor.bcdUSB);
      return 0;
    }
  if (dev->descriptor.bDeviceSubClass != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a " HP5590_NAMES " chipset (bDeviceSubClass = 0x%x)\n",
		dev->descriptor.bDeviceSubClass);
      return 0;
    }
  if (dev->descriptor.bDeviceProtocol != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a " HP5590_NAMES " chipset (bDeviceProtocol = 0x%x)\n",
		dev->descriptor.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (dev->config[0].interface[0].altsetting[0].bNumEndpoints != 3)
    {
      if (verbose > 2)
	printf ("    this is not a " HP5590_NAMES " chipset (bNumEndpoints = %d)\n",
		dev->config[0].interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }
  if ((dev->config[0].interface[0].altsetting[0].endpoint[0].
       bEndpointAddress != 0x81)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].
	  wMaxPacketSize != 0x200)
      || (dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a " HP5590_NAMES " chipset (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[1].
       bEndpointAddress != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  bmAttributes != 0x02)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].
	  wMaxPacketSize != 0x200)
      || (dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval !=
	  0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a " HP5590_NAMES " chipset (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[1].bInterval);
      return 0;
    }

  if ((dev->config[0].interface[0].altsetting[0].endpoint[2].
       bEndpointAddress != 0x83)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  bmAttributes != 0x03)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].
	  wMaxPacketSize != 0x01)
      || (dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval !=
	  0x08))
    {
      if (verbose > 2)
	printf
	  ("    this is not a " HP5590_NAMES " chipset (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   bEndpointAddress,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bmAttributes,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].
	   wMaxPacketSize,
	   dev->config[0].interface[0].altsetting[0].endpoint[2].bInterval);
      return 0;
    }

  result = prepare_interface (dev, &handle);
  if (!result)
    return NULL;

  /* USB-in-USB command URB - command 0x0012 (init scanner), returns 0x32 bytes */
  memset (&ctrl, 0, sizeof(ctrl));
  ctrl.bRequestType = 0xc0;
  ctrl.bRequest = 0x04;
  ctrl.wValue = 0x1200;
  ctrl.wIndex = 0x00;
  ctrl.wLength = sizeof(data);
  result = usb_control_msg (handle, USB_ENDPOINT_OUT | USB_TYPE_VENDOR,
			    0x04, 0x8f, 0x00,
			    (char *) &ctrl, sizeof (ctrl), TIMEOUT);
  if (result < 0)
    {
      if (verbose > 2)
	printf ("    Couldn't send send USB-in-USB command (%s)\n",
		strerror (errno));
      return NULL;
    }

  /* Get confirmation for USB-in-USB command */
  result = usb_control_msg (handle, USB_ENDPOINT_IN | USB_TYPE_VENDOR, 
			    0x0c, 0x8e, 0x20,
			    (char *) &status, sizeof(status), TIMEOUT);
  if (result < 0)
    {
      if (verbose > 2)
	printf ("    Couldn't read USB-in-USB confirmation (%s)\n",
		strerror (errno));
      finish_interface (handle);
      return NULL;
    }

  /* Check the confirmation received */
  if (status != 0x01)
    {
      if (verbose > 2)
	printf ("    Didn't get correct confirmation for USB-in-USB command "
		"(needed: 0x01, got: 0x%02x\n",
		status);
      finish_interface (handle);
      return NULL;
    }

  /* Read data in 8 byte packets */
  ptr = data;
  len = sizeof(data);
  while (len)
    {
      next_packet_size = 8;
      if (len < 8)
	next_packet_size = len;

      /* Read data packet */
      result = usb_control_msg (handle, USB_ENDPOINT_IN | USB_TYPE_VENDOR,
				0x04, 0x90, 0x00,
				(char *) ptr, next_packet_size, TIMEOUT);
      if (result < 0)
	{
	  if (verbose > 2)
	    printf ("    Couldn't read USB-in-USB data (%s)\n",
		    strerror (errno));
	  finish_interface (handle);
	  return NULL;
	}

      /* Check if all of the requested data was received */
      if (result != next_packet_size)
	{
	  if (verbose > 2)
	    printf ("    Incomplete USB-in-USB data received (needed: %u, got: %u)\n",
		    next_packet_size, result);
	  finish_interface (handle);
	  return NULL;
	}

      ptr += next_packet_size;
      len -= next_packet_size;
    }

  /* Acknowledge the whole received data */
  ack = 0;
  result = usb_control_msg (handle, USB_ENDPOINT_OUT | USB_TYPE_VENDOR,
			    0x0c, 0x8f, 0x00,
			    (char *) &ack, sizeof(ack), TIMEOUT);
  if (result < 0)
    {
      if (verbose > 2)
	printf ("    Couldn't send USB-in-USB data confirmation (%s)\n",
		strerror (errno));
      finish_interface (handle);
      return NULL;
    }

  /* Get confirmation for acknowledge */
  result = usb_control_msg (handle, USB_ENDPOINT_IN | USB_TYPE_VENDOR,
			    0x0c, 0x8e, 0x20,
			    (char *) &status, sizeof(status), TIMEOUT);
  if (result < 0)
    {
      if (verbose > 2)
	printf ("    Couldn't read USB-in-USB confirmation for data confirmation (%s)\n",
		strerror (errno));
      finish_interface (handle);
      return NULL;
    }

  /* Check the confirmation received */
  if (status != 0x01)
    {
      if (verbose > 2)
	printf ("    Didn't get correct confirmation for USB-in-USB command "
		"(needed: 0x01, got: 0x%02x\n",
		status);
      finish_interface (handle);
      return NULL;
    }

  /* Check vendor ID */
  if (memcmp (data+1, "SILITEK", 7) != 0)
    {
      if (verbose > 2)
	printf ("   Incorrect product ID received\n");
      finish_interface (handle);
      return NULL;
    }

  finish_interface (handle);
  return HP5590_NAMES;
}

char *
check_usb_chip (struct usb_device *dev, int verbosity, SANE_Bool from_file)
{
  char *chip_name = 0;

  verbose = verbosity;
  no_chipset_access = from_file;

  if (verbose > 2)
    printf ("\n<trying to find out which USB chip is used>\n");

  chip_name = check_gt6801 (dev);

  if (!chip_name)
    chip_name = check_gt6816 (dev);

  if (!chip_name)
    chip_name = check_gt8911 (dev);

  if (!chip_name)
    chip_name = check_ma1017 (dev);

  if (!chip_name)
    chip_name = check_ma1015 (dev);

  if (!chip_name)
    chip_name = check_ma1509 (dev);

  if (!chip_name)
    chip_name = check_merlin (dev);

  if (!chip_name)
    chip_name = check_gl646 (dev);

  if (!chip_name)
    chip_name = check_gl646_hp (dev);

  if (!chip_name)
    chip_name = check_gl660_gl646 (dev);

  if (!chip_name)
    chip_name = check_gl841 (dev);

  if (!chip_name)
    chip_name = check_icm532b (dev);

  if (!chip_name)
    chip_name = check_pv8630_lm9830 (dev);

  if (!chip_name)
    chip_name = check_m011 (dev);

  if (!chip_name)
    chip_name = check_rts8822 (dev);

  if (!chip_name)
    chip_name = check_rts8858c (dev);

  if (!chip_name)
    chip_name = check_sq113 (dev);

  if (!chip_name)
    chip_name = check_hp5590 (dev);

  if (!chip_name)
    chip_name = check_rts88x1 (dev);

  if (verbose > 2)
    {
      if (chip_name)
	printf ("<This USB chip looks like a %s (result from %s)>\n\n",
		chip_name, PACKAGE_STRING);
      else
	printf ("<Couldn't determine the type of the USB chip (result from %s)>\n\n",
		PACKAGE_STRING);
    }

  return chip_name;
}

#endif /* HAVE_LIBUSB */

#ifdef HAVE_LIBUSB_1_0

#include <libusb.h>

#include "../include/sane/sane.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "../include/_stdint.h"

#define TIMEOUT 1000


/** @brief detect used chip
 *
 * Try to detect the chip used by a device that looks like a scanner by
 * trying some basic operation like reading/writing registers.
 * @param dev libusb_device to probe
 * @param verbosity level of messages verbosity
 * @param from_file SANE_True if data read from file
 * @return a string containing the name of the detected scanner chip
 */
char *check_usb_chip (int verbosity,
		      struct libusb_device_descriptor desc,
		      libusb_device_handle * hdl,
		      struct libusb_config_descriptor *config0);

static int verbose = 0;

static int
genesys_write_reg (libusb_device_handle * handle, unsigned char reg,
		   unsigned char val)
{
  int result;
  unsigned char data[2];

  data[0] = reg;
  data[1] = val;

  result = libusb_control_transfer (handle,
				    0x40,
				    0x04, 0x83, 0x00, data, 0x02, TIMEOUT);
  if (result < 0)
    return 0;
  return 1;
}

static int
genesys_read_reg (libusb_device_handle * handle, unsigned char reg,
		  unsigned char *val)
{
  int result;

  result = libusb_control_transfer (handle,
				    0x40,
				    0x0c, 0x83, 0x00, &reg, 0x01, TIMEOUT);
  if (result < 0)
    return 0;

  result = libusb_control_transfer (handle,
				    0xc0,
				    0x0c, 0x84, 0x00, val, 0x01, TIMEOUT);
  if (result < 0)
    return 0;

  return 1;
}

/** @brief check for genesys GL646 chips
 *
 * @param dev libusb device
 * @param hdl libusb opened handle
 * @param config0 configuration 0 from get config _descriptor
 * @return a string with ASIC name, or NULL if not recognized
 */
static char *
check_gl646 (libusb_device_handle * handle,
	     struct libusb_device_descriptor desc,
	     struct libusb_config_descriptor *config0)
{
  unsigned char val;
  int result;

  if (desc.bcdUSB != 0x110)
    {
      if (verbose > 2)
	printf ("    this is not a GL646 (bcdUSB = 0x%x)\n", desc.bcdUSB);
      return 0;
    }
  if (desc.bDeviceSubClass != 0x00)
    {
      if (verbose > 2)
	printf ("    this is not a GL646 (bDeviceSubClass = 0x%x)\n",
		desc.bDeviceSubClass);
      return 0;
    }
  if (desc.bDeviceProtocol != 0)
    {
      if (verbose > 2)
	printf ("    this is not a GL646 (bDeviceProtocol = 0x%x)\n",
		desc.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (config0->interface[0].altsetting[0].bNumEndpoints != 3)
    {
      if (verbose > 2)
	printf ("    this is not a GL646 (bNumEndpoints = %d)\n",
		config0->interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }

  if ((config0->interface[0].altsetting[0].endpoint[0].bEndpointAddress !=
       0x81)
      || (config0->interface[0].altsetting[0].endpoint[0].bmAttributes !=
	  0x02)
      || (config0->interface[0].altsetting[0].endpoint[0].wMaxPacketSize !=
	  0x40)
      || (config0->interface[0].altsetting[0].endpoint[0].bInterval != 0x0))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL646 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   config0->interface[0].altsetting[0].endpoint[0].bEndpointAddress,
	   config0->interface[0].altsetting[0].endpoint[0].bmAttributes,
	   config0->interface[0].altsetting[0].endpoint[0].wMaxPacketSize,
	   config0->interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }

  if ((config0->interface[0].altsetting[0].endpoint[1].bEndpointAddress !=
       0x02)
      || (config0->interface[0].altsetting[0].endpoint[1].bmAttributes !=
	  0x02)
      || (config0->interface[0].altsetting[0].endpoint[1].wMaxPacketSize !=
	  0x40)
      || (config0->interface[0].altsetting[0].endpoint[1].bInterval != 0))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL646 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   config0->interface[0].altsetting[0].endpoint[1].bEndpointAddress,
	   config0->interface[0].altsetting[0].endpoint[1].bmAttributes,
	   config0->interface[0].altsetting[0].endpoint[1].wMaxPacketSize,
	   config0->interface[0].altsetting[0].endpoint[1].bInterval);
      return 0;
    }

  if ((config0->interface[0].altsetting[0].endpoint[2].bEndpointAddress !=
       0x83)
      || (config0->interface[0].altsetting[0].endpoint[2].bmAttributes !=
	  0x03)
      || (config0->interface[0].altsetting[0].endpoint[2].wMaxPacketSize !=
	  0x1)
      || (config0->interface[0].altsetting[0].endpoint[2].bInterval != 8))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GL646 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   config0->interface[0].altsetting[0].endpoint[2].bEndpointAddress,
	   config0->interface[0].altsetting[0].endpoint[2].bmAttributes,
	   config0->interface[0].altsetting[0].endpoint[2].wMaxPacketSize,
	   config0->interface[0].altsetting[0].endpoint[2].bInterval);
      return 0;
    }

  result = genesys_write_reg (handle, 0x38, 0x15);
  if (!result)
    {
      if (verbose > 2)
	printf ("    this is not a GL646 (writing register failed)\n");
      return 0;
    }

  result = genesys_read_reg (handle, 0x4e, &val);
  if (!result)
    {
      if (verbose > 2)
	printf ("    this is not a GL646 (reading register failed)\n");
      return 0;
    }

  if (val != 0x15)
    {
      if (verbose > 2)
	printf ("    this is not a GL646 (reg 0x4e != reg 0x38)\n");
      return 0;
    }

  return "GL646";
}

/** @brief check for gt6801 chip
 *
 * @param dev libusb device
 * @param hdl libusb opened handle
 * @param config0 configuration 0 from get config _descriptor
 * @return a string with ASIC name, or NULL if not recognized
 */
static char *
check_gt6801 (libusb_device_handle * handle,
	       struct libusb_device_descriptor desc,
	       struct libusb_config_descriptor *config0)
{
  unsigned char req[64];
  int result;

  if (verbose > 2)
    printf ("    checking for GT-6801 ...\n");

  /* Check device descriptor */
  if (desc.bDeviceClass != LIBUSB_CLASS_VENDOR_SPEC)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6801 (bDeviceClass = %d)\n",
		desc.bDeviceClass);
      return 0;
    }
  if (desc.bcdUSB != 0x110)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6801 (bcdUSB = 0x%x)\n", desc.bcdUSB);
      return 0;
    }
  if (desc.bDeviceSubClass != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6801 (bDeviceSubClass = 0x%x)\n",
		desc.bDeviceSubClass);
      return 0;
    }
  if (desc.bDeviceProtocol != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6801 (bDeviceProtocol = 0x%x)\n",
		desc.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (config0->interface[0].altsetting[0].bNumEndpoints != 1)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6801 (bNumEndpoints = %d)\n",
		config0->interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }
  if ((config0->interface[0].altsetting[0].endpoint[0].bEndpointAddress != 0x81)
      || (config0->interface[0].altsetting[0].endpoint[0].bmAttributes != 0x02)
      || (config0->interface[0].altsetting[0].endpoint[0].wMaxPacketSize != 0x40)
      || (config0->interface[0].altsetting[0].endpoint[0].bInterval != 0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GT-6801 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   config0->interface[0].altsetting[0].endpoint[0].bEndpointAddress,
	   config0->interface[0].altsetting[0].endpoint[0].bmAttributes,
	   config0->interface[0].altsetting[0].endpoint[0].wMaxPacketSize,
	   config0->interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }

  /* Now we send a control message */

  memset (req, 0, 64);
  req[0] = 0x2e;		/* get identification information */
  req[1] = 0x01;

  result = libusb_control_transfer (handle, 0x40, 0x01, 0x2010, 0x3f40, req, 64, TIMEOUT);
  if (result <= 0)
    {
      if (verbose > 2)
	printf ("    Couldn't send write control message (%s)\n",
		strerror (errno));
      return NULL;
    }
  result = libusb_control_transfer (handle, 0xc0, 0x01, 0x2011, 0x3f00, req, 64, TIMEOUT);
  if (result <= 0)
    {
      if (verbose > 2)
	printf ("    Couldn't send read control message (%s)\n",
		strerror (errno));
      return NULL;
    }
  if (req[0] != 0 || (req[1] != 0x2e && req[1] != 0))
    {
      if (verbose > 2)
	printf ("    Unexpected result from control message (%0x/%0x)\n",
		req[0], req[1]);
      return NULL;
    }
  return "GT-6801";
}

/** @brief check for gt6816 chip
 *
 * @param dev libusb device
 * @param hdl libusb opened handle
 * @param config0 configuration 0 from get config _descriptor
 * @return a string with ASIC name, or NULL if not recognized
 */
static char *
check_gt6816 (libusb_device_handle * handle,
	       struct libusb_device_descriptor desc,
	       struct libusb_config_descriptor *config0)
{
  unsigned char req[64];
  int result,i;

  if (verbose > 2)
    printf ("    checking for GT-6816 ...\n");

  /* Check device descriptor */
  if ((desc.bDeviceClass != LIBUSB_CLASS_PER_INTERFACE)
      || (config0->interface[0].altsetting[0].bInterfaceClass != LIBUSB_CLASS_VENDOR_SPEC))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GT-6816 (bDeviceClass = %d, bInterfaceClass = %d)\n",
	   desc.bDeviceClass,
	   config0->interface[0].altsetting[0].bInterfaceClass);
      return 0;
    }
  if (desc.bcdUSB != 0x110)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6816 (bcdUSB = 0x%x)\n", desc.bcdUSB);
      return 0;
    }
  if (desc.bDeviceSubClass != 0x00)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6816 (bDeviceSubClass = 0x%x)\n",
		desc.bDeviceSubClass);
      return 0;
    }
  if (desc.bDeviceProtocol != 0x00)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6816 (bDeviceProtocol = 0x%x)\n",
		desc.bDeviceProtocol);
      return 0;
    }

  if (config0->bNumInterfaces != 0x01)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6816 (bNumInterfaces = 0x%x)\n",
		config0->bNumInterfaces);
      return 0;
    }

  /* Check endpoints */
  if (config0->interface[0].altsetting[0].bNumEndpoints != 2)
    {
      if (verbose > 2)
	printf ("    this is not a GT-6816 (bNumEndpoints = %d)\n",
		config0->interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }
  if ((config0->interface[0].altsetting[0].endpoint[0].bEndpointAddress != 0x81)
      || (config0->interface[0].altsetting[0].endpoint[0].bmAttributes != 0x02)
      || (config0->interface[0].altsetting[0].endpoint[0].wMaxPacketSize != 0x40)
      || (config0->interface[0].altsetting[0].endpoint[0].bInterval != 0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GT-6816 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   config0->interface[0].altsetting[0].endpoint[0].bEndpointAddress,
	   config0->interface[0].altsetting[0].endpoint[0].bmAttributes,
	   config0->interface[0].altsetting[0].endpoint[0].wMaxPacketSize,
	   config0->interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }
  if ((config0->interface[0].altsetting[0].endpoint[1].bEndpointAddress != 0x02)
      || (config0->interface[0].altsetting[0].endpoint[1].bmAttributes != 0x02)
      || (config0->interface[0].altsetting[0].endpoint[1].wMaxPacketSize != 0x40)
      || (config0->interface[0].altsetting[0].endpoint[1].bInterval != 0x00))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GT-6816 (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   config0->interface[0].altsetting[0].endpoint[1].bEndpointAddress,
	   config0->interface[0].altsetting[0].endpoint[1].bmAttributes,
	   config0->interface[0].altsetting[0].endpoint[1].wMaxPacketSize,
	   config0->interface[0].altsetting[0].endpoint[1].bInterval);
      return 0;
    }

  /* Now we send a control message */

  memset (req, 0, 64);
  for (i = 0; i < 8; i++)
    {
      req[8 * i + 0] = 0x73;	/* check firmware */
      req[8 * i + 1] = 0x01;
    }

  result = libusb_control_transfer (handle, 0x40, 0x04, 0x2012, 0x3f40, req, 64, TIMEOUT);
  if (result <= 0)
    {
      if (verbose > 2)
	printf ("    Couldn't send write control message (%s)\n",
		strerror (errno));
      return NULL;
    }
  result = libusb_control_transfer (handle, 0xc0, 0x01, 0x2013, 0x3f00, req, 64, TIMEOUT);
  if (result <= 0)
    {
      /* Before firmware upload, 64 bytes are returned. Some libusb
	 implementations/operating systems can't seem to cope with short
	 packets. */
      result = libusb_control_transfer (handle, 0xc0, 0x01, 0x2013, 0x3f00, req, 8, TIMEOUT);
      if (result <= 0)
        {
          if (verbose > 2)
	    printf ("    Couldn't send read control message (%s)\n",
		    strerror (errno));
          return NULL;
        }
    }
  if (req[0] != 0)
    {
      if (verbose > 2)
	printf ("    Unexpected result from control message (%0x/%0x)\n",
		req[0], req[1]);
      return NULL;
    }
  return "GT-6816";
}

/** @brief check for known genesys chip
 *
 * Try to check if the scanner use a known genesys ASIC.
 * The various incarnations could be distinguished by the 
 * bcdDevice entry:
 *    0x701 --> GL124
 *    0x700 --> ?
 *    0x605 --> GL845
 *    0x603 --> GL847
 *    0x601 --> GL846 
 *    0x500 --> GL843 
 *    0x300 --> GL842 (perhaps only >= 0x303 ?)
 *    0x200 --> GL841
 *
 * @param dev libusb device
 * @param hdl libusb opened handle
 * @param config0 configuration 0 from get config _descriptor
 * @return a string with ASIC name, or NULL if not recognized
 */
static char *
check_genesys (libusb_device_handle * handle,
	       struct libusb_device_descriptor desc,
	       struct libusb_config_descriptor *config0)
{
  unsigned char val;
  int result;

  if (verbose > 2)
    printf ("    checking for GLxxx ...\n");

  /* Check GL646 device descriptor */
  if ((desc.bDeviceClass == LIBUSB_CLASS_PER_INTERFACE)
      && (config0->interface[0].altsetting[0].bInterfaceClass == 0x10))
    {
      return check_gl646 (handle, desc, config0);
    }
  if (verbose > 2)
    {
      printf
	("    this is not a GL646 (bDeviceClass = %d, bInterfaceClass = %d)\n",
	 desc.bDeviceClass,
	 config0->interface[0].altsetting[0].bInterfaceClass);
    }

  /* Check device desc */
  if ((desc.bDeviceClass != LIBUSB_CLASS_VENDOR_SPEC)
      || (config0->interface[0].altsetting[0].bInterfaceClass !=
	  LIBUSB_CLASS_VENDOR_SPEC))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GLxxx (bDeviceClass = %d, bInterfaceClass = %d)\n",
	   desc.bDeviceClass,
	   config0->interface[0].altsetting[0].bInterfaceClass);
      return NULL;
    }

  if (desc.bDeviceSubClass != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a GLxxx (bDeviceSubClass = 0x%x)\n",
		desc.bDeviceSubClass);
      return NULL;
    }
  if (desc.bDeviceProtocol != 0xff)
    {
      if (verbose > 2)
	printf ("    this is not a GLxxx (bDeviceProtocol = 0x%x)\n",
		desc.bDeviceProtocol);
      return NULL;
    }

  /* Check endpoints */
  if (config0->interface[0].altsetting[0].bNumEndpoints != 3)
    {
      if (verbose > 2)
	printf ("    this is not a GLxxx (bNumEndpoints = %d)\n",
		config0->interface[0].altsetting[0].bNumEndpoints);
      return NULL;
    }

  if ((config0->interface[0].altsetting[0].endpoint[0].bEndpointAddress !=
       0x81)
      || (config0->interface[0].altsetting[0].endpoint[0].bmAttributes !=
	  0x02)
      ||
      ((config0->interface[0].altsetting[0].endpoint[0].wMaxPacketSize !=
	0x40)
       && (config0->interface[0].altsetting[0].endpoint[0].wMaxPacketSize !=
	   0x200))
      || (config0->interface[0].altsetting[0].endpoint[0].bInterval != 0x0))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GLxxx (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   config0->interface[0].altsetting[0].endpoint[0].bEndpointAddress,
	   config0->interface[0].altsetting[0].endpoint[0].bmAttributes,
	   config0->interface[0].altsetting[0].endpoint[0].wMaxPacketSize,
	   config0->interface[0].altsetting[0].endpoint[0].bInterval);
      return NULL;
    }

  if ((config0->interface[0].altsetting[0].endpoint[1].bEndpointAddress !=
       0x02)
      || (config0->interface[0].altsetting[0].endpoint[1].bmAttributes !=
	  0x02)
      ||
      ((config0->interface[0].altsetting[0].endpoint[1].wMaxPacketSize !=
	0x40)
       && (config0->interface[0].altsetting[0].endpoint[1].wMaxPacketSize !=
	   0x200))
      || (config0->interface[0].altsetting[0].endpoint[1].bInterval != 0))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GLxxx (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   config0->interface[0].altsetting[0].endpoint[1].bEndpointAddress,
	   config0->interface[0].altsetting[0].endpoint[1].bmAttributes,
	   config0->interface[0].altsetting[0].endpoint[1].wMaxPacketSize,
	   config0->interface[0].altsetting[0].endpoint[1].bInterval);
      return NULL;
    }

  if ((config0->interface[0].altsetting[0].endpoint[2].bEndpointAddress !=
       0x83)
      || (config0->interface[0].altsetting[0].endpoint[2].bmAttributes !=
	  0x03)
      || (config0->interface[0].altsetting[0].endpoint[2].wMaxPacketSize !=
	  0x1)
      || ((config0->interface[0].altsetting[0].endpoint[2].bInterval != 8)
	  && (config0->interface[0].altsetting[0].endpoint[2].bInterval !=
	      16)))
    {
      if (verbose > 2)
	printf
	  ("    this is not a GLxxx (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   config0->interface[0].altsetting[0].endpoint[2].bEndpointAddress,
	   config0->interface[0].altsetting[0].endpoint[2].bmAttributes,
	   config0->interface[0].altsetting[0].endpoint[2].wMaxPacketSize,
	   config0->interface[0].altsetting[0].endpoint[2].bInterval);
      return NULL;
    }

  /* write then read a register using common read/write control message */
  result = genesys_write_reg (handle, 0x38, 0x15);
  if (!result)
    {
      if (verbose > 2)
	printf ("    this is not a GLxxx (writing register failed)\n");
      return NULL;
    }

  result = genesys_read_reg (handle, 0x38, &val);
  if (!result)
    {
      if (verbose > 2)
	printf ("    this is not a GLxxx (reading register failed)\n");
      return NULL;
    }

  if (val != 0x15)
    {
      if (verbose > 2)
	printf ("    this is not a GLxxx (reg 0x38 != 0x15)\n");
      return NULL;
    }

  /* decide revision number based on bcdUsb heuristics */
  if (desc.bcdDevice == 0x702)
    return "GL128";
  if (desc.bcdDevice == 0x701)
    return "GL124";
  if (desc.bcdDevice >= 0x700)
    return "GL848+";
  if (desc.bcdDevice >= 0x605)
    return "GL845";
  if (desc.bcdDevice >= 0x603)
    return "GL847";
  if (desc.bcdDevice >= 0x600)
    return "GL846";
  if (desc.bcdDevice >= 0x500)
    return "GL843";
  if (desc.bcdDevice >= 0x300)
    return "GL842";
  if (desc.bcdDevice > 0x101)
    return "GL841";
  return "GL646_HP";
}

/********** the lm983x section **********/

static int
lm983x_wb (libusb_device_handle *handle, unsigned char reg, unsigned char val)
{
  unsigned char buf[5];
  int written;
  int result;

  buf[0] = 0;
  buf[1] = reg;
  buf[2] = 0;
  buf[3] = 1;
  buf[4] = val;

  result = libusb_bulk_transfer(handle, 0x03, buf, 5, &written, TIMEOUT);
  if (result < 0)
    return 0;

  if (written != 5)
    return 0;

  return 1;
}

static int
lm983x_rb (libusb_device_handle *handle, unsigned char reg, unsigned char *val)
{
  unsigned char buf[5];
  int result;
  int tfx;

  buf[0] = 1;
  buf[1] = reg;
  buf[2] = 0;
  buf[3] = 1;

  result = libusb_bulk_transfer(handle, 0x03, buf, 4, &tfx, TIMEOUT);
  if (result < 0)
    return 0;

  if (tfx != 4)
    return 0;

  result = libusb_bulk_transfer(handle, 0x82, val, 1, &tfx, TIMEOUT);
  if (result < 0)
    return 0;

  if (tfx != 1)
    return 0;

  return 1;
}

/** @brief check for known LM983x chip (aka Merlin)
 *
 * Try to check if the scanner uses a LM983x ASIC.
 *
 * @param dev libusb device
 * @param hdl libusb opened handle
 * @param config0 configuration 0 from get config _descriptor
 * @return a string with ASIC name, or NULL if not recognized
 */
static char *
check_merlin(libusb_device_handle * handle,
	       struct libusb_device_descriptor desc,
	       struct libusb_config_descriptor *config0)
{
  unsigned char val;
  int result;

  if (verbose > 2)
    printf ("    checking for LM983[1,2,3] ...\n");

  /* Check device descriptor */
  if (((desc.bDeviceClass != LIBUSB_CLASS_VENDOR_SPEC)
       && (desc.bDeviceClass != 0))
      || (config0->interface[0].altsetting[0].bInterfaceClass !=
	  LIBUSB_CLASS_VENDOR_SPEC))
    {
      if (verbose > 2)
	printf
	  ("    this is not a LM983x (bDeviceClass = %d, bInterfaceClass = %d)\n",
	   desc.bDeviceClass,
	   config0->interface[0].altsetting[0].bInterfaceClass);
      return 0;
    }
  if ((desc.bcdUSB != 0x110)
      && (desc.bcdUSB != 0x101)
      && (desc.bcdUSB != 0x100))
    {
      if (verbose > 2)
	printf ("    this is not a LM983x (bcdUSB = 0x%x)\n", desc.bcdUSB);
      return 0;
    }
  if (desc.bDeviceSubClass != 0x00)
    {
      if (verbose > 2)
	printf ("    this is not a LM983x (bDeviceSubClass = 0x%x)\n",
		desc.bDeviceSubClass);
      return 0;
    }
  if ((desc.bDeviceProtocol != 0) &&
      (desc.bDeviceProtocol != 0xff))
    {
      if (verbose > 2)
	printf ("    this is not a LM983x (bDeviceProtocol = 0x%x)\n",
		desc.bDeviceProtocol);
      return 0;
    }

  /* Check endpoints */
  if (config0->interface[0].altsetting[0].bNumEndpoints != 3)
    {
      if (verbose > 2)
	printf ("    this is not a LM983x (bNumEndpoints = %d)\n",
		config0->interface[0].altsetting[0].bNumEndpoints);
      return 0;
    }

  if ((config0->interface[0].altsetting[0].endpoint[0].bEndpointAddress != 0x81)
      || (config0->interface[0].altsetting[0].endpoint[0].bmAttributes != 0x03)
      || (config0->interface[0].altsetting[0].endpoint[0].wMaxPacketSize != 0x1)
      || (config0->interface[0].altsetting[0].endpoint[0].bInterval != 0x10))
    {
      if (verbose > 2)
	printf
	  ("    this is not a LM983x (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   config0->interface[0].altsetting[0].endpoint[0].bEndpointAddress,
	   config0->interface[0].altsetting[0].endpoint[0].bmAttributes,
	   config0->interface[0].altsetting[0].endpoint[0].wMaxPacketSize,
	   config0->interface[0].altsetting[0].endpoint[0].bInterval);
      return 0;
    }

  if ((config0->interface[0].altsetting[0].endpoint[1].bEndpointAddress != 0x82)
      || (config0->interface[0].altsetting[0].endpoint[1].bmAttributes != 0x02)
      || (config0->interface[0].altsetting[0].endpoint[1].wMaxPacketSize != 0x40)
      /* Currently disabled as we have some problems in detection here ! */
      /*|| (config0->interface[0].altsetting[0].endpoint[1].bInterval != 0) */
    )
    {
      if (verbose > 2)
	printf
	  ("    this is not a LM983x (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   config0->interface[0].altsetting[0].endpoint[1].bEndpointAddress,
	   config0->interface[0].altsetting[0].endpoint[1].bmAttributes,
	   config0->interface[0].altsetting[0].endpoint[1].wMaxPacketSize,
	   config0->interface[0].altsetting[0].endpoint[1].bInterval);
      return 0;
    }

  if ((config0->interface[0].altsetting[0].endpoint[2].bEndpointAddress != 0x03)
      || (config0->interface[0].altsetting[0].endpoint[2].bmAttributes != 0x02)
      || (config0->interface[0].altsetting[0].endpoint[2].wMaxPacketSize != 0x40)
      /* Currently disabled as we have some problems in detection here ! */
      /* || (config0->interface[0].altsetting[0].endpoint[2].bInterval != 0) */
    )
    {
      if (verbose > 2)
	printf
	  ("    this is not a LM983x (bEndpointAddress = 0x%x, bmAttributes = 0x%x, "
	   "wMaxPacketSize = 0x%x, bInterval = 0x%x)\n",
	   config0->interface[0].altsetting[0].endpoint[2].bEndpointAddress,
	   config0->interface[0].altsetting[0].endpoint[2].bmAttributes,
	   config0->interface[0].altsetting[0].endpoint[2].wMaxPacketSize,
	   config0->interface[0].altsetting[0].endpoint[2].bInterval);
      return 0;
    }

  result = lm983x_wb (handle, 0x07, 0x00);
  if (1 == result)
    result = lm983x_wb (handle, 0x08, 0x02);
  if (1 == result)
    result = lm983x_rb (handle, 0x07, &val);
  if (1 == result)
    result = lm983x_rb (handle, 0x08, &val);
  if (1 == result)
    result = lm983x_rb (handle, 0x69, &val);

  if (0 == result)
    {
      if (verbose > 2)
	printf ("  Couldn't access LM983x registers.\n");
      return 0;
    }

  switch (val)
    {
    case 4:
      return "LM9832/3";
      break;
    case 3:
      return "LM9831";
      break;
    case 2:
      return "LM9830";
      break;
    default:
      return "LM983x?";
      break;
    }
}


char *
check_usb_chip (int verbosity,
		struct libusb_device_descriptor desc,
		libusb_device_handle * hdl,
		struct libusb_config_descriptor *config0)
{
  char *chip_name = NULL;
  int ret;

  verbose = verbosity;

  if (verbose > 2)
    printf ("\n<trying to find out which USB chip is used>\n");

  /* set config if needed */
  if (desc.bNumConfigurations > 1)
    {
      ret = libusb_set_configuration (hdl, config0->bConfigurationValue);
      if (ret < 0)
	{
	  if (verbose > 2)
	    printf ("couldnt set device to configuration %d\n",
		    config0->bConfigurationValue);
	  return NULL;
	}
    }

  /* claim the interface (only interface 0 is currently handled */
  ret = libusb_claim_interface (hdl, 0);
  if (ret < 0)
    {
      if (verbose > 2)
	printf ("could not claim USB device interface\n");
      return NULL;
    }

  /* now USB is opened and set up, actual chip detection */

  if (!chip_name)
    chip_name = check_merlin (hdl, desc, config0);

  if (!chip_name)
  	chip_name = check_gt6801 (hdl, desc, config0);

  if (!chip_name)
  	chip_name = check_gt6816 (hdl, desc, config0);

  if (!chip_name)
    chip_name = check_genesys (hdl, desc, config0);

  if (verbose > 2)
    {
      if (chip_name)
	printf ("<This USB chip looks like a %s (result from %s)>\n\n",
		chip_name, PACKAGE_STRING);
      else
	printf
	  ("<Couldn't determine the type of the USB chip (result from %s)>\n\n",
	   PACKAGE_STRING);
    }

  /* close USB device */
  libusb_release_interface (hdl, 0);
  return chip_name;
}
#endif /* HAVE_LIBUSB_1_0 */
