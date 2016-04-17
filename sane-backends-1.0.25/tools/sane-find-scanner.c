/* sane-find-scanner.c

   Copyright (C) 1997-2013 Oliver Rauch, Henning Meier-Geinitz, and others.

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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#if defined (HAVE_DDK_NTDDSCSI_H) || defined (HAVE_NTDDSCSI_H)
# define WIN32_SCSI
# include <windows.h>
# if defined (HAVE_DDK_NTDDSCSI_H)
#  include <ddk/scsi.h>
#  include <ddk/ntddscsi.h>
# elif defined (HAVE_NTDDSCSI_H)
#  include <ntddscsi.h>
# endif
#endif

#include "../include/sane/sanei.h"
#include "../include/sane/sanei_scsi.h"
#include "../include/sane/sanei_pa4s2.h"
#include "../include/sane/sanei_config.h"

#ifdef HAVE_LIBUSB
#ifdef HAVE_LUSB0_USB_H
#include <lusb0_usb.h>
#else
#include <usb.h>
#endif
extern char * check_usb_chip (struct usb_device *dev, int verbosity, SANE_Bool from_file);
#endif

#ifdef HAVE_LIBUSB_1_0
#include <libusb.h>
extern char * check_usb_chip (int verbosity,
			      struct libusb_device_descriptor desc,
			      libusb_device_handle *hdl,
			      struct libusb_config_descriptor *config0);
#endif

#include "../include/sane/sanei_usb.h"

#ifndef PATH_MAX
# define PATH_MAX 1024
#endif

static const char *prog_name;
static int verbose = 1;
static SANE_Bool force = SANE_FALSE;
static SANE_Bool device_found = SANE_FALSE;
static SANE_Bool libusb_device_found = SANE_FALSE;
static SANE_Bool unknown_found = SANE_FALSE;

#ifdef HAVE_LIBUSB_1_0
libusb_context *sfs_usb_ctx;
#endif

typedef struct
{
  unsigned char *cmd;
  size_t size;
}
scsiblk;

#define INQUIRY					0x12
#define set_inquiry_return_size(icb,val)	icb[0x04]=val
#define IN_periph_devtype_cpu			0x03
#define IN_periph_devtype_scanner		0x06
#define get_scsi_inquiry_vendor(in, buf)	strncpy(buf, in + 0x08, 0x08)
#define get_scsi_inquiry_product(in, buf)	strncpy(buf, in + 0x10, 0x010)
#define get_scsi_inquiry_version(in, buf)	strncpy(buf, in + 0x20, 0x04)
#define get_scsi_inquiry_periph_devtype(in)	(in[0] & 0x1f)
#define get_scsi_inquiry_additional_length(in)	in[0x04]
#define set_scsi_inquiry_length(out,n)		out[0x04]=n-5

static unsigned char inquiryC[] = {
  INQUIRY, 0x00, 0x00, 0x00, 0xff, 0x00
};
static scsiblk inquiry = {
  inquiryC, sizeof (inquiryC)
};

static void
usage (char *msg)
{
  fprintf (stderr, "Usage: %s [-hvqf] [devname ...]\n", prog_name);
  fprintf (stderr, "\t-h: print this help message\n");
  fprintf (stderr, "\t-v: be more verbose (can be used multiple times)\n");
  fprintf (stderr, "\t-q: be quiet (print only devices, no comments)\n");
  fprintf (stderr, "\t-f: force opening devname as SCSI even if it looks "
	   "like USB\n");
  fprintf (stderr, "\t-p: enable scanning for parallel port devices\n");
#ifdef HAVE_LIBUSB
  fprintf (stderr, "\t-F file: try to detect chipset from given "
	   "/proc/bus/usb/devices file\n");
#endif
  if (msg)
    fprintf (stderr, "\t%s\n", msg);
}

/* if SCSI generic is optional on your OS, and there is a software test
   which will determine if it is included, add it here. If you are sure
   that SCSI generic was found, return 1. If SCSI generic is always
   available in your OS, return 1 */

static int
check_sg (void)
{
#if defined(__linux__)
  /* Assumption: /proc/devices lines are shorter than 256 characters */
  char line[256], driver[256] = "";
  FILE *stream;

  stream = fopen ("/proc/devices", "r");
  /* Likely reason for failure, no /proc => probably no SG either */
  if (stream == NULL)
    return 0;

  while (fgets (line, sizeof (line) - 1, stream))
    {
      if (sscanf (line, " %*d %s\n", driver) > 0 && !strcmp (driver, "sg"))
	break;
    }
  fclose (stream);

  if (strcmp (driver, "sg"))
    {
      return 0;
    }
  else
    {
      return 1;
    }
#endif
  return 1;			/* Give up, and assume yes to avoid false negatives */
}

/* Display a buffer in the log. Display by lines of 16 bytes. */
static void
hexdump (const char *comment, unsigned char *buf, const int length)
{
  int i;
  char line[128];
  char *ptr;
  char asc_buf[17];
  char *asc_ptr;

  printf ("  %s\n", comment);

  i = 0;
  goto start;

  do
    {
      if (i < length)
	{
	  ptr += sprintf (ptr, " %2.2x", *buf);

	  if (*buf >= 32 && *buf <= 127)
	    {
	      asc_ptr += sprintf (asc_ptr, "%c", *buf);
	    }
	  else
	    {
	      asc_ptr += sprintf (asc_ptr, ".");
	    }
	}
      else
	{
	  /* After the length; do nothing. */
	  ptr += sprintf (ptr, "   ");
	}

      i++;
      buf++;

      if ((i % 16) == 0)
	{
	  /* It's a new line */
	  printf ("  %s    %s\n", line, asc_buf);

	start:
	  ptr = line;
	  *ptr = '\0';
	  asc_ptr = asc_buf;
	  *asc_ptr = '\0';

	  ptr += sprintf (ptr, "  %3.3d:", i);
	}

    }
  while (i < ((length + 15) & ~15));
}

static SANE_Status
scanner_do_scsi_inquiry (unsigned char *buffer, int sfd)
{
  size_t size;
  SANE_Status status;

  memset (buffer, '\0', 256);	/* clear buffer */

  size = 5;			/* first get only 5 bytes to get size of 
				   inquiry_return_block */
  set_inquiry_return_size (inquiry.cmd, size);
  status = sanei_scsi_cmd (sfd, inquiry.cmd, inquiry.size, buffer, &size);

  if (status != SANE_STATUS_GOOD)
    return (status);

  size = get_scsi_inquiry_additional_length (buffer) + 5;

  /* then get inquiry with actual size */
  set_inquiry_return_size (inquiry.cmd, size);
  status = sanei_scsi_cmd (sfd, inquiry.cmd, inquiry.size, buffer, &size);

  return (status);
}

static void
scanner_identify_scsi_scanner (unsigned char *buffer, int sfd,
			       char *devicename)
{
  unsigned char vendor[9];
  unsigned char product[17];
  unsigned char version[5];
  unsigned char *pp;
  unsigned int devtype;
  SANE_Status status;
  static char *devtypes[] = {
    "disk", "tape", "printer", "processor", "CD-writer",
    "CD-drive", "scanner", "optical-drive", "jukebox",
    "communicator"
  };
  status = scanner_do_scsi_inquiry (buffer, sfd);
  if (status != SANE_STATUS_GOOD)
    {
      if (verbose > 1)
	printf ("inquiry for device %s failed (%s)\n",
		devicename, sane_strstatus (status));
      return;
    }

  if (verbose > 2)
    hexdump ("Inquiry for device:", buffer,
	     get_scsi_inquiry_additional_length (buffer) + 5);

  devtype = get_scsi_inquiry_periph_devtype (buffer);
  if (verbose <= 1 && devtype != IN_periph_devtype_scanner
      /* old HP scanners use the CPU id ... */
      && devtype != IN_periph_devtype_cpu)
    return;			/* no, continue searching */

  get_scsi_inquiry_vendor ((char *) buffer, (char *) vendor);
  get_scsi_inquiry_product ((char *) buffer, (char *) product);
  get_scsi_inquiry_version ((char *) buffer, (char *) version);

  pp = &vendor[7];
  vendor[8] = '\0';
  while (pp >= vendor && (*pp == ' ' || *pp >= 127))
    *pp-- = '\0';

  pp = &product[15];
  product[16] = '\0';
  while (pp >= product && (*pp == ' ' || *pp >= 127))
    *pp-- = '\0';

  pp = &version[3];
  version[4] = '\0';
  while (pp >= version && (*pp == ' ' || *(pp - 1) >= 127))
    *pp-- = '\0';

  device_found = SANE_TRUE;
  printf ("found SCSI %s \"%s %s %s\" at %s\n",
	  devtype < NELEMS (devtypes) ? devtypes[devtype] : "unknown device",
	  vendor, product, version, devicename);
  return;
}

static void
check_scsi_file (char *file_name)
{
  int result;
  int sfd;
  unsigned char buffer[16384];

  if (strstr (file_name, "usb")
      || strstr (file_name, "uscanner") || strstr (file_name, "ugen"))
    {
      if (force)
	{
	  if (verbose > 1)
	    printf ("checking %s even though it looks like a USB device...",
		    file_name);
	}
      else
	{
	  if (verbose > 1)
	    printf ("ignored %s (not a SCSI device)\n", file_name);
	  return;
	}
    }
  else if (verbose > 1)
    printf ("checking %s...", file_name);

  result = sanei_scsi_open (file_name, &sfd, NULL, NULL);

  if (verbose > 1)
    {
      if (result != 0)
	printf (" failed to open (%s)\n", sane_strstatus (result));
      else
	printf (" open ok\n");
    }

  if (result == SANE_STATUS_GOOD)
    {
      scanner_identify_scsi_scanner (buffer, sfd, file_name);
      sanei_scsi_close (sfd);
    }
  return;
}

