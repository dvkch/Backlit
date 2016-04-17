#ifndef _EPSON_USB_H_
#define _EPSON_USB_H_

#define SANE_EPSON_VENDOR_ID	(0x4b8)

extern SANE_Word sanei_epson_usb_product_ids[];

extern int sanei_epson_getNumberOfUSBProductIds (void);

#endif
