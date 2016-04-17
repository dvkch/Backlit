/*
   Copyright (C) 2008, Panasonic Russia Ltd.
*/
/* sane - Scanner Access Now Easy.
   Panasonic KV-S1020C / KV-S1025C USB scanners.
*/

#ifndef __KVS1025_USB_H
#define __KVS1025_USB_H

#include "kvs1025_cmds.h"

SANE_Status kv_usb_enum_devices (void);
SANE_Status kv_usb_open (PKV_DEV dev);
SANE_Bool kv_usb_already_open (PKV_DEV dev);
void kv_usb_close (PKV_DEV dev);
void kv_usb_cleanup (void);

SANE_Status
kv_usb_escape (PKV_DEV dev,
	       PKV_CMD_HEADER header,
	       unsigned char *status_byte);

SANE_Status kv_usb_send_command (PKV_DEV dev,
				 PKV_CMD_HEADER header,
				 PKV_CMD_RESPONSE response);

#endif /* #ifndef __KVS1025_USB_H */