static void
check_usb_file (char *file_name)
{
  SANE_Status result;
  SANE_Word vendor, product;
  SANE_Int fd;

  if (!strstr (file_name, "usb")
      && !strstr (file_name, "uscanner") && !strstr (file_name, "ugen"))
    {
      if (force)
	{
	  if (verbose > 1)
	    printf ("checking %s even though doesn't look like a "
		    "USB device...", file_name);
	}
      else
	{
	  if (verbose > 1)
	    printf ("ignored %s (not a USB device)\n", file_name);
	  return;
	}
    }
  else if (verbose > 1)
    printf ("checking %s...", file_name);

  result = sanei_usb_open (file_name, &fd);

  if (result != SANE_STATUS_GOOD)
    {
      if (verbose > 1)
	printf (" failed to open (%s)\n", sane_strstatus (result));
    }
  else
    {
      result = sanei_usb_get_vendor_product (fd, &vendor, &product);
      if (result == SANE_STATUS_GOOD)
	{
	  if (verbose > 1)
	    printf (" open ok, vendor and product ids were identified\n");
	  printf ("found USB scanner (vendor=0x%04x, "
		  "product=0x%04x) at %s\n", vendor, product, file_name);
	}
      else
	{
	  if (verbose > 1)
	    printf (" open ok, but vendor and product could NOT be "
		    "identified\n");
	  printf ("found USB scanner (UNKNOWN vendor and product) "
		  "at device %s\n", file_name);
	  unknown_found = SANE_TRUE;
	}
      device_found = SANE_TRUE;
      sanei_usb_close (fd);
    }
}

#ifdef HAVE_LIBUSB

static char *
get_libusb_string_descriptor (struct usb_device *dev, int index)
{
  usb_dev_handle *handle;
  char *buffer, short_buffer[2];
  struct usb_string_descriptor *sd;
  int size = 2;
  int i;

  if (!index)
    return 0;

  handle = usb_open (dev);
  if (!handle)
    return 0;

  sd = (struct usb_string_descriptor *) short_buffer;

  if (usb_control_msg (handle,
		       USB_ENDPOINT_IN + USB_TYPE_STANDARD + USB_RECIP_DEVICE,
		       USB_REQ_GET_DESCRIPTOR,
		       (USB_DT_STRING << 8) + index, 0, short_buffer,
		       size, 1000) < 0)
    {
      usb_close (handle);
      return 0;
    }

  if (sd->bLength < 2 
      || sd->bDescriptorType != USB_DT_STRING)
    {
      usb_close (handle);
      return 0;
    }
  
  size = sd->bLength;

  buffer = calloc (1, size + 1);
  if (!buffer)
    return 0;

  sd = (struct usb_string_descriptor *) buffer;

  if (usb_control_msg (handle,
		       USB_ENDPOINT_IN + USB_TYPE_STANDARD + USB_RECIP_DEVICE,
		       USB_REQ_GET_DESCRIPTOR,
		       (USB_DT_STRING << 8) + index, 0, buffer,
		       size, 1000) < 0)
    {
      usb_close (handle);
      free (buffer);
      return 0;
    }

  if (sd->bLength < 2 || sd->bLength > size
      || sd->bDescriptorType != USB_DT_STRING)
    {
      usb_close (handle);
      free (buffer);
      return 0;
    }
  size = sd->bLength - 2;
  for (i = 0; i < (size / 2); i++)
    buffer[i] = buffer[2 + 2 * i];
  buffer[i] = 0;

  usb_close (handle);
  return buffer;
}

static char *
get_libusb_vendor (struct usb_device *dev)
{
  return get_libusb_string_descriptor (dev, dev->descriptor.iManufacturer);
}

static char *
get_libusb_product (struct usb_device *dev)
{
  return get_libusb_string_descriptor (dev, dev->descriptor.iProduct);
}

static void
check_libusb_device (struct usb_device *dev, SANE_Bool from_file)
{
  int is_scanner = 0;
  char *vendor;
  char *product;
  int interface_nr;

  if (!dev->config)
    {
      if (verbose > 1)
	printf ("device 0x%04x/0x%04x is not configured\n",
		dev->descriptor.idVendor, dev->descriptor.idProduct);
      return;
    }

  vendor = get_libusb_vendor (dev);
  product = get_libusb_product (dev);

  if (verbose > 2)
    {
      /* print everything we know about the device */
      char *buf;
      int config_nr;
      struct usb_device_descriptor *d = &dev->descriptor;

      printf ("\n");
      printf ("<device descriptor of 0x%04x/0x%04x at %s:%s",
	      d->idVendor, d->idProduct, dev->bus->dirname, dev->filename);
      if (vendor || product)
	{
	  printf (" (%s%s%s)", vendor ? vendor : "",
		  (vendor && product) ? " " : "", product ? product : "");
	}
      printf (">\n");
      printf ("bLength               %d\n", d->bLength);
      printf ("bDescriptorType       %d\n", d->bDescriptorType);
      printf ("bcdUSB                %d.%d%d\n", d->bcdUSB >> 8,
	      (d->bcdUSB >> 4) & 15, d->bcdUSB & 15);
      printf ("bDeviceClass          %d\n", d->bDeviceClass);
      printf ("bDeviceSubClass       %d\n", d->bDeviceSubClass);
      printf ("bDeviceProtocol       %d\n", d->bDeviceProtocol);
      printf ("bMaxPacketSize0       %d\n", d->bMaxPacketSize0);
      printf ("idVendor              0x%04X\n", d->idVendor);
      printf ("idProduct             0x%04X\n", d->idProduct);
      printf ("bcdDevice             %d.%d%d\n", d->bcdDevice >> 8,
	      (d->bcdDevice >> 4) & 15, d->bcdDevice & 15);
      printf ("iManufacturer         %d (%s)\n", d->iManufacturer,
	      (vendor) ? vendor : "");
      printf ("iProduct              %d (%s)\n", d->iProduct,
	      (product) ? product : "");

      buf = get_libusb_string_descriptor (dev, d->iSerialNumber);
      printf ("iSerialNumber         %d (%s)\n", d->iSerialNumber,
	      (buf) ? buf : "");
      if (buf)
	free (buf);
      printf ("bNumConfigurations    %d\n", d->bNumConfigurations);

      for (config_nr = 0; config_nr < d->bNumConfigurations; config_nr++)
	{
	  struct usb_config_descriptor *c = &dev->config[config_nr];
	  int interface_nr;

	  printf (" <configuration %d>\n", config_nr);
	  printf (" bLength              %d\n", c->bLength);
	  printf (" bDescriptorType      %d\n", c->bDescriptorType);
	  printf (" wTotalLength         %d\n", c->wTotalLength);
	  printf (" bNumInterfaces       %d\n", c->bNumInterfaces);
	  printf (" bConfigurationValue  %d\n", c->bConfigurationValue);
	  buf = get_libusb_string_descriptor (dev, c->iConfiguration);
	  printf (" iConfiguration       %d (%s)\n", c->iConfiguration,
		  (buf) ? buf : "");
	  if (buf)
	    free (buf);
	  printf (" bmAttributes         %d (%s%s)\n", c->bmAttributes,
		  c->bmAttributes & 64 ? "Self-powered" : "",
		  c->bmAttributes & 32 ? "Remote Wakeup" : "");
	  printf (" MaxPower             %d mA\n", c->MaxPower * 2);

	  for (interface_nr = 0; interface_nr < c->bNumInterfaces;
	       interface_nr++)
	    {
	      struct usb_interface *interface = &c->interface[interface_nr];
	      int alt_setting_nr;

	      printf ("  <interface %d>\n", interface_nr);
	      for (alt_setting_nr = 0;
		   alt_setting_nr < interface->num_altsetting;
		   alt_setting_nr++)
		{
		  struct usb_interface_descriptor *i
		    = &interface->altsetting[alt_setting_nr];
		  int ep_nr;
		  printf ("   <altsetting %d>\n", alt_setting_nr);
		  printf ("   bLength            %d\n", i->bLength);
		  printf ("   bDescriptorType    %d\n", i->bDescriptorType);
		  printf ("   bInterfaceNumber   %d\n", i->bInterfaceNumber);
		  printf ("   bAlternateSetting  %d\n", i->bAlternateSetting);
		  printf ("   bNumEndpoints      %d\n", i->bNumEndpoints);
		  printf ("   bInterfaceClass    %d\n", i->bInterfaceClass);
		  printf ("   bInterfaceSubClass %d\n",
			  i->bInterfaceSubClass);
		  printf ("   bInterfaceProtocol %d\n",
			  i->bInterfaceProtocol);
		  buf = get_libusb_string_descriptor (dev, i->iInterface);
		  printf ("   iInterface         %d (%s)\n", i->iInterface,
			  (buf) ? buf : "");
		  if (buf)
		    free (buf);

		  for (ep_nr = 0; ep_nr < i->bNumEndpoints; ep_nr++)
		    {
		      struct usb_endpoint_descriptor *e = &i->endpoint[ep_nr];
		      char *ep_type;

		      switch (e->bmAttributes & USB_ENDPOINT_TYPE_MASK)
			{
			case USB_ENDPOINT_TYPE_CONTROL:
			  ep_type = "control";
			  break;
			case USB_ENDPOINT_TYPE_ISOCHRONOUS:
			  ep_type = "isochronous";
			  break;
			case USB_ENDPOINT_TYPE_BULK:
			  ep_type = "bulk";
			  break;
			case USB_ENDPOINT_TYPE_INTERRUPT:
			  ep_type = "interrupt";
			  break;
			default:
			  ep_type = "unknown";
			  break;
			}
		      printf ("    <endpoint %d>\n", ep_nr);
		      printf ("    bLength           %d\n", e->bLength);
		      printf ("    bDescriptorType   %d\n",
			      e->bDescriptorType);
		      printf ("    bEndpointAddress  0x%02X (%s 0x%02X)\n",
			      e->bEndpointAddress,
			      e->bEndpointAddress & USB_ENDPOINT_DIR_MASK ?
			      "in" : "out",
			      e->
			      bEndpointAddress & USB_ENDPOINT_ADDRESS_MASK);
		      printf ("    bmAttributes      %d (%s)\n",
			      e->bmAttributes, ep_type);
		      printf ("    wMaxPacketSize    %d\n",
			      e->wMaxPacketSize);
		      printf ("    bInterval         %d ms\n", e->bInterval);
		      printf ("    bRefresh          %d\n", e->bRefresh);
		      printf ("    bSynchAddress     %d\n", e->bSynchAddress);
		    }
		}
	    }
	}

    }

  /* Some heuristics, which device may be a scanner */
  if (dev->descriptor.idVendor == 0)	/* hub */
    --is_scanner;
  if (dev->descriptor.idProduct == 0)	/* hub */
    --is_scanner;

  for (interface_nr = 0; interface_nr < dev->config[0].bNumInterfaces && is_scanner <= 0; interface_nr++)
    {
      switch (dev->descriptor.bDeviceClass)
	{
	case USB_CLASS_VENDOR_SPEC:
	  ++is_scanner;
	  break;
	case USB_CLASS_PER_INTERFACE:
	  if (dev->config[0].interface[interface_nr].num_altsetting == 0 || 
	      !dev->config[0].interface[interface_nr].altsetting)
	    break;
	  switch (dev->config[0].interface[interface_nr].altsetting[0].bInterfaceClass)
	    {
	    case USB_CLASS_VENDOR_SPEC:
	    case USB_CLASS_PER_INTERFACE:
	    case 16:                /* data? */
	      ++is_scanner;
	      break;
	    }
	  break;
	}
    }

  if (is_scanner > 0)
    {
      char * chipset = check_usb_chip (dev, verbose, from_file);

      printf ("found USB scanner (vendor=0x%04x", dev->descriptor.idVendor);
      if (vendor)
	printf (" [%s]", vendor);
      printf (", product=0x%04x", dev->descriptor.idProduct);
      if (product)
	printf (" [%s]", product);
      if (chipset)
	printf (", chip=%s", chipset);
      if (from_file)
	printf (")\n");
      else
	printf (") at libusb:%s:%s\n", dev->bus->dirname, dev->filename);

      libusb_device_found = SANE_TRUE;
      device_found = SANE_TRUE;
    }

  if (vendor)
    free (vendor);

  if (product)
    free (product);
}
#endif /* HAVE_LIBUSB */


