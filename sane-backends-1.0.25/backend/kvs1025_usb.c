/*
   Copyright (C) 2008, Panasonic Russia Ltd.
*/
/* sane - Scanner Access Now Easy.
   Panasonic KV-S1020C / KV-S1025C USB scanners.
*/

#define DEBUG_DECLARE_ONLY

#include "../include/sane/config.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "../include/sane/sane.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_config.h"
#include "../include/lassert.h"

#include "kvs1025.h"
#include "kvs1025_low.h"
#include "kvs1025_usb.h"
#include "kvs1025_cmds.h"

#include "../include/sane/sanei_debug.h"

extern PKV_DEV g_devices;	/* Chain of devices */
extern const SANE_Device **g_devlist;

/* static functions */

/* Attach USB scanner */
static SANE_Status
attach_scanner_usb (const char *device_name)
{
  PKV_DEV dev;
  SANE_Word vendor, product;

  DBG (DBG_error, "attaching USB scanner %s\n", device_name);

  sanei_usb_get_vendor_product_byname(device_name,&vendor,&product);

  dev = (PKV_DEV) malloc (sizeof (KV_DEV));

  if (dev == NULL)
    return SANE_STATUS_NO_MEM;

  memset (dev, 0, sizeof (KV_DEV));

  dev->bus_mode = KV_USB_BUS;
  dev->usb_fd = -1;
  dev->scsi_fd = -1;
  strcpy (dev->device_name, device_name);

  dev->buffer0 = (unsigned char *) malloc (SCSI_BUFFER_SIZE + 12);
  dev->buffer = dev->buffer0 + 12;

  if (dev->buffer0 == NULL)
    {
      free (dev);
      return SANE_STATUS_NO_MEM;
    }

  dev->scsi_type = 6;
  strcpy (dev->scsi_type_str, "ADF Scanner");
  strcpy (dev->scsi_vendor, "Panasonic");
  strcpy (dev->scsi_product,
    product == (int) KV_S1020C ? "KV-S1020C" :
    product == (int) KV_S1025C ? "KV-S1025C" :
    product == (int) KV_S1045C ? "KV-S1045C" :
    "KV-S10xxC");
  strcpy (dev->scsi_version, "1.00");

  /* Set SANE_Device */
  dev->sane.name = dev->device_name;
  dev->sane.vendor = dev->scsi_vendor;
  dev->sane.model = dev->scsi_product;
  dev->sane.type = dev->scsi_type_str;

  /* Add into g_devices chain */
  dev->next = g_devices;
  g_devices = dev;

  return SANE_STATUS_GOOD;
}

/* Get all supported scanners, and store into g_devlist */
SANE_Status
kv_usb_enum_devices (void)
{
  int cnt = 0;
  int i;
  PKV_DEV pd;
  char usb_str[18];

  DBG (DBG_proc, "kv_usb_enum_devices: enter\n");

  sanei_usb_init();

  sprintf(usb_str,"usb %#04x %#04x",VENDOR_ID,KV_S1020C);
  sanei_usb_attach_matching_devices(usb_str, attach_scanner_usb);

  sprintf(usb_str,"usb %#04x %#04x",VENDOR_ID,KV_S1025C);
  sanei_usb_attach_matching_devices(usb_str, attach_scanner_usb);

  sprintf(usb_str,"usb %#04x %#04x",VENDOR_ID,KV_S1045C);
  sanei_usb_attach_matching_devices(usb_str, attach_scanner_usb);

  for (pd = g_devices; pd; pd=pd->next) {
    cnt++;
  }

  g_devlist =
    (const SANE_Device **) malloc (sizeof (SANE_Device *) * (cnt + 1));
  if (g_devlist == NULL)
    {
      DBG (DBG_proc,
	   "kv_usb_enum_devices: leave on error " " --out of memory\n");
      return SANE_STATUS_NO_MEM;
    }

  pd = g_devices;
  for (i = 0; i < cnt; i++)
    {
      g_devlist[i] = (const SANE_Device *) &pd->sane;
      pd = pd->next;
    }
  g_devlist[cnt] = 0;

  DBG (DBG_proc, "kv_usb_enum_devices: leave with %d devices.\n", cnt);

  return SANE_STATUS_GOOD;
}

