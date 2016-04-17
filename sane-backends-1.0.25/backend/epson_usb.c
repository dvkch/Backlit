#include <sys/types.h>
#include "../include/sane/sanei_usb.h"
#include "epson_usb.h"

/* generated with epson2usb.pl doc/descriptions/epson.desc */

SANE_Word sanei_epson_usb_product_ids[] = {
  0x101, /* Perfection 636U */
  0x103, /* Perfection 610 */
  0x104, /* Perfection 1200U, Perfection 1200Photo */
  0x107, /* Expression 1600 */
  0x10a, /* Perfection 1640 */
  0x10b, /* Perfection 1240 */
  0x10c, /* Perfection 640 */
  0x10e, /* Expression 1680 */
  0x110, /* Perfection 1650 */
  0x112, /* Perfection 2450 */
  0x11b, /* Perfection 2400 */
  0x11c, /* Perfection 3200 */
  0x11e, /* Perfection 1660 */
  0x128, /* Perfection 4870 */
  0x12a, /* Perfection 4990 */
  0x12c, /* V700, V750 */
  0x801, /* CX-5200, CX-5400 */
  0x802, /* CX-3200 */
  0x805, /* CX-6300, CX-6400 */
  0x806, /* RX-600 */
  0x807, /* RX-500 */
  0x808, /* CX-5400 */
  0x80d, /* CX-4600 */
  0x80e, /* CX-3600, CX-3650 */
  0x80f, /* RX-425 */
  0x810, /* RX-700 */
  0x811, /* RX-620 */
  0x813, /* CX-6500, CX-6600 */
  0x815, /* AcuLaser CX11, AcuLaser CX11NF */
  0x818, /* DX-3850, CX-3700, CX-3800, DX-3800 */
  0x819, /* CX-4800 */
  0x820, /* CX-4200 */
  0x82b, /* CX-5000, DX-5000, DX-5050 */
  0x82e, /* DX-6000 */
  0x82f, /* DX-4050 */
  0x838, /* DX-7400 */
  0				/* last entry - this is used for devices that are specified 
				   in the config file as "usb <vendor> <product>" */
};

int
sanei_epson_getNumberOfUSBProductIds (void)
{
  return sizeof (sanei_epson_usb_product_ids) / sizeof (SANE_Word);
}