#ifdef HAVE_LIBUSB_1_0
static char *
sfs_libusb_strerror (int errcode)
{
  /* Error codes & descriptions from the libusb-1.0 documentation */

  switch (errcode)
    {
      case LIBUSB_SUCCESS:
	return "Success (no error)";

      case LIBUSB_ERROR_IO:
	return "Input/output error";

      case LIBUSB_ERROR_INVALID_PARAM:
	return "Invalid parameter";

      case LIBUSB_ERROR_ACCESS:
	return "Access denied (insufficient permissions)";

      case LIBUSB_ERROR_NO_DEVICE:
	return "No such device (it may have been disconnected)";

      case LIBUSB_ERROR_NOT_FOUND:
	return "Entity not found";

      case LIBUSB_ERROR_BUSY:
	return "Resource busy";

      case LIBUSB_ERROR_TIMEOUT:
	return "Operation timed out";

      case LIBUSB_ERROR_OVERFLOW:
	return "Overflow";

      case LIBUSB_ERROR_PIPE:
	return "Pipe error";

      case LIBUSB_ERROR_INTERRUPTED:
	return "System call interrupted (perhaps due to signal)";

      case LIBUSB_ERROR_NO_MEM:
	return "Insufficient memory";

      case LIBUSB_ERROR_NOT_SUPPORTED:
	return "Operation not supported or unimplemented on this platform";

      case LIBUSB_ERROR_OTHER:
	return "Other error";

      default:
	return "Unknown libusb-1.0 error code";
    }
}

static char *
get_libusb_string_descriptor (libusb_device_handle *hdl, int index)
{
  unsigned char *buffer, short_buffer[2];
  int size;
  int ret;
  int i;

  if (!index)
    return NULL;

  ret = libusb_get_descriptor (hdl, LIBUSB_DT_STRING, index,
			       short_buffer, sizeof (short_buffer));
  if (ret < 0)
    {
      printf ("could not fetch string descriptor: %s\n",
	      sfs_libusb_strerror (ret));
      return NULL;
    }

  if ((short_buffer[0] < 2) /* descriptor length */
      || (short_buffer[1] != LIBUSB_DT_STRING)) /* descriptor type */
    return NULL;
  
  size = short_buffer[0];

  buffer = calloc (1, size + 1);
  if (!buffer)
    return NULL;

  ret = libusb_get_descriptor (hdl, LIBUSB_DT_STRING, index,
			       buffer, size);
  if (ret < 0)
    {
      printf ("could not fetch string descriptor (again): %s\n",
	      sfs_libusb_strerror (ret));
      free (buffer);
      return NULL;
    }

  if ((buffer[0] < 2) || (buffer[0] > size) /* descriptor length */
      || (buffer[1] != LIBUSB_DT_STRING)) /* descriptor type */
    {
      free (buffer);
      return NULL;
    }

  size = buffer[0] - 2;
  for (i = 0; i < (size / 2); i++)
    buffer[i] = buffer[2 + 2 * i];
  buffer[i] = '\0';

  return (char *) buffer;
}

