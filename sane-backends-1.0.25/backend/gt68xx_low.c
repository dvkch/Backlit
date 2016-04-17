/* sane - Scanner Access Now Easy.

   Copyright (C) 2002 Sergey Vlasov <vsu@altlinux.ru>
   Copyright (C) 2002 - 2007 Henning Geinitz <sane@geinitz.org>
   
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
*/

/** @file
 * @brief Implementation of the low-level scanner interface functions.
 */

#include "gt68xx_low.h"

#include "../include/sane/sane.h"
#include "../include/sane/sanei_debug.h"
#include "../include/sane/sanei_usb.h"

#include <stdlib.h>
#include <string.h>

#ifdef USE_FORK
#include <sys/wait.h>
#include <unistd.h>
#include "gt68xx_shm_channel.c"
#endif

/** Check that the device pointer is not NULL.
 *
 * @param dev       Pointer to the device object (GT68xx_Device).
 * @param func_name Function name (for use in debug messages).
 */
#define CHECK_DEV_NOT_NULL(dev, func_name)                              \
  do {                                                                  \
    IF_DBG(                                                             \
    if (!(dev))                                                         \
      {                                                                 \
        DBG (0, "BUG: NULL device\n");                                  \
        return SANE_STATUS_INVAL;                                       \
      }                                                                 \
     )                                                                  \
  } while (SANE_FALSE)

/** Check that the device is open.
 *
 * @param dev       Pointer to the device object (GT68xx_Device).
 * @param func_name Function name (for use in debug messages).
 */
#define CHECK_DEV_OPEN(dev, func_name)                                  \
  do {                                                                  \
    IF_DBG(                                                             \
    CHECK_DEV_NOT_NULL ((dev), (func_name));                            \
    if ((dev)->fd == -1)                                                \
      {                                                                 \
        DBG (0, "%s: BUG: device %p not open\n", (func_name),           \
             ((void *) dev));                                           \
        return SANE_STATUS_INVAL;                                       \
      }                                                                 \
    )                                                                   \
  } while (SANE_FALSE)

/** Check that the device is open and active.
 *
 * @param dev       Pointer to the device (GT68xx_Device).
 * @param func_name Function name (for use in debug messages).
 */
#define CHECK_DEV_ACTIVE(dev, func_name)                                \
  do {                                                                  \
    IF_DBG(                                                             \
    CHECK_DEV_OPEN ((dev), (func_name));                                \
    if (!(dev)->active)                                                 \
      {                                                                 \
        DBG (0, "%s: BUG: device %p not active\n", (func_name),         \
               ((void *) dev));                                         \
        return SANE_STATUS_INVAL;                                       \
      }                                                                 \
   )                                                                    \
  } while (SANE_FALSE)


#ifndef NDEBUG

/** Dump a request packet for debugging purposes.
 *
 * @param prefix String printed before the packet contents.
 * @param req    The request packet to be dumped.
 */
static void
dump_req (SANE_String_Const prefix, GT68xx_Packet req)
{
  int i;
  char buf[GT68XX_PACKET_SIZE * 3 + 1];

  for (i = 0; i < GT68XX_PACKET_SIZE; ++i)
    sprintf (buf + i * 3, " %02x", req[i]);
  DBG (8, "%s%s\n", prefix, buf);
}

#endif /* not NDEBUG */

/** Dump a request packet if the debug level is at 8 or above.
 *
 * @param prefix String printed before the packet contents.
 * @param req    The request packet to be dumped.
 */
#define DUMP_REQ(prefix, req) \
  do { IF_DBG( if (DBG_LEVEL >= 8) dump_req ((prefix), (req)); ) } while (0)