/* Check if device is already open */
SANE_Bool
kv_usb_already_open (PKV_DEV dev)
{
  return (dev->usb_fd > -1);
}

/* Open an USB device */
SANE_Status
kv_usb_open (PKV_DEV dev)
{
  SANE_Status ret;

  DBG (DBG_proc, "kv_usb_open: enter\n");
  if (kv_usb_already_open(dev))
    {
      DBG (DBG_proc, "kv_usb_open: leave -- already open\n");
      return SANE_STATUS_GOOD;
    }

  ret = sanei_usb_open (dev->device_name, &(dev->usb_fd));
  if (ret)
    {
      DBG (DBG_error, "kv_usb_open: leave -- cannot open device\n");
      return SANE_STATUS_IO_ERROR;
    }

  sanei_usb_clear_halt (dev->usb_fd);

  DBG (DBG_proc, "kv_usb_open: leave\n");
  return SANE_STATUS_GOOD;
}

/* Close an USB device */
void
kv_usb_close (PKV_DEV dev)
{
  DBG (DBG_proc, "kv_usb_close: enter\n");
  if (kv_usb_already_open(dev))
    {
      sanei_usb_close(dev->usb_fd);
      dev->usb_fd = -1;
    }
  DBG (DBG_proc, "kv_usb_close: leave\n");
}

/* Clean up the USB bus and release all resources allocated to devices */
void
kv_usb_cleanup (void)
{
}