static void
check_libusb_device (libusb_device *dev, SANE_Bool from_file)
{
  libusb_device_handle *hdl;
  struct libusb_device_descriptor desc;
  struct libusb_config_descriptor *config0;

  int is_scanner = 0;
  char *vendor;
  char *product;
  unsigned short vid, pid;
  unsigned char busno, address;
  int config;
  int intf;
  int ret;

  busno = libusb_get_bus_number (dev);
  address = libusb_get_device_address (dev);

  ret = libusb_get_device_descriptor (dev, &desc);
  if (ret < 0)
    {
      printf ("could not get device descriptor for device at %03d:%03d: %s\n",
	      busno, address, sfs_libusb_strerror (ret));
      return;
    }

  vid = desc.idVendor;
  pid = desc.idProduct;

  ret = libusb_open (dev, &hdl);
  if (ret < 0)
    {
      printf ("could not open USB device 0x%04x/0x%04x at %03d:%03d: %s\n",
	      vid, pid, busno, address, sfs_libusb_strerror (ret));
      return;
    }

  ret = libusb_get_configuration (hdl, &config);
  if (ret < 0)
    {
      printf ("could not get configuration for device 0x%04x/0x%04x at %03d:%03d: %s\n",
	      vid, pid, busno, address, sfs_libusb_strerror (ret));
      libusb_close (hdl);
      return;
    }

  if (config == 0)
    {
      if (verbose > 1)
	printf ("device 0x%04x/0x%04x at %03d:%03d is not configured\n",
		vid, pid, busno, address);
      libusb_close (hdl);
      return;
    }

  vendor = get_libusb_string_descriptor (hdl, desc.iManufacturer);
  product = get_libusb_string_descriptor (hdl, desc.iProduct);

  if (verbose > 2)
    {
      /* print everything we know about the device */
      char *buf;
      int config_nr;

      printf ("\n");
      printf ("<device descriptor of 0x%04x/0x%04x at %03d:%03d",
	      vid, pid, busno, address);
      if (vendor || product)
	{
	  printf (" (%s%s%s)", (vendor) ? vendor : "",
		  (vendor && product) ? " " : "", (product) ? product : "");
	}
      printf (">\n");
      printf ("bLength               %d\n", desc.bLength);
      printf ("bDescriptorType       %d\n", desc.bDescriptorType);
      printf ("bcdUSB                %d.%d%d\n", desc.bcdUSB >> 8,
	      (desc.bcdUSB >> 4) & 15, desc.bcdUSB & 15);
      printf ("bDeviceClass          %d\n", desc.bDeviceClass);
      printf ("bDeviceSubClass       %d\n", desc.bDeviceSubClass);
      printf ("bDeviceProtocol       %d\n", desc.bDeviceProtocol);
      printf ("bMaxPacketSize0       %d\n", desc.bMaxPacketSize0);
      printf ("idVendor              0x%04X\n", desc.idVendor);
      printf ("idProduct             0x%04X\n", desc.idProduct);
      printf ("bcdDevice             %d.%d%d\n", desc.bcdDevice >> 8,
	      (desc.bcdDevice >> 4) & 15, desc.bcdDevice & 15);
      printf ("iManufacturer         %d (%s)\n", desc.iManufacturer,
	      (vendor) ? vendor : "");
      printf ("iProduct              %d (%s)\n", desc.iProduct,
	      (product) ? product : "");
      buf = get_libusb_string_descriptor (hdl, desc.iSerialNumber);
      printf ("iSerialNumber         %d (%s)\n", desc.iSerialNumber,
	      (buf) ? buf : "");
      if (buf)
	free (buf);
      printf ("bNumConfigurations    %d\n", desc.bNumConfigurations);

      for (config_nr = 0; config_nr < desc.bNumConfigurations; config_nr++)
	{
	  struct libusb_config_descriptor *c;

	  ret = libusb_get_config_descriptor (dev, config_nr, &c);
	  if (ret < 0)
	    {
	      printf ("could not get configuration descriptor %d: %s\n",
		      config_nr, sfs_libusb_strerror (ret));
	      continue;
	    }

	  printf (" <configuration %d>\n", config_nr);
	  printf (" bLength              %d\n", c->bLength);
	  printf (" bDescriptorType      %d\n", c->bDescriptorType);
	  printf (" wTotalLength         %d\n", c->wTotalLength);
	  printf (" bNumInterfaces       %d\n", c->bNumInterfaces);
	  printf (" bConfigurationValue  %d\n", c->bConfigurationValue);

	  buf = get_libusb_string_descriptor (hdl, c->iConfiguration);
	  printf (" iConfiguration       %d (%s)\n", c->iConfiguration,
		  (buf) ? buf : "");
	  free (buf);
		  
	  printf (" bmAttributes         %d (%s%s)\n", c->bmAttributes,
		  c->bmAttributes & 64 ? "Self-powered" : "",
		  c->bmAttributes & 32 ? "Remote Wakeup" : "");
	  printf (" MaxPower             %d mA\n", c->MaxPower * 2);

	  for (intf = 0; intf < c->bNumInterfaces; intf++)
	    {
	      const struct libusb_interface *interface;
	      int alt_setting_nr;

	      interface = &c->interface[intf];

	      printf ("  <interface %d>\n", intf);
	      for (alt_setting_nr = 0;
		   alt_setting_nr < interface->num_altsetting;
		   alt_setting_nr++)
		{
		  const struct libusb_interface_descriptor *i;
		  int ep_nr;

		  i = &interface->altsetting[alt_setting_nr];

		  printf ("   <altsetting %d>\n", alt_setting_nr);
		  printf ("   bLength            %d\n", i->bLength);
		  printf ("   bDescriptorType    %d\n", i->bDescriptorType);
		  printf ("   bInterfaceNumber   %d\n", i->bInterfaceNumber);
		  printf ("   bAlternateSetting  %d\n", i->bAlternateSetting);
		  printf ("   bNumEndpoints      %d\n", i->bNumEndpoints);
		  printf ("   bInterfaceClass    %d\n", i->bInterfaceClass);
		  printf ("   bInterfaceSubClass %d\n",
			  i->bInterfaceSubClass);
		  printf ("   bInterfaceProtocol %d\n",
			  i->bInterfaceProtocol);

		  buf = NULL;
		  buf = get_libusb_string_descriptor (hdl, i->iInterface);
		  printf ("   iInterface         %d (%s)\n", i->iInterface,
			  (buf) ? buf : "");
		  free (buf);
		  for (ep_nr = 0; ep_nr < i->bNumEndpoints; ep_nr++)
		    {
		      const struct libusb_endpoint_descriptor *e;
		      char *ep_type;

		      e = &i->endpoint[ep_nr];

		      switch (e->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK)
			{
			  case LIBUSB_TRANSFER_TYPE_CONTROL:
			    ep_type = "control";
			    break;
			  case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
			    ep_type = "isochronous";
			    break;
			  case LIBUSB_TRANSFER_TYPE_BULK:
			    ep_type = "bulk";
			    break;
			  case LIBUSB_TRANSFER_TYPE_INTERRUPT:
			    ep_type = "interrupt";
			    break;
			  default:
			    ep_type = "unknown";
			    break;
			}
		      printf ("    <endpoint %d>\n", ep_nr);
		      printf ("    bLength           %d\n", e->bLength);
		      printf ("    bDescriptorType   %d\n",
			      e->bDescriptorType);
		      printf ("    bEndpointAddress  0x%02X (%s 0x%02X)\n",
			      e->bEndpointAddress,
			      e->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK ?
			      "in" : "out",
			      e->
			      bEndpointAddress & USB_ENDPOINT_ADDRESS_MASK);
		      printf ("    bmAttributes      %d (%s)\n",
			      e->bmAttributes, ep_type);
		      printf ("    wMaxPacketSize    %d\n",
			      e->wMaxPacketSize);
		      printf ("    bInterval         %d ms\n", e->bInterval);
		      printf ("    bRefresh          %d\n", e->bRefresh);
		      printf ("    bSynchAddress     %d\n", e->bSynchAddress);
		    }
		}
	    }
	}
    }


  /* Some heuristics, which device may be a scanner */
  if (desc.idVendor == 0)	/* hub */
    --is_scanner;
  if (desc.idProduct == 0)	/* hub */
    --is_scanner;

  ret = libusb_get_config_descriptor (dev, 0, &config0);
  if (ret < 0)
    {
      printf ("could not get config[0] descriptor: %s\n",
	      sfs_libusb_strerror (ret));

      goto out_free;
    }

  for (intf = 0; (intf < config0->bNumInterfaces) && (is_scanner <= 0); intf++)
    {
      switch (desc.bDeviceClass)
	{
	  case USB_CLASS_VENDOR_SPEC:
	    ++is_scanner;
	    break;
	  case USB_CLASS_PER_INTERFACE:
	    if ((config0->interface[intf].num_altsetting == 0)
		|| !config0->interface[intf].altsetting)
	      break;
	    switch (config0->interface[intf].altsetting[0].bInterfaceClass)
	      {
	        case USB_CLASS_VENDOR_SPEC:
	        case USB_CLASS_PER_INTERFACE:
	        case 16:                /* data? */
		  ++is_scanner;
		  break;
	      }
	    break;
	}
    }

  if (is_scanner > 0)
    {
      char *chipset = NULL;
      
      if(!from_file)
        chipset = check_usb_chip (verbose, desc, hdl, config0);

      printf ("found USB scanner (vendor=0x%04x", vid);
      if (vendor)
	printf (" [%s]", vendor);
      printf (", product=0x%04x", pid);
      if (product)
	printf (" [%s]", product);
      if (chipset)
	printf (", chip=%s", chipset);
      if (from_file)
	printf (")\n");
      else
	printf (") at libusb:%03d:%03d\n", busno, address);

      libusb_device_found = SANE_TRUE;
      device_found = SANE_TRUE;
    }
  
  libusb_free_config_descriptor (config0);

 out_free:
  libusb_close (hdl);
  if (vendor)
    free (vendor);

  if (product)
    free (product);
}
#endif /* HAVE_LIBUSB_1_0 */


static DIR *
scan_directory (char *dir_name)
{
  struct stat stat_buf;
  DIR *dir;

  if (verbose > 2)
    printf ("scanning directory %s\n", dir_name);

  if (stat (dir_name, &stat_buf) < 0)
    {
      if (verbose > 1)
	printf ("cannot stat `%s' (%s)\n", dir_name, strerror (errno));
      return 0;
    }
  if (!S_ISDIR (stat_buf.st_mode))
    {
      if (verbose > 1)
	printf ("`%s' is not a directory\n", dir_name);
      return 0;
    }
  if ((dir = opendir (dir_name)) == 0)
    {
      if (verbose > 1)
	printf ("cannot read directory `%s' (%s)\n", dir_name,
		strerror (errno));
      return 0;
    }
  return dir;
}

static char *
get_next_file (char *dir_name, DIR * dir)
{
  struct dirent *dir_entry;
  static char file_name[PATH_MAX];

  do
    {
      dir_entry = readdir (dir);
      if (!dir_entry)
	return 0;
    }
  while (strcmp (dir_entry->d_name, ".") == 0
	 || strcmp (dir_entry->d_name, "..") == 0);

  if (strlen (dir_name) + strlen (dir_entry->d_name) + 1 > PATH_MAX)
    {
      if (verbose > 1)
	printf ("filename too long\n");
      return 0;
    }
  sprintf (file_name, "%s%s", dir_name, dir_entry->d_name);
  return file_name;
}