SANE_Status
gt68xx_device_new (GT68xx_Device ** dev_return)
{
  GT68xx_Device *dev;

  DBG (7, "gt68xx_device_new: enter\n");
  if (!dev_return)
    return SANE_STATUS_INVAL;

  dev = (GT68xx_Device *) malloc (sizeof (GT68xx_Device));

  if (!dev)
    {
      DBG (3, "gt68xx_device_new: couldn't malloc %lu bytes for device\n",
	   (u_long) sizeof (GT68xx_Device));
      *dev_return = 0;
      return SANE_STATUS_NO_MEM;
    }
  *dev_return = dev;

  memset (dev, 0, sizeof (GT68xx_Device));

  dev->fd = -1;
  dev->active = SANE_FALSE;

  dev->model = NULL;
  dev->command_set_private = NULL;

  dev->read_buffer = NULL;
  dev->read_buffer_size = 32768;

  dev->manual_selection = SANE_FALSE;

#ifdef USE_FORK
  dev->shm_channel = NULL;
#endif /* USE_FORK */

  DBG (7, "gt68xx_device_new:: leave: ok\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_device_free (GT68xx_Device * dev)
{
  DBG (7, "gt68xx_device_free: enter: dev=%p\n", (void *) dev);
  if (dev)
    {
      if (dev->active)
	gt68xx_device_deactivate (dev);

      if (dev->fd != -1)
	gt68xx_device_close (dev);

      if (dev->model && dev->model->allocated)
	{
	  DBG (7, "gt68xx_device_free: freeing model data %p\n",
	       (void *) dev->model);
	  free (dev->model);
	}

      DBG (7, "gt68xx_device_free: freeing dev\n");
      free (dev);
    }
  DBG (7, "gt68xx_device_free: leave: ok\n");
  return SANE_STATUS_GOOD;
}

static GT68xx_USB_Device_Entry *
gt68xx_find_usb_device_entry (SANE_Word vendor, SANE_Word product)
{
  GT68xx_USB_Device_Entry *entry;

  for (entry = gt68xx_usb_device_list; entry->model; ++entry)
    {
      if (vendor == entry->vendor && product == entry->product)
	return entry;
    }

  return NULL;
}

static SANE_Status
gt68xx_device_identify (GT68xx_Device * dev)
{
  SANE_Status status;
  SANE_Word vendor, product;
  GT68xx_USB_Device_Entry *entry;

  CHECK_DEV_OPEN (dev, "gt68xx_device_identify");

  status = sanei_usb_get_vendor_product (dev->fd, &vendor, &product);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (3, "gt68xx_device_identify: error getting USB id: %s\n",
	   sane_strstatus (status));
      return status;
    }

  entry = gt68xx_find_usb_device_entry (vendor, product);

  if (entry)
    {
      dev->model = entry->model;
    }
  else
    {
      dev->model = NULL;
      DBG (3, "gt68xx_device_identify: unknown USB device (vendor 0x%04x, "
	   "product 0x%04x)\n", vendor, product);
      return SANE_STATUS_INVAL;
    }

  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_device_open (GT68xx_Device * dev, const char *dev_name)
{
  SANE_Status status;
  SANE_Int fd;

  DBG (7, "gt68xx_device_open: enter: dev=%p\n", (void *) dev);

  CHECK_DEV_NOT_NULL (dev, "gt68xx_device_open");

  if (dev->fd != -1)
    {
      DBG (3, "gt68xx_device_open: device already open\n");
      return SANE_STATUS_INVAL;
    }

  status = sanei_usb_open (dev_name, &fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (3, "gt68xx_device_open: sanei_usb_open failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  dev->fd = fd;

  if (!dev->model)
    gt68xx_device_identify (dev);

  DBG (7, "gt68xx_device_open: leave: ok\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_device_close (GT68xx_Device * dev)
{
  DBG (7, "gt68xx_device_close: enter: dev=%p\n", (void *) dev);

  CHECK_DEV_OPEN (dev, "gt68xx_device_close");

  if (dev->active)
    gt68xx_device_deactivate (dev);

  sanei_usb_close (dev->fd);
  dev->fd = -1;

  DBG (7, "gt68xx_device_close: leave: ok\n");
  return SANE_STATUS_GOOD;
}

SANE_Bool
gt68xx_device_is_configured (GT68xx_Device * dev)
{
  if (dev && dev->model && dev->model->command_set)
    return SANE_TRUE;
  else
    return SANE_FALSE;
}

SANE_Status
gt68xx_device_set_model (GT68xx_Device * dev, GT68xx_Model * model)
{
  if (dev->active)
    {
      DBG (3, "gt68xx_device_set_model: device already active\n");
      return SANE_STATUS_INVAL;
    }

  if (dev->model && dev->model->allocated)
    free (dev->model);

  dev->model = model;

  return SANE_STATUS_GOOD;
}

static SANE_Bool
gt68xx_device_get_model (SANE_String name, GT68xx_Model ** model)
{
  GT68xx_USB_Device_Entry *entry;

  for (entry = gt68xx_usb_device_list; entry->model; ++entry)
    {
      if (strcmp (name, entry->model->name) == 0)
	{
	  *model = entry->model;
	  return SANE_TRUE;
	}
    }
  return SANE_FALSE;
}


SANE_Status
gt68xx_device_activate (GT68xx_Device * dev)
{
  SANE_Status status;
  CHECK_DEV_OPEN (dev, "gt68xx_device_activate");
  if (dev->active)
    {
      DBG (3, "gt68xx_device_activate: device already active\n");
      return SANE_STATUS_INVAL;
    }

  if (!gt68xx_device_is_configured (dev))
    {
      DBG (3, "gt68xx_device_activate: device is not configured\n");
      return SANE_STATUS_INVAL;
    }

  DBG (7, "gt68xx_device_activate: model \"%s\"\n", dev->model->name);
  if (dev->model->command_set->activate)
    {
      status = (*dev->model->command_set->activate) (dev);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (3, "gt68xx_device_activate: command-set-specific "
	       "activate failed: %s\n", sane_strstatus (status));
	  return status;
	}
    }
  dev->afe = malloc (sizeof (*dev->afe));
  dev->exposure = malloc (sizeof (*dev->exposure));
  if (!dev->afe || !dev->exposure)
    return SANE_STATUS_NO_MEM;
  memcpy (dev->afe, &dev->model->afe_params, sizeof (*dev->afe));
  memcpy (dev->exposure, &dev->model->exposure, sizeof (*dev->exposure));
  dev->gamma_value = dev->model->default_gamma_value;
  dev->active = SANE_TRUE;
  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_device_deactivate (GT68xx_Device * dev)
{
  SANE_Status status = SANE_STATUS_GOOD;
  CHECK_DEV_ACTIVE (dev, "gt68xx_device_deactivate");
  if (dev->read_active)
    gt68xx_device_read_finish (dev);
  if (dev->model->command_set->deactivate)
    {
      status = (*dev->model->command_set->deactivate) (dev);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (3,
	       "gt68xx_device_deactivate: command set-specific deactivate failed: %s\n",
	       sane_strstatus (status));
	  /* proceed with deactivate anyway */
	}
    }
  if (dev->afe)
    free (dev->afe);
  dev->afe = 0;
  if (dev->exposure)
    free (dev->exposure);
  dev->exposure = 0;
  dev->active = SANE_FALSE;
  return status;
}

SANE_Status
gt68xx_device_memory_write (GT68xx_Device * dev,
			    SANE_Word addr, SANE_Word size, SANE_Byte * data)
{
  SANE_Status status;
  DBG (8,
       "gt68xx_device_memory_write: dev=%p, addr=0x%x, size=0x%x, data=%p\n",
       (void *) dev, addr, size, data);
  CHECK_DEV_ACTIVE (dev, "gt68xx_device_memory_write");
  status =
    sanei_usb_control_msg (dev->fd, 0x40,
			   dev->model->command_set->request,
			   dev->model->command_set->memory_write_value,
			   addr, size, data);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (3,
	   "gt68xx_device_memory_write: sanei_usb_control_msg failed: %s\n",
	   sane_strstatus (status));
    }
  return status;
}

SANE_Status
gt68xx_device_memory_read (GT68xx_Device * dev,
			   SANE_Word addr, SANE_Word size, SANE_Byte * data)
{
  SANE_Status status;
  DBG (8,
       "gt68xx_device_memory_read: dev=%p, addr=0x%x, size=0x%x, data=%p\n",
       (void *) dev, addr, size, data);
  CHECK_DEV_ACTIVE (dev, "gt68xx_device_memory_read");
  status =
    sanei_usb_control_msg (dev->fd, 0xc0,
			   dev->model->command_set->request,
			   dev->model->command_set->memory_read_value,
			   addr, size, data);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (3,
	   "gt68xx_device_memory_read: sanei_usb_control_msg failed: %s\n",
	   sane_strstatus (status));
    }

  return status;
}

static SANE_Status
gt68xx_device_generic_req (GT68xx_Device * dev,
			   SANE_Byte request_type, SANE_Word request,
			   SANE_Word cmd_value, SANE_Word cmd_index,
			   SANE_Word res_value, SANE_Word res_index,
			   GT68xx_Packet cmd, GT68xx_Packet res,
			   size_t res_size)
{
  SANE_Status status;
  DBG (7, "gt68xx_device_generic_req: command=0x%02x\n", cmd[0]);
  DUMP_REQ (">>", cmd);
  CHECK_DEV_ACTIVE (dev, "gt68xx_device_generic_req");
  status = sanei_usb_control_msg (dev->fd,
				  request_type, request, cmd_value,
				  cmd_index, GT68XX_PACKET_SIZE, cmd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (3, "gt68xx_device_generic_req: writing command failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  memset (res, 0, sizeof (GT68xx_Packet));
  status = sanei_usb_control_msg (dev->fd,
				  request_type | 0x80, request,
				  res_value, res_index, res_size, res);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (3, "gt68xx_device_generic_req: reading response failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DUMP_REQ ("<<", res);
  return status;
}

SANE_Status
gt68xx_device_req (GT68xx_Device * dev, GT68xx_Packet cmd, GT68xx_Packet res)
{
  GT68xx_Command_Set *command_set = dev->model->command_set;
  return gt68xx_device_generic_req (dev,
				    command_set->request_type,
				    command_set->request,
				    command_set->send_cmd_value,
				    command_set->send_cmd_index,
				    command_set->recv_res_value,
				    command_set->recv_res_index, cmd,
				    res, GT68XX_PACKET_SIZE);
}

SANE_Status
gt68xx_device_small_req (GT68xx_Device * dev, GT68xx_Packet cmd,
			 GT68xx_Packet res)
{
  GT68xx_Command_Set *command_set = dev->model->command_set;
  GT68xx_Packet fixed_cmd;
  int i;
  for (i = 0; i < 8; ++i)
    memcpy (fixed_cmd + i * 8, cmd, 8);
  return gt68xx_device_generic_req (dev,
				    command_set->request_type,
				    command_set->request,
				    command_set->send_small_cmd_value,
				    command_set->send_small_cmd_index,
				    command_set->recv_small_res_value,
				    command_set->recv_small_res_index,
				    fixed_cmd, res, 0x08);
}


SANE_Status
gt68xx_device_download_firmware (GT68xx_Device * dev,
				 SANE_Byte * data, SANE_Word size)
{
  CHECK_DEV_ACTIVE (dev, "gt68xx_device_download_firmware");
  if (dev->model->command_set->download_firmware)
    return (*dev->model->command_set->download_firmware) (dev, data, size);
  else
    return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
gt68xx_device_get_power_status (GT68xx_Device * dev, SANE_Bool * power_ok)
{
  CHECK_DEV_ACTIVE (dev, "gt68xx_device_get_power_status");
  if (dev->model->command_set->get_power_status)
    return (*dev->model->command_set->get_power_status) (dev, power_ok);
  else
    return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
gt68xx_device_get_ta_status (GT68xx_Device * dev, SANE_Bool * ta_attached)
{
  CHECK_DEV_ACTIVE (dev, "gt68xx_device_get_ta_status");
  if (dev->model->command_set->get_ta_status)
    return (*dev->model->command_set->get_ta_status) (dev, ta_attached);
  else
    return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
gt68xx_device_lamp_control (GT68xx_Device * dev, SANE_Bool fb_lamp,
			    SANE_Bool ta_lamp)
{
  CHECK_DEV_ACTIVE (dev, "gt68xx_device_lamp_control");
  if (dev->model->command_set->lamp_control)
    return (*dev->model->command_set->lamp_control) (dev, fb_lamp, ta_lamp);
  else
    return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
gt68xx_device_is_moving (GT68xx_Device * dev, SANE_Bool * moving)
{
  CHECK_DEV_ACTIVE (dev, "gt68xx_device_is_moving");
  if (dev->model->command_set->is_moving)
    return (*dev->model->command_set->is_moving) (dev, moving);
  else
    return SANE_STATUS_UNSUPPORTED;
}

/* currently not used */
#if 0
static SANE_Status
gt68xx_device_move_relative (GT68xx_Device * dev, SANE_Int distance)
{
  CHECK_DEV_ACTIVE (dev, "gt68xx_device_move_relative");
  if (dev->model->command_set->move_relative)
    return (*dev->model->command_set->move_relative) (dev, distance);
  else
    return SANE_STATUS_UNSUPPORTED;
}
#endif

SANE_Status
gt68xx_device_carriage_home (GT68xx_Device * dev)
{
  CHECK_DEV_ACTIVE (dev, "gt68xx_device_carriage_home");
  if (dev->model->command_set->carriage_home)
    return (*dev->model->command_set->carriage_home) (dev);
  else
    return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
gt68xx_device_paperfeed (GT68xx_Device * dev)
{
  CHECK_DEV_ACTIVE (dev, "gt68xx_device_paperfeed");
  if (dev->model->command_set->paperfeed)
    return (*dev->model->command_set->paperfeed) (dev);
  else
    return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
gt68xx_device_start_scan (GT68xx_Device * dev)
{
  CHECK_DEV_ACTIVE (dev, "gt68xx_device_start_scan");
  if (dev->model->command_set->start_scan)
    return (*dev->model->command_set->start_scan) (dev);
  else
    return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
gt68xx_device_read_scanned_data (GT68xx_Device * dev, SANE_Bool * ready)
{
  CHECK_DEV_ACTIVE (dev, "gt68xx_device_read_scanned_data");
  if (dev->model->command_set->read_scanned_data)
    return (*dev->model->command_set->read_scanned_data) (dev, ready);
  else
    return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
gt68xx_device_setup_scan (GT68xx_Device * dev,
			  GT68xx_Scan_Request * request,
			  GT68xx_Scan_Action action,
			  GT68xx_Scan_Parameters * params)
{
  CHECK_DEV_ACTIVE (dev, "gt68xx_device_setup_scan");
  if (dev->model->command_set->setup_scan)
    return (*dev->model->command_set->setup_scan) (dev, request, action,
						   params);
  else
    return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
gt68xx_device_set_afe (GT68xx_Device * dev, GT68xx_AFE_Parameters * params)
{
  CHECK_DEV_ACTIVE (dev, "gt68xx_device_set_afe");
  if (dev->model->command_set->set_afe)
    return (*dev->model->command_set->set_afe) (dev, params);
  else
    return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
gt68xx_device_set_exposure_time (GT68xx_Device * dev,
				 GT68xx_Exposure_Parameters * params)
{
  CHECK_DEV_ACTIVE (dev, "gt68xx_device_set_exposure_time");
  if (dev->model->command_set->set_exposure_time)
    return (*dev->model->command_set->set_exposure_time) (dev, params);
  else
    return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
gt68xx_device_stop_scan (GT68xx_Device * dev)
{
  CHECK_DEV_ACTIVE (dev, "gt68xx_device_stop_scan");
  if (dev->model->command_set->stop_scan)
    return (*dev->model->command_set->stop_scan) (dev);
  else
    return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
gt68xx_device_read_raw (GT68xx_Device * dev, SANE_Byte * buffer,
			size_t * size)
{
  SANE_Status status;
  CHECK_DEV_ACTIVE (dev, "gt68xx_device_read_raw");
  DBG (7, "gt68xx_device_read_raw: enter: size=%lu\n", (unsigned long) *size);
  status = sanei_usb_read_bulk (dev->fd, buffer, size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (3, "gt68xx_device_read_raw: bulk read failed: %s\n",
	   sane_strstatus (status));
      return status;
    }
  DBG (7, "gt68xx_device_read_raw: leave: size=%lu\n", (unsigned long) *size);
  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_device_set_read_buffer_size (GT68xx_Device * dev, size_t buffer_size)
{
  CHECK_DEV_NOT_NULL (dev, "gt68xx_device_set_read_buffer_size");
  if (dev->read_active)
    {
      DBG (3, "gt68xx_device_set_read_buffer_size: BUG: read already "
	   "active\n");
      return SANE_STATUS_INVAL;
    }

  buffer_size = (buffer_size + 63UL) & ~63UL;
  if (buffer_size > 0)
    {
      dev->requested_buffer_size = buffer_size;
      return SANE_STATUS_GOOD;
    }

  DBG (3, "gt68xx_device_set_read_buffer_size: bad buffer size\n");
  return SANE_STATUS_INVAL;
}

SANE_Status
gt68xx_device_read_prepare (GT68xx_Device * dev,
			    size_t expected_count, SANE_Bool final_scan)
{
  size_t buffer_size;
  CHECK_DEV_ACTIVE (dev, "gt68xx_device_read_prepare");
  if (dev->read_active)
    {
      DBG (3, "gt68xx_device_read_prepare: read already active\n");
      return SANE_STATUS_INVAL;
    }
  DBG (5, "gt68xx_device_read_prepare: total size: %lu bytes\n",
       (unsigned long) expected_count);
  buffer_size = dev->requested_buffer_size;
  DBG (5, "gt68xx_device_read_prepare: requested buffer size: %lu\n",
       (unsigned long) buffer_size);
  if (buffer_size > expected_count)
    {
      buffer_size = (expected_count + 63UL) & ~63UL;
    }
  DBG (5, "gt68xx_device_read_prepare: real size: %lu\n",
       (unsigned long) buffer_size);
  dev->read_buffer_size = buffer_size;
  dev->read_buffer = (SANE_Byte *) malloc (buffer_size);
  if (!dev->read_buffer)
    {
      DBG (3,
	   "gt68xx_device_read_prepare: not enough memory for the read buffer (%lu bytes)\n",
	   (unsigned long) buffer_size);
      return SANE_STATUS_NO_MEM;
    }

  dev->read_active = SANE_TRUE;
  dev->final_scan = final_scan;
  dev->read_pos = dev->read_bytes_in_buffer = 0;
  dev->read_bytes_left = expected_count;
  return SANE_STATUS_GOOD;
}

#ifdef USE_FORK

static SANE_Status
gt68xx_reader_process (GT68xx_Device * dev)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Int buffer_id;
  SANE_Byte *buffer_addr;
  size_t size;
  SANE_Int line = 0;
  size_t read_bytes_left = dev->read_bytes_left;
  shm_channel_writer_init (dev->shm_channel);
  while (read_bytes_left > 0)
    {
      status = shm_channel_writer_get_buffer (dev->shm_channel,
					      &buffer_id, &buffer_addr);
      if (status != SANE_STATUS_GOOD)
	break;
      DBG (9, "gt68xx_reader_process: buffer %d: get\n", buffer_id);
      size = dev->read_buffer_size;
      DBG (9, "gt68xx_reader_process: buffer %d: trying to read %lu bytes "
	   "(%lu bytes left, line %d)\n", buffer_id, (unsigned long) size,
	   (unsigned long) read_bytes_left, line);
      status = gt68xx_device_read_raw (dev, buffer_addr, &size);
      if (status != SANE_STATUS_GOOD)
	break;
      DBG (9,
	   "gt68xx_reader_process: buffer %d: read %lu bytes (line %d)\n",
	   buffer_id, (unsigned long) size, line);
      status =
	shm_channel_writer_put_buffer (dev->shm_channel, buffer_id, size);
      if (status != SANE_STATUS_GOOD)
	break;
      DBG (9, "gt68xx_reader_process: buffer %d: put\n", buffer_id);
      read_bytes_left -= size;
      line++;
    }
  DBG (9, "gt68xx_reader_process: finished, now sleeping\n");
  if (status != SANE_STATUS_GOOD)
    return status;
  sleep (5 * 60);		/* wait until we are killed (or timeout) */
  shm_channel_writer_close (dev->shm_channel);
  return status;
}

static SANE_Status
gt68xx_device_read_start_fork (GT68xx_Device * dev)
{
  SANE_Status status;
  int pid;
  if (dev->shm_channel)
    {
      DBG (3,
	   "gt68xx_device_read_start_fork: BUG: shm_channel already created\n");
      return SANE_STATUS_INVAL;
    }

  status =
    shm_channel_new (dev->read_buffer_size, SHM_BUFFERS, &dev->shm_channel);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (3,
	   "gt68xx_device_read_start_fork: cannot create shared memory channel: "
	   "%s\n", sane_strstatus (status));
      dev->shm_channel = NULL;
      return status;
    }

  pid = fork ();
  if (pid == -1)
    {
      DBG (3, "gt68xx_device_read_start_fork: cannot fork: %s\n",
	   strerror (errno));
      shm_channel_free (dev->shm_channel);
      dev->shm_channel = NULL;
      return SANE_STATUS_NO_MEM;
    }

  if (pid == 0)
    {
      /* Child process */
      status = gt68xx_reader_process (dev);
      _exit (status);
    }
  else
    {
      /* Parent process */
      dev->reader_pid = pid;
      shm_channel_reader_init (dev->shm_channel);
      shm_channel_reader_start (dev->shm_channel);
      return SANE_STATUS_GOOD;
    }
}

#endif /* USE_FORK */


static SANE_Status
gt68xx_device_read_start (GT68xx_Device * dev)
{
  CHECK_DEV_ACTIVE (dev, "gt68xx_device_read_start");
#ifdef USE_FORK
  /* Don't fork a separate process for every calibration scan. */
  if (dev->final_scan)
    return gt68xx_device_read_start_fork (dev);
#endif /* USE_FORK */
  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_device_read (GT68xx_Device * dev, SANE_Byte * buffer, size_t * size)
{
  SANE_Status status;
  size_t byte_count = 0;
  size_t left_to_read = *size;
  size_t transfer_size, block_size, raw_block_size;
#ifdef USE_FORK
  SANE_Int buffer_id;
  SANE_Byte *buffer_addr;
  SANE_Int buffer_bytes;
#endif /* USE_FORK */
  CHECK_DEV_ACTIVE (dev, "gt68xx_device_read");
  if (!dev->read_active)
    {
      DBG (3, "gt68xx_device_read: read not active\n");
      return SANE_STATUS_INVAL;
    }

  while (left_to_read > 0)
    {
      if (dev->read_bytes_in_buffer == 0)
	{
	  block_size = dev->read_buffer_size;
	  if (block_size > dev->read_bytes_left)
	    block_size = dev->read_bytes_left;
	  if (block_size == 0)
	    break;
	  raw_block_size = (block_size + 63UL) & ~63UL;
	  DBG (7, "gt68xx_device_read: trying to read %ld bytes\n",
	       (long) raw_block_size);
#ifdef USE_FORK
	  if (dev->shm_channel)
	    {
	      status = shm_channel_reader_get_buffer (dev->shm_channel,
						      &buffer_id,
						      &buffer_addr,
						      &buffer_bytes);
	      if (status == SANE_STATUS_GOOD && buffer_addr != NULL)
		{
		  DBG (9, "gt68xx_device_read: buffer %d: get\n", buffer_id);
		  memcpy (dev->read_buffer, buffer_addr, buffer_bytes);
		  shm_channel_reader_put_buffer (dev->shm_channel, buffer_id);
		  DBG (9, "gt68xx_device_read: buffer %d: put\n", buffer_id);
		}
	    }
	  else
#endif /* USE_FORK */
	    status = gt68xx_device_read_raw (dev, dev->read_buffer,
					     &raw_block_size);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (3, "gt68xx_device_read: read failed\n");
	      return status;
	    }
	  dev->read_pos = 0;
	  dev->read_bytes_in_buffer = block_size;
	  dev->read_bytes_left -= block_size;
	}

      transfer_size = left_to_read;
      if (transfer_size > dev->read_bytes_in_buffer)
	transfer_size = dev->read_bytes_in_buffer;
      if (transfer_size > 0)
	{
	  memcpy (buffer, dev->read_buffer + dev->read_pos, transfer_size);
	  dev->read_pos += transfer_size;
	  dev->read_bytes_in_buffer -= transfer_size;
	  byte_count += transfer_size;
	  left_to_read -= transfer_size;
	  buffer += transfer_size;
	}
    }

  *size = byte_count;
  if (byte_count == 0)
    return SANE_STATUS_EOF;
  else
    return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_device_read_finish (GT68xx_Device * dev)
{
  SANE_Status status = SANE_STATUS_GOOD;
  CHECK_DEV_ACTIVE (dev, "gt68xx_device_read_finish");
  if (!dev->read_active)
    {
      DBG (3, "gt68xx_device_read_finish: read not active\n");
      return SANE_STATUS_INVAL;
    }

  DBG (7, "gt68xx_device_read_finish: read_bytes_left = %ld\n",
       (long) dev->read_bytes_left);
#ifdef USE_FORK
  if (dev->reader_pid != 0)
    {
      int pid_status;
      /* usleep (100000); */
      DBG (7, "gt68xx_device_read_finish: trying to kill reader process\n");
      kill (dev->reader_pid, SIGKILL);
      waitpid (dev->reader_pid, &pid_status, 0);
      if (WIFEXITED (pid_status))
	status = WEXITSTATUS (pid_status);
      DBG (7, "gt68xx_device_read_finish: reader process killed\n");
      dev->reader_pid = 0;
    }
  if (dev->shm_channel)
    {
      shm_channel_free (dev->shm_channel);
      dev->shm_channel = NULL;
    }

#endif /* USE_FORK */

  free (dev->read_buffer);
  dev->read_buffer = NULL;
  dev->read_active = SANE_FALSE;
  DBG (7, "gt68xx_device_read_finish: exit (%s)\n", sane_strstatus (status));
  return status;
}

static SANE_Status
gt68xx_device_check_result (GT68xx_Packet res, SANE_Byte command)
{
  if (res[0] != 0)
    {
      DBG (1, "gt68xx_device_check_result: result was %2X %2X "
	   "(expected: %2X %2X)\n", res[0], res[1], 0, command);
      return SANE_STATUS_IO_ERROR;
    }
  /* The Gt681xfw.usb firmware doesn't return the command byte
     in the second byte, so we can't rely on that test */
  if (res[1] != command)
    DBG (5, "gt68xx_device_check_result: warning: result was %2X %2X "
	 "(expected: %2X %2X)\n", res[0], res[1], 0, command);
  return SANE_STATUS_GOOD;
}

SANE_Status
gt68xx_device_get_id (GT68xx_Device * dev)
{
  CHECK_DEV_ACTIVE (dev, "gt68xx_device_get_id");
  if (dev->model->command_set->get_id)
    return (*dev->model->command_set->get_id) (dev);
  else
    return SANE_STATUS_UNSUPPORTED;
}

static void 
gt68xx_device_fix_descriptor (GT68xx_Device * dev)
{
  SANE_Byte data[8];
  sanei_usb_control_msg (dev->fd, 0x80, 0x06, 0x01 << 8, 0, 8, data);
}


/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