/* Send command via USB, and get response data */
SANE_Status
kv_usb_escape (PKV_DEV dev,
	       PKV_CMD_HEADER header, unsigned char *status_byte)
{
  int got_response = 0;
  size_t len;
  unsigned char cmd_buff[24];
  memset (cmd_buff, 0, 24);
  cmd_buff[3] = 0x18;		/* container length */
  cmd_buff[5] = 1;		/* container type: command block */
  cmd_buff[6] = 0x90;		/* code */

  if (!kv_usb_already_open(dev))
    {
      DBG (DBG_error, "kv_usb_escape: error, device not open.\n");
      return SANE_STATUS_IO_ERROR;
    }
  memcpy (cmd_buff + 12, header->cdb, header->cdb_size);

  /* change timeout */
  sanei_usb_set_timeout(KV_CMD_TIMEOUT);

  /* Send command */
  len = 24;
  if (sanei_usb_write_bulk (dev->usb_fd, (SANE_Byte *) cmd_buff, &len))
    {
      DBG (DBG_error, "usb_bulk_write: Error writing command.\n");
      hexdump (DBG_error, "cmd block", cmd_buff, 24);
      return SANE_STATUS_IO_ERROR;
    }

  /* Send / Read data */
  if (header->direction == KV_CMD_IN)
    {
      size_t size = header->data_size + 12;
      size_t size_read = size;
      unsigned char *data = ((unsigned char *) header->data) - 12;
      SANE_Status ret;

      ret  = sanei_usb_read_bulk (dev->usb_fd, (SANE_Byte *) data, &size_read);

      /*empty read is ok?*/
      if (ret == SANE_STATUS_EOF){
	sanei_usb_clear_halt (dev->usb_fd);
        ret = SANE_STATUS_GOOD;
      }

      if (ret) {
	sanei_usb_clear_halt (dev->usb_fd);
	DBG (DBG_error, "usb_bulk_read: Error reading data.\n");
	return SANE_STATUS_IO_ERROR;
      }

      if (size_read != size)
	{
	  DBG (DBG_shortread, "usb_bulk_read: Warning - short read\n");
	  DBG (DBG_shortread, "usb_bulk_read: bytes to read = %lu\n", (unsigned long)size);
	  DBG (DBG_shortread,
	       "usb_bulk_read: bytes actual read = %lu\n", (unsigned long)size_read);
	  /*hexdump (DBG_shortread, "data", data, size_read); */
	}
    }

  if (header->direction == KV_CMD_OUT)
    {
      size_t size = header->data_size + 12;
      size_t size_written = size;
      unsigned char *data = ((unsigned char *) header->data) - 12;
      SANE_Status ret;

      memset (data, 0, 12);
      Ito32 (size, data);
      data[5] = 0x02;		/* container type: data block */
      data[6] = 0xb0;		/* code */

      ret = sanei_usb_write_bulk (dev->usb_fd, (SANE_Byte *) data, &size_written);

      /*empty write is ok?*/
      if (ret == SANE_STATUS_EOF){
	sanei_usb_clear_halt (dev->usb_fd);
        ret = SANE_STATUS_GOOD;
      }

      if (ret) {
	sanei_usb_clear_halt (dev->usb_fd);
	DBG (DBG_error, "usb_bulk_write: Error writing data.\n");
	return SANE_STATUS_IO_ERROR;
      }

      if (size_written != size)
	{
	  DBG (DBG_shortread, "usb_bulk_write: Warning - short written\n");
	  DBG (DBG_shortread, "usb_bulk_write: bytes to write = %lu\n", (unsigned long)size);
	  DBG (DBG_shortread,
	       "usb_bulk_write: bytes actual written = %lu\n", (unsigned long)size_written);
	  hexdump (DBG_shortread, "data", data, size_written);
	}
    }

  /* Get response */
  if (!got_response)
    {
      SANE_Status ret;
      size_t len = 16;

      ret = sanei_usb_read_bulk (dev->usb_fd, (SANE_Byte *) cmd_buff, &len);
      
      if (ret || len != 16)
	{
	  DBG (DBG_error, "usb_bulk_read: Error reading response."
	       " read %lu bytes\n", (unsigned long)len);
	  sanei_usb_clear_halt (dev->usb_fd);
	  return SANE_STATUS_IO_ERROR;
	}
    }

  if (cmd_buff[5] != 3)
    {
      DBG (DBG_error, "usb_bulk_read: Invalid response block.\n");
      hexdump (DBG_error, "response", cmd_buff, 16);
      return SANE_STATUS_IO_ERROR;
    }

  *status_byte = cmd_buff[15] & 0x3E;

  return SANE_STATUS_GOOD;
}

/* Send command via USB, and request sense on CHECK CONDITION status */
SANE_Status
kv_usb_send_command (PKV_DEV dev,
		     PKV_CMD_HEADER header, PKV_CMD_RESPONSE response)
{
  unsigned char status = 0;
  SANE_Status s;
  memset (response, 0, sizeof (KV_CMD_RESPONSE));
  response->status = KV_FAILED;

  s = kv_usb_escape (dev, header, &status);

  if (s)
    {
      status = 0x02;
    }

  if (status == 0x02)
    {				/* check condition */
      /* request sense */
      KV_CMD_HEADER hdr;
      memset (&hdr, 0, sizeof (hdr));
      hdr.direction = KV_CMD_IN;
      hdr.cdb[0] = SCSI_REQUEST_SENSE;
      hdr.cdb[4] = 0x12;
      hdr.cdb_size = 6;
      hdr.data_size = 0x12;
      hdr.data = &response->sense;

      if (kv_usb_escape (dev, &hdr, &status) != 0)
	return SANE_STATUS_IO_ERROR;

      hexdump (DBG_error, "sense data", (unsigned char *) &response->sense,
	       0x12);

      response->status = KV_CHK_CONDITION;
    }
  else
    {
      response->status = KV_SUCCESS;
    }
  return SANE_STATUS_GOOD;
}