#if defined(WIN32_SCSI)
/* Return a list of potential scanners. There's a lot of hardcoded values here that might break on a system with lots of scsi devices. */
static char **build_scsi_dev_list(void)
{
	char **dev_list;
	int dev_list_index;
	int hca;
	HANDLE fd;
	char scsi_hca_name[20];
	char buffer[4096];
	DWORD BytesReturned;
	BOOL ret;
	size_t dev_list_size;
	PSCSI_ADAPTER_BUS_INFO adapter;
	PSCSI_INQUIRY_DATA inquiry;
	int i;

	/* Allocate room for about 100 scanners. That should be enough. */
	dev_list_size = 100;
	dev_list_index = 0;
	dev_list = calloc(1, dev_list_size * sizeof(char *));

	hca = 0;

	for(hca = 0; ; hca++) {

		/* Open the adapter */
		snprintf(scsi_hca_name, 20, "\\\\.\\Scsi%d:", hca);
		fd = CreateFile(scsi_hca_name, GENERIC_READ | GENERIC_WRITE,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL, OPEN_EXISTING,
						FILE_FLAG_RANDOM_ACCESS, NULL );

		if (fd == INVALID_HANDLE_VALUE) {
			/* Assume there is no more adapter. This is wrong in the case
			 * of hot-plug stuff, but I have yet to see it on a user
			 * machine. */
			break;
		}

		/* Get the inquiry info for the devices on that hca. */
        ret = DeviceIoControl(fd,
							  IOCTL_SCSI_GET_INQUIRY_DATA,
							  NULL,
							  0,
							  buffer,
							  sizeof(buffer),
							  &BytesReturned,
							  FALSE);

        if(ret == 0)
			{
				CloseHandle(fd);
				continue;
			}

		adapter = (PSCSI_ADAPTER_BUS_INFO)buffer;

		for(i = 0; i < adapter->NumberOfBuses; i++) {	

			if (adapter->BusData[i].InquiryDataOffset == 0) {
				/* No device here */
				continue;
			}

			inquiry = (PSCSI_INQUIRY_DATA) (buffer + 
											adapter->BusData[i].InquiryDataOffset);
			while(1) {
				/* Check if it is a scanner or a processor
				 * device. Ignore the other
				 * device types. */
				if (inquiry->InquiryDataLength >= 5 &&
					((inquiry->InquiryData[0] & 0x1f) == 3 ||
					 (inquiry->InquiryData[0] & 0x1f) == 6)) {
					char device_name[20];
					sprintf(device_name, "h%db%dt%dl%d", hca, inquiry->PathId, inquiry->TargetId, inquiry->Lun);
					dev_list[dev_list_index] = strdup(device_name);
					dev_list_index++;
				}
			
				if (inquiry->NextInquiryDataOffset == 0) {
					/* No device here */
					break;
				} else {
					inquiry =  (PSCSI_INQUIRY_DATA) (buffer +
													 inquiry->NextInquiryDataOffset);
				}
			}
	    }

		CloseHandle(fd);

	}

	return dev_list;

}
#endif

#if defined (HAVE_IOKIT_CDB_IOSCSILIB_H) || \
    defined (HAVE_IOKIT_SCSI_SCSICOMMANDOPERATIONCODES_H) || \
    defined (HAVE_IOKIT_SCSI_COMMANDS_SCSICOMMANDOPERATIONCODES_H)
char **scsi_dev_list;
int scsi_dev_list_index;

static SANE_Status AddToSCSIDeviceList (const char *dev) {
  if (scsi_dev_list_index < 99) {
    scsi_dev_list [scsi_dev_list_index] = strdup (dev);
    scsi_dev_list_index++;
    return SANE_STATUS_GOOD;
  }
  else
    return SANE_STATUS_NO_MEM;
}

static char **build_scsi_dev_list(void)
{
  scsi_dev_list_index = 0;
  scsi_dev_list = malloc (100 * sizeof(char *));
  sanei_scsi_find_devices (NULL, NULL, NULL, -1, -1, -1, -1,
			   AddToSCSIDeviceList);
  scsi_dev_list [scsi_dev_list_index] = NULL;
  return scsi_dev_list;
}
#endif

static int
check_mustek_pp_device (void)
{
  const char **devices;
  int ctr = 0, found = 0, scsi = 0;

  if (verbose > 1)
    printf ("searching for Mustek parallel port scanners:\n");

  devices = sanei_pa4s2_devices ();

  while (devices[ctr] != NULL) {
    int fd;
    SANE_Status result;

    /* ordinary parallel port scanner type */
    if (verbose > 1)
      printf ("checking %s...", devices[ctr]);

    result = sanei_pa4s2_open (devices[ctr], &fd);
    
    if (verbose > 1)
      {
        if (result != 0)
  	  printf (" failed to open (%s)\n", sane_strstatus (result));
        else
	  printf (" open ok\n");
      }

    if (result == 0) {
      printf ("found possible Mustek parallel port scanner at \"%s\"\n",
              devices[ctr]);
      found++;
      sanei_pa4s2_close(fd);
    }
    
    /* trying scsi over pp devices */
    if (verbose > 1)
      printf ("checking %s (SCSI emulation)...", devices[ctr]);

    result = sanei_pa4s2_scsi_pp_open (devices[ctr], &fd);
    
    if (verbose > 1)
      {
        if (result != 0)
  	  printf (" failed to open (%s)\n", sane_strstatus (result));
        else
	  printf (" open ok\n");
      }

    if (result == 0) {
      printf ("found possible Mustek SCSI over PP scanner at \"%s\"\n",
              devices[ctr]);
      scsi++;
      sanei_pa4s2_close(fd);
    }

    ctr++;
  }

  free(devices);

  if (found > 0 && verbose > 0)
    printf("\n  # Your Mustek parallel port scanner was detected.  It may or\n"
           "  # may not be supported by SANE.  Please read the sane-mustek_pp\n"
	   "  # man-page for setup instructions.\n");

  if (scsi > 0 && verbose > 0)
    printf("\n  # Your Mustek parallel port scanner was detected.  It may or\n"
           "  # may not be supported by SANE.  Please read the sane-mustek_pp\n"
	   "  # man-page for setup instructions.\n");

  return (found > 0 || scsi > 0);
}

#ifdef HAVE_LIBUSB
static SANE_Bool
parse_num (char* search, const char* line, int base, long int * number)
{
  char* start_number;

  start_number = strstr (line, search);
  if (start_number == NULL)
    return SANE_FALSE;
  start_number += strlen (search);
 
  *number = strtol (start_number, NULL, base);
  if (verbose > 2)
    printf ("Found %s%ld\n", search, *number);
  return SANE_TRUE;
}

static SANE_Bool
parse_bcd (char* search, const char* line, long int * number)
{
  char* start_number;
  char* end_number;
  int first_part;
  int second_part;

  start_number = strstr (line, search);
  if (start_number == NULL)
    return SANE_FALSE;
  start_number += strlen (search);
 
  first_part = strtol (start_number, &end_number, 10);
  start_number = end_number + 1; /* skip colon */
  second_part = strtol (start_number, NULL, 10);
  *number = ((first_part / 10) << 12) + ((first_part % 10) << 8) 
    + ((second_part / 10) << 4) + (second_part % 10);
  if (verbose > 2)
    printf ("Found %s%ld\n", search, *number);
  return SANE_TRUE;
}

static void
parse_file (char *filename)
{
  FILE * parsefile;
  char line [PATH_MAX], *token;
  const char * p;
  struct usb_device *dev = 0;
  long int number = 0;
  int current_config = 1;
  int current_if = -1;
  int current_as = -1;
  int current_ep = -1;

  if (verbose > 1)
    printf ("trying to open %s\n", filename);
  parsefile = fopen (filename, "r");

  if (parsefile == NULL)
    {
      if (verbose > 0)
	printf ("opening %s failed: %s\n", filename, strerror (errno));
      return;
    }

  while (sanei_config_read (line, PATH_MAX, parsefile))
    {
      if (verbose > 2)
	printf ("parsing line: `%s'\n", line);
      p = sanei_config_get_string (line, &token);
      if (!token || !p || token[0] == '\0')
	continue;
      if (token[1] != ':')
	{
	  if (verbose > 2)
	    printf ("missing `:'?\n");
	  continue;
	}
      switch (token[0])
	{
	case 'T':
	  if (dev)
	    check_libusb_device (dev, SANE_TRUE);
	  dev = calloc (1, sizeof (struct usb_device));
	  dev->bus = calloc (1, sizeof (struct usb_bus));
	  current_config = 1;
	  current_if = -1;
	  current_as = -1;
	  current_ep = -1;
	  break;
	case 'D':
	  if (parse_bcd ("Ver=", line, &number))
	    dev->descriptor.bcdUSB = number;
	  if (parse_num ("Cls=", line, 16, &number))
	    dev->descriptor.bDeviceClass = number;
	  if (parse_num ("Sub=", line, 16, &number))
	    dev->descriptor.bDeviceSubClass = number;
	  if (parse_num ("Prot=", line, 16, &number))
	    dev->descriptor.bDeviceProtocol = number;
	  if (parse_num ("MxPS=", line, 10, &number))
	    dev->descriptor.bMaxPacketSize0 = number;
	  if (parse_num ("#Cfgs=", line, 10, &number))
	    dev->descriptor.bNumConfigurations = number;
	  dev->config = calloc (number, sizeof (struct usb_config_descriptor));
	  break;
	case 'P':
	  if (parse_num ("Vendor=", line, 16, &number))
	    dev->descriptor.idVendor = number;
	  if (parse_num ("ProdID=", line, 16, &number))
	    dev->descriptor.idProduct = number;
	  if (parse_bcd ("Rev=", line, &number))
	    dev->descriptor.bcdDevice = number;
	  break;
	case 'C':
	  current_if = -1;
	  current_as = -1;
	  current_ep = -1;
	  if (parse_num ("Cfg#=", line, 10, &number))
	    {
	      current_config = number - 1;
	      dev->config[current_config].bConfigurationValue = number;
	    }
	  if (parse_num ("Ifs=", line, 10, &number))
	    dev->config[current_config].bNumInterfaces = number;
	  dev->config[current_config].interface 
	    = calloc (number, sizeof (struct usb_interface));
	  if (parse_num ("Atr=", line, 16, &number))
	    dev->config[current_config].bmAttributes = number;
	  if (parse_num ("MxPwr=", line, 10, &number))
	    dev->config[current_config].MaxPower = number / 2;
	  break;
	case 'I':
	  current_ep = -1;
	  if (parse_num ("If#=", line, 10, &number))
	    {
	      if (current_if != number)
		{
		  current_if = number;
		  current_as = -1;
		  dev->config[current_config].interface[current_if].altsetting
		    = calloc (20, sizeof (struct usb_interface_descriptor));
		  /* Can't read number of altsettings */
		  dev->config[current_config].interface[current_if].num_altsetting = 1;
		}
	      else
		dev->config[current_config].interface[current_if].num_altsetting++;
	    }
	  if (parse_num ("Alt=", line, 10, &number))
	    {
	      current_as = number;
	      dev->config[current_config].interface[current_if].altsetting[current_as].bInterfaceNumber 
		= current_if;
	      dev->config[current_config].interface[current_if].altsetting[current_as].bAlternateSetting 
		= current_as;
	    }
	  if (parse_num ("#EPs=", line, 10, &number))
	    dev->config[current_config].interface[current_if].altsetting[current_as].bNumEndpoints 
	      = number;
	  dev->config[current_config].interface[current_if].altsetting[current_as].endpoint 
	    = calloc (number, sizeof (struct usb_endpoint_descriptor));
	  if (parse_num ("Cls=", line, 16, &number))
	    dev->config[current_config].interface[current_if].altsetting[current_as].bInterfaceClass 
	      = number;
	  if (parse_num ("Sub=", line, 16, &number))
	    dev->config[current_config].interface[current_if].altsetting[current_as].bInterfaceSubClass 
	      = number;
	  if (parse_num ("Prot=", line, 16, &number))
	    dev->config[current_config].interface[current_if].altsetting[current_as].bInterfaceProtocol 
	      = number;
	  break;
	case 'E':
	  current_ep++;
	  if (parse_num ("Ad=", line, 16, &number))
	    dev->config[current_config].interface[current_if].altsetting[current_as]
	      .endpoint[current_ep].bEndpointAddress = number;
	  if (parse_num ("Atr=", line, 16, &number))
	    dev->config[current_config].interface[current_if].altsetting[current_as]
	      .endpoint[current_ep].bmAttributes = number;
	  if (parse_num ("MxPS=", line, 10, &number))
	    dev->config[current_config].interface[current_if].altsetting[current_as]
	      .endpoint[current_ep].wMaxPacketSize = number;
	  if (parse_num ("Ivl=", line, 10, &number))
	    dev->config[current_config].interface[current_if].altsetting[current_as]
	      .endpoint[current_ep].bInterval = number;
	  break;
	case 'S':
	case 'B':
	  continue;
	default:
	  if (verbose > 1)
	    printf ("ignoring unknown line identifier: %c\n", token[0]);
	  continue;
	}
      free (token);
    }
  if (dev)
    check_libusb_device (dev, SANE_TRUE);
  fclose (parsefile);
  return;
}
#endif

int
main (int argc, char **argv)
{
  char **dev_list, **usb_dev_list, *dev_name, **ap;
  int enable_pp_checks = SANE_FALSE;

  prog_name = strrchr (argv[0], '/');
  if (prog_name)
    ++prog_name;
  else
    prog_name = argv[0];

  for (ap = argv + 1; ap < argv + argc; ++ap)
    {
      if ((*ap)[0] != '-')
	break;
      switch ((*ap)[1])
	{
	case '?':
	case 'h':
	  usage (0);
	  exit (0);

	case 'v':
	  ++verbose;
	  break;

	case 'q':
	  --verbose;
	  break;

	case 'f':
	  force = SANE_TRUE;
	  break;

	case 'p':
	  enable_pp_checks = SANE_TRUE;
	  break;

	case 'F':
#ifdef HAVE_LIBUSB
	  parse_file ((char *) (*(++ap)));
#elif defined(HAVE_LIBUSB_1_0)
	  printf ("option -F not implemented with libusb-1.0\n");
#else
	  printf ("libusb not available: option -F can't be used\n");
#endif
	  exit (0);

	default:
	  printf ("unknown option: -%c, try -h for help\n", (*ap)[1]);
	  exit (0);
	}
    }
  if (ap < argv + argc)
    {
      dev_list = ap;
      usb_dev_list = ap;
    }
  else
    {
      static char *default_dev_list[] = {
#if defined(__sgi)
	"/dev/scsi/sc0d1l0", "/dev/scsi/sc0d2l0",
	"/dev/scsi/sc0d3l0", "/dev/scsi/sc0d4l0",
	"/dev/scsi/sc0d5l0", "/dev/scsi/sc0d6l0",
	"/dev/scsi/sc0d7l0", "/dev/scsi/sc0d8l0",
	"/dev/scsi/sc0d9l0", "/dev/scsi/sc0d10l0",
	"/dev/scsi/sc0d11l0", "/dev/scsi/sc0d12l0",
	"/dev/scsi/sc0d13l0", "/dev/scsi/sc0d14l0",
	"/dev/scsi/sc0d15l0",
	"/dev/scsi/sc1d1l0", "/dev/scsi/sc1d2l0",
	"/dev/scsi/sc1d3l0", "/dev/scsi/sc1d4l0",
	"/dev/scsi/sc1d5l0", "/dev/scsi/sc1d6l0",
	"/dev/scsi/sc1d7l0", "/dev/scsi/sc1d8l0",
	"/dev/scsi/sc1d9l0", "/dev/scsi/sc1d10l0",
	"/dev/scsi/sc1d11l0", "/dev/scsi/sc1d12l0",
	"/dev/scsi/sc1d13l0", "/dev/scsi/sc1d14l0",
	"/dev/scsi/sc1d15l0",
	"/dev/scsi/sc2d1l0", "/dev/scsi/sc2d2l0",
	"/dev/scsi/sc2d3l0", "/dev/scsi/sc2d4l0",
	"/dev/scsi/sc2d5l0", "/dev/scsi/sc2d6l0",
	"/dev/scsi/sc2d7l0", "/dev/scsi/sc2d8l0",
	"/dev/scsi/sc2d9l0", "/dev/scsi/sc2d10l0",
	"/dev/scsi/sc2d11l0", "/dev/scsi/sc2d12l0",
	"/dev/scsi/sc2d13l0", "/dev/scsi/sc2d14l0",
	"/dev/scsi/sc2d15l0",
	"/dev/scsi/sc3d1l0", "/dev/scsi/sc3d2l0",
	"/dev/scsi/sc3d3l0", "/dev/scsi/sc3d4l0",
	"/dev/scsi/sc3d5l0", "/dev/scsi/sc3d6l0",
	"/dev/scsi/sc3d7l0", "/dev/scsi/sc3d8l0",
	"/dev/scsi/sc3d9l0", "/dev/scsi/sc3d10l0",
	"/dev/scsi/sc3d11l0", "/dev/scsi/sc3d12l0",
	"/dev/scsi/sc3d13l0", "/dev/scsi/sc3d14l0",
	"/dev/scsi/sc3d15l0",
	"/dev/scsi/sc4d1l0", "/dev/scsi/sc4d2l0",
	"/dev/scsi/sc4d3l0", "/dev/scsi/sc4d4l0",
	"/dev/scsi/sc4d5l0", "/dev/scsi/sc4d6l0",
	"/dev/scsi/sc4d7l0", "/dev/scsi/sc4d8l0",
	"/dev/scsi/sc4d9l0", "/dev/scsi/sc4d10l0",
	"/dev/scsi/sc4d11l0", "/dev/scsi/sc4d12l0",
	"/dev/scsi/sc4d13l0", "/dev/scsi/sc4d14l0",
	"/dev/scsi/sc4d15l0",
	"/dev/scsi/sc5d1l0", "/dev/scsi/sc5d2l0",
	"/dev/scsi/sc5d3l0", "/dev/scsi/sc5d4l0",
	"/dev/scsi/sc5d5l0", "/dev/scsi/sc5d6l0",
	"/dev/scsi/sc5d7l0", "/dev/scsi/sc5d8l0",
	"/dev/scsi/sc5d9l0", "/dev/scsi/sc5d10l0",
	"/dev/scsi/sc5d11l0", "/dev/scsi/sc5d12l0",
	"/dev/scsi/sc5d13l0", "/dev/scsi/sc5d14l0",
	"/dev/scsi/sc5d15l0",
	"/dev/scsi/sc6d1l0", "/dev/scsi/sc6d2l0",
	"/dev/scsi/sc6d3l0", "/dev/scsi/sc6d4l0",
	"/dev/scsi/sc6d5l0", "/dev/scsi/sc6d6l0",
	"/dev/scsi/sc6d7l0", "/dev/scsi/sc6d8l0",
	"/dev/scsi/sc6d9l0", "/dev/scsi/sc6d10l0",
	"/dev/scsi/sc6d11l0", "/dev/scsi/sc6d12l0",
	"/dev/scsi/sc6d13l0", "/dev/scsi/sc6d14l0",
	"/dev/scsi/sc6d15l0",
	"/dev/scsi/sc7d1l0", "/dev/scsi/sc7d2l0",
	"/dev/scsi/sc7d3l0", "/dev/scsi/sc7d4l0",
	"/dev/scsi/sc7d5l0", "/dev/scsi/sc7d6l0",
	"/dev/scsi/sc7d7l0", "/dev/scsi/sc7d8l0",
	"/dev/scsi/sc7d9l0", "/dev/scsi/sc7d10l0",
	"/dev/scsi/sc7d11l0", "/dev/scsi/sc7d12l0",
	"/dev/scsi/sc7d13l0", "/dev/scsi/sc7d14l0",
	"/dev/scsi/sc7d15l0",
	"/dev/scsi/sc8d1l0", "/dev/scsi/sc8d2l0",
	"/dev/scsi/sc8d3l0", "/dev/scsi/sc8d4l0",
	"/dev/scsi/sc8d5l0", "/dev/scsi/sc8d6l0",
	"/dev/scsi/sc8d7l0", "/dev/scsi/sc8d8l0",
	"/dev/scsi/sc8d9l0", "/dev/scsi/sc8d10l0",
	"/dev/scsi/sc8d11l0", "/dev/scsi/sc8d12l0",
	"/dev/scsi/sc8d13l0", "/dev/scsi/sc8d14l0",
	"/dev/scsi/sc8d15l0",
	"/dev/scsi/sc9d1l0", "/dev/scsi/sc9d2l0",
	"/dev/scsi/sc9d3l0", "/dev/scsi/sc9d4l0",
	"/dev/scsi/sc9d5l0", "/dev/scsi/sc9d6l0",
	"/dev/scsi/sc9d7l0", "/dev/scsi/sc9d8l0",
	"/dev/scsi/sc9d9l0", "/dev/scsi/sc9d10l0",
	"/dev/scsi/sc9d11l0", "/dev/scsi/sc9d12l0",
	"/dev/scsi/sc9d13l0", "/dev/scsi/sc9d14l0",
	"/dev/scsi/sc9d15l0",
	"/dev/scsi/sc10d1l0", "/dev/scsi/sc10d2l0",
	"/dev/scsi/sc10d3l0", "/dev/scsi/sc10d4l0",
	"/dev/scsi/sc10d5l0", "/dev/scsi/sc10d6l0",
	"/dev/scsi/sc10d7l0", "/dev/scsi/sc10d8l0",
	"/dev/scsi/sc10d9l0", "/dev/scsi/sc10d10l0",
	"/dev/scsi/sc10d11l0", "/dev/scsi/sc10d12l0",
	"/dev/scsi/sc10d13l0", "/dev/scsi/sc10d14l0",
	"/dev/scsi/sc10d15l0",
	"/dev/scsi/sc11d1l0", "/dev/scsi/sc11d2l0",
	"/dev/scsi/sc11d3l0", "/dev/scsi/sc11d4l0",
	"/dev/scsi/sc11d5l0", "/dev/scsi/sc11d6l0",
	"/dev/scsi/sc11d7l0", "/dev/scsi/sc11d8l0",
	"/dev/scsi/sc11d9l0", "/dev/scsi/sc11d10l0",
	"/dev/scsi/sc11d11l0", "/dev/scsi/sc11d12l0",
	"/dev/scsi/sc11d13l0", "/dev/scsi/sc11d14l0",
	"/dev/scsi/sc11d15l0",
	"/dev/scsi/sc12d1l0", "/dev/scsi/sc12d2l0",
	"/dev/scsi/sc12d3l0", "/dev/scsi/sc12d4l0",
	"/dev/scsi/sc12d5l0", "/dev/scsi/sc12d6l0",
	"/dev/scsi/sc12d7l0", "/dev/scsi/sc12d8l0",
	"/dev/scsi/sc12d9l0", "/dev/scsi/sc12d10l0",
	"/dev/scsi/sc12d11l0", "/dev/scsi/sc12d12l0",
	"/dev/scsi/sc12d13l0", "/dev/scsi/sc12d14l0",
	"/dev/scsi/sc12d15l0",
	"/dev/scsi/sc13d1l0", "/dev/scsi/sc13d2l0",
	"/dev/scsi/sc13d3l0", "/dev/scsi/sc13d4l0",
	"/dev/scsi/sc13d5l0", "/dev/scsi/sc13d6l0",
	"/dev/scsi/sc13d7l0", "/dev/scsi/sc13d8l0",
	"/dev/scsi/sc13d9l0", "/dev/scsi/sc13d10l0",
	"/dev/scsi/sc13d11l0", "/dev/scsi/sc13d12l0",
	"/dev/scsi/sc13d13l0", "/dev/scsi/sc13d14l0",
	"/dev/scsi/sc13d15l0",
	"/dev/scsi/sc14d1l0", "/dev/scsi/sc14d2l0",
	"/dev/scsi/sc14d3l0", "/dev/scsi/sc14d4l0",
	"/dev/scsi/sc14d5l0", "/dev/scsi/sc14d6l0",
	"/dev/scsi/sc14d7l0", "/dev/scsi/sc14d8l0",
	"/dev/scsi/sc14d9l0", "/dev/scsi/sc14d10l0",
	"/dev/scsi/sc14d11l0", "/dev/scsi/sc14d12l0",
	"/dev/scsi/sc14d13l0", "/dev/scsi/sc14d14l0",
	"/dev/scsi/sc14d15l0",
	"/dev/scsi/sc15d1l0", "/dev/scsi/sc15d2l0",
	"/dev/scsi/sc15d3l0", "/dev/scsi/sc15d4l0",
	"/dev/scsi/sc15d5l0", "/dev/scsi/sc15d6l0",
	"/dev/scsi/sc15d7l0", "/dev/scsi/sc15d8l0",
	"/dev/scsi/sc15d9l0", "/dev/scsi/sc15d10l0",
	"/dev/scsi/sc15d11l0", "/dev/scsi/sc15d12l0",
	"/dev/scsi/sc15d13l0", "/dev/scsi/sc15d14l0",
	"/dev/scsi/sc15d15l0",
#elif defined(__EMX__)
	"b0t0l0", "b0t1l0", "b0t2l0", "b0t3l0",
	"b0t4l0", "b0t5l0", "b0t6l0", "b0t7l0",
	"b1t0l0", "b1t1l0", "b1t2l0", "b1t3l0",
	"b1t4l0", "b1t5l0", "b1t6l0", "b1t7l0",
	"b2t0l0", "b2t1l0", "b2t2l0", "b2t3l0",
	"b2t4l0", "b2t5l0", "b2t6l0", "b2t7l0",
	"b3t0l0", "b3t1l0", "b3t2l0", "b3t3l0",
	"b3t4l0", "b3t5l0", "b3t6l0", "b3t7l0",
#elif defined(__linux__)
	"/dev/scanner",
	"/dev/sg0", "/dev/sg1", "/dev/sg2", "/dev/sg3",
	"/dev/sg4", "/dev/sg5", "/dev/sg6", "/dev/sg7",
	"/dev/sg8", "/dev/sg9",
	"/dev/sga", "/dev/sgb", "/dev/sgc", "/dev/sgd",
	"/dev/sge", "/dev/sgf", "/dev/sgg", "/dev/sgh",
	"/dev/sgi", "/dev/sgj", "/dev/sgk", "/dev/sgl",
	"/dev/sgm", "/dev/sgn", "/dev/sgo", "/dev/sgp",
	"/dev/sgq", "/dev/sgr", "/dev/sgs", "/dev/sgt",
	"/dev/sgu", "/dev/sgv", "/dev/sgw", "/dev/sgx",
	"/dev/sgy", "/dev/sgz",
#elif defined(__NeXT__)
	"/dev/sg0a", "/dev/sg0b", "/dev/sg0c", "/dev/sg0d",
	"/dev/sg0e", "/dev/sg0f", "/dev/sg0g", "/dev/sg0h",
	"/dev/sg1a", "/dev/sg1b", "/dev/sg1c", "/dev/sg1d",
	"/dev/sg1e", "/dev/sg1f", "/dev/sg1g", "/dev/sg1h",
	"/dev/sg2a", "/dev/sg2b", "/dev/sg2c", "/dev/sg2d",
	"/dev/sg2e", "/dev/sg2f", "/dev/sg2g", "/dev/sg2h",
	"/dev/sg3a", "/dev/sg3b", "/dev/sg3c", "/dev/sg3d",
	"/dev/sg3e", "/dev/sg3f", "/dev/sg3g", "/dev/sg3h",
#elif defined(_AIX)
	"/dev/scanner",
	"/dev/gsc0", "/dev/gsc1", "/dev/gsc2", "/dev/gsc3",
	"/dev/gsc4", "/dev/gsc5", "/dev/gsc6", "/dev/gsc7",
	"/dev/gsc8", "/dev/gsc9", "/dev/gsc10", "/dev/gsc11",
	"/dev/gsc12", "/dev/gsc13", "/dev/gsc14", "/dev/gsc15",
#elif defined(__sun)
	"/dev/scg0a", "/dev/scg0b", "/dev/scg0c", "/dev/scg0d",
	"/dev/scg0e", "/dev/scg0f", "/dev/scg0g",
	"/dev/scg1a", "/dev/scg1b", "/dev/scg1c", "/dev/scg1d",
	"/dev/scg1e", "/dev/scg1f", "/dev/scg1g",
	"/dev/scg2a", "/dev/scg2b", "/dev/scg2c", "/dev/scg2d",
	"/dev/scg2e", "/dev/scg2f", "/dev/scg2g",
	"/dev/sg/0", "/dev/sg/1", "/dev/sg/2", "/dev/sg/3",
	"/dev/sg/4", "/dev/sg/5", "/dev/sg/6",
	"/dev/scsi/scanner/", "/dev/scsi/processor/",
#elif defined(HAVE_CAMLIB_H)
	"/dev/scanner", "/dev/scanner0", "/dev/scanner1",
	"/dev/pass0", "/dev/pass1", "/dev/pass2", "/dev/pass3",
	"/dev/pass4", "/dev/pass5", "/dev/pass6", "/dev/pass7",
#elif defined(__FreeBSD__)
	"/dev/uk0", "/dev/uk1", "/dev/uk2", "/dev/uk3", "/dev/uk4",
	"/dev/uk5", "/dev/uk6",
#elif defined(__NetBSD__)
	"/dev/uk0", "/dev/uk1", "/dev/uk2", "/dev/uk3", "/dev/uk4",
	"/dev/uk5", "/dev/uk6",
	"/dev/ss0",
#elif defined(__OpenBSD__)
	"/dev/uk0", "/dev/uk1", "/dev/uk2", "/dev/uk3", "/dev/uk4",
	"/dev/uk5", "/dev/uk6",
#elif defined(__hpux)
	"/dev/rscsi/",
#endif
	0
      };
      static char *usb_default_dev_list[] = {
#if defined(__linux__)
	"/dev/usb/scanner",
	"/dev/usb/scanner0", "/dev/usb/scanner1",
	"/dev/usb/scanner2", "/dev/usb/scanner3",
	"/dev/usb/scanner4", "/dev/usb/scanner5",
	"/dev/usb/scanner5", "/dev/usb/scanner7",
	"/dev/usb/scanner8", "/dev/usb/scanner9",
	"/dev/usb/scanner10", "/dev/usb/scanner11",
	"/dev/usb/scanner12", "/dev/usb/scanner13",
	"/dev/usb/scanner14", "/dev/usb/scanner15",
	"/dev/usbscanner",
	"/dev/usbscanner0", "/dev/usbscanner1",
	"/dev/usbscanner2", "/dev/usbscanner3",
	"/dev/usbscanner4", "/dev/usbscanner5",
	"/dev/usbscanner6", "/dev/usbscanner7",
	"/dev/usbscanner8", "/dev/usbscanner9",
	"/dev/usbscanner10", "/dev/usbscanner11",
	"/dev/usbscanner12", "/dev/usbscanner13",
	"/dev/usbscanner14", "/dev/usbscanner15",
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
	"/dev/uscanner",
	"/dev/uscanner0", "/dev/uscanner1",
	"/dev/uscanner2", "/dev/uscanner3",
	"/dev/uscanner4", "/dev/uscanner5",
	"/dev/uscanner6", "/dev/uscanner7",
	"/dev/uscanner8", "/dev/uscanner9",
	"/dev/uscanner10", "/dev/uscanner11",
	"/dev/uscanner12", "/dev/uscanner13",
	"/dev/uscanner14", "/dev/uscanner15",
#endif
	0
      };

#if defined (WIN32_SCSI) || \
    defined (HAVE_IOKIT_CDB_IOSCSILIB_H) || \
    defined (HAVE_IOKIT_SCSI_SCSICOMMANDOPERATIONCODES_H) || \
    defined (HAVE_IOKIT_SCSI_COMMANDS_SCSICOMMANDOPERATIONCODES_H)
   /* Build a list of valid of possible scanners found */
      dev_list = build_scsi_dev_list();
#else
      dev_list = default_dev_list;
#endif

      usb_dev_list = usb_default_dev_list;
    }
  if (verbose > 1)
    printf ("This is sane-find-scanner from %s\n", PACKAGE_STRING);

  if (verbose > 0)
    printf ("\n  # sane-find-scanner will now attempt to detect your scanner. If the"
	    "\n  # result is different from what you expected, first make sure your"
	    "\n  # scanner is powered up and properly connected to your computer.\n\n");

  if (verbose > 1)
    printf ("searching for SCSI scanners:\n");

  while ((dev_name = *dev_list++))
    {
      if (strlen (dev_name) == 0)
	continue;		/* Empty device names ... */

      if (dev_name[strlen (dev_name) - 1] == '/')
	{
	  /* check whole directories */
	  DIR *dir;
	  char *file_name;

	  dir = scan_directory (dev_name);
	  if (!dir)
	    continue;

	  while ((file_name = get_next_file (dev_name, dir)))
	    check_scsi_file (file_name);
	}
      else
	{
	  /* check device files */
	  check_scsi_file (dev_name);
	}
    }
  if (device_found)
    {
      if (verbose > 0)
	printf
	  ("  # Your SCSI scanner was detected. It may or may not be "
	   "supported by SANE. Try\n  # scanimage -L and read the backend's "
	   "manpage.\n");
    }
  else
    {
      if (verbose > 0)
	printf
	  ("  # No SCSI scanners found. If you expected something different, "
	   "make sure that\n  # you have loaded a kernel SCSI driver for your SCSI "
	   "adapter.\n");
      if (!check_sg ())
	{
	  if (verbose > 0)
	    printf
	      ("  # Also you need support for SCSI Generic (sg) in your "
	       "operating system.\n  # If using Linux, try \"modprobe "
	       "sg\".\n");
	}
    }
  if (verbose > 0)
    printf ("\n");
  device_found = SANE_FALSE;
  sanei_usb_init ();
  if (verbose > 1)
    printf ("searching for USB scanners:\n");

  while ((dev_name = *usb_dev_list++))
    {
      if (strlen (dev_name) == 0)
	continue;		/* Empty device names ... */

      if (dev_name[strlen (dev_name) - 1] == '/')
	{
	  /* check whole directories */
	  DIR *dir;
	  char *file_name;

	  dir = scan_directory (dev_name);
	  if (!dir)
	    continue;

	  while ((file_name = get_next_file (dev_name, dir)))
	    check_usb_file (file_name);
	}
      else
	{
	  /* check device files */
	  check_usb_file (dev_name);
	}
    }
#ifdef HAVE_LIBUSB
  /* Now the libusb devices */
  {
    struct usb_bus *bus;
    struct usb_device *dev;

    if (ap < argv + argc)
      {
	/* user-specified devices not useful for libusb */
	if (verbose > 1)
	  printf ("ignoring libusb devices\n");
      }
    else
      {
	if (verbose > 2)
	  printf ("trying libusb:\n");
	for (bus = usb_get_busses (); bus; bus = bus->next)
	  {
	    for (dev = bus->devices; dev; dev = dev->next)
	      {
		check_libusb_device (dev, SANE_FALSE);
	      }			/* for (dev) */
	  }			/* for (bus) */
      }
  }
#elif defined(HAVE_LIBUSB_1_0)
  /* Now the libusb-1.0 devices */
  {
    if (ap < argv + argc)
      {
	/* user-specified devices not useful for libusb */
	if (verbose > 1)
	  printf ("ignoring libusb devices\n");
      }
    else
      {
	libusb_device **devlist;
	ssize_t devcnt;
	int i;
	int ret;

	if (verbose > 2)
	  printf ("trying libusb:\n");

	ret = libusb_init (&sfs_usb_ctx);
	if (ret < 0)
	  {
	    printf ("# Could not initialize libusb-1.0, error %d\n", ret);
	    printf ("# Skipping libusb devices\n");

	    goto failed_libusb_1_0;
	  }

	if (verbose > 3)
	  libusb_set_debug (sfs_usb_ctx, 3);

	devcnt = libusb_get_device_list (sfs_usb_ctx, &devlist);
	if (devcnt < 0)
	  {
	    printf ("# Could not get device list, error %d\n", ret);

	    goto deinit_libusb_1_0;
	  }

	for (i = 0; i < devcnt; i++)
	  {
	    check_libusb_device (devlist[i], SANE_FALSE);
	  }

	libusb_free_device_list (devlist, 1);

      deinit_libusb_1_0:
	libusb_exit (sfs_usb_ctx);

      failed_libusb_1_0:
	; /* init failed, jumping here */
      }
  }
#else /* not HAVE_LIBUSB && not HAVE_LIBUSB_1_0 */
  if (verbose > 1)
    printf ("libusb not available\n");
#endif /* not HAVE_LIBUSB && not HAVE_LIBUSB_1_0 */

  if (device_found)
    {
      if (libusb_device_found)
	{
	  if (verbose > 0)
	    printf
	      ("  # Your USB scanner was (probably) detected. It "
	       "may or may not be supported by\n  # SANE. Try scanimage "
	       "-L and read the backend's manpage.\n");
	}
      else if (verbose > 0)
	printf
	  ("  # Your USB scanner was detected. It may or may not "
	   "be supported by\n  # SANE. Try scanimage -L and read the "
	   "backend's manpage.\n");
      if (unknown_found && verbose > 0)
	printf
	  ("  # `UNKNOWN vendor and product' means that there seems to be a "
	   "scanner at this\n  # device file but the vendor and product ids "
	   "couldn't be identified.\n  # Currently identification only works "
	   "with Linux versions >= 2.4.8. You may\n  # need to configure your "
	   "backend manually, see the backend's manpage.\n");
    }
  else
    {
      if (verbose > 0)
	printf
	  ("  # No USB scanners found. If you expected something different, "
	   "make sure that\n  # you have loaded a kernel driver for your USB host "
	   "controller and have setup\n  # the USB system correctly. "
	   "See man sane-usb for details.\n");
#if !defined(HAVE_LIBUSB) && !defined(HAVE_LIBUSB_1_0)
      if (verbose > 0)
	printf ("  # SANE has been built without libusb support. This may be a "
		"reason\n  # for not detecting USB scanners. Read README for "
		"more details.\n");
#endif
    }
  if (enable_pp_checks == SANE_TRUE) 
    {
      if (!check_mustek_pp_device() && verbose > 0)
        printf ("\n  # No Mustek parallel port scanners found. If you expected"
                " something\n  # different, make sure the scanner is correctly"
	        " connected to your computer\n  # and you have apropriate"
	        " access rights.\n");
    }
  else if (verbose > 0)
    printf ("\n  # Not checking for parallel port scanners.\n");
  if (verbose > 0)
    printf ("\n  # Most Scanners connected to the parallel port or other "
	    "proprietary ports\n  # can't be detected by this program.\n");
#ifdef HAVE_GETUID
  if (getuid ())
    if (verbose > 0)
      printf
	("\n  # You may want to run this program as root to find all devices. "
	 "Once you\n  # found the scanner devices, be sure to adjust access "
	 "permissions as\n  # necessary.\n");
#endif

  if (verbose > 1)
    printf ("done\n");

  return 0;
}
