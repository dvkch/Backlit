#include <sys/types.h>
#include "../include/sane/sanei_usb.h"
#include "epson_usb.h"

/* generated with epson2usb.pl doc/descriptions/epson2.desc */

SANE_Word sanei_epson_usb_product_ids[] = {
  0x101, /* GT-7000U, Perfection 636U */
  0x103, /* GT-6600U, Perfection 610 */
  0x104, /* GT-7600U, GT-7600UF, Perfection 1200U, Perfection 1200U PHOTO */
  0x105, /* Stylus Scan 2000 */
  0x106, /* Stylus Scan 2500 */
  0x107, /* ES-2000, Expression 1600 */
  0x109, /* ES-8500, Expression 1640XL */
  0x10a, /* GT-8700, GT-8700F, Perfection 1640SU, Perfection 1640SU PHOTO */
  0x10b, /* GT-7700U, Perfection 1240U */
  0x10c, /* GT-6700U, Perfection 640U */
  0x10e, /* ES-2200, Expression 1680 */
  0x110, /* GT-8200U, GT-8200UF, Perfection 1650, Perfection 1650 PHOTO */
  0x112, /* GT-9700F, Perfection 2450 PHOTO */
  0x11b, /* GT-9300UF, Perfection 2400 PHOTO */
  0x11c, /* GT-9800F, Perfection 3200 PHOTO */
  0x11e, /* GT-8300UF, Perfection 1660 PHOTO */
  0x126, /* ES-7000H, GT-15000 */
  0x128, /* GT-X700, Perfection 4870 PHOTO */
  0x129, /* ES-10000G, Expression 10000XL */
  0x12a, /* GT-X800, Perfection 4990 PHOTO */
  0x12b, /* ES-H300, GT-2500 */
  0x12c, /* GT-X900, Perfection V700 Photo, Perfection V750 Photo */
  0x135, /* GT-X970 */
  0x138, /* ES-H7200, GT-20000 */
  0x14b, /* ES-G11000, Expression 11000XL */
  0x151, /* GT-X980, Perfection V800 Photo, Perfection V850 Pro */
  0x801, /* CC-600PX, Stylus CX5100, Stylus CX5200 */
  0x802, /* CC-570L, Stylus CX3100, Stylus CX3200 */
  0x805, /* Stylus CX6300, Stylus CX6400 */
  0x806, /* PM-A850, Stylus Photo RX600 */
  0x807, /* Stylus Photo RX500, Stylus Photo RX510 */
  0x808, /* Stylus CX5300, Stylus CX5400 */
  0x80d, /* Stylus CX4500, Stylus CX4600 */
  0x80e, /* PX-A550, Stylus CX3500, Stylus CX3600, Stylus CX3650 */
  0x80f, /* Stylus Photo RX420, Stylus Photo RX425, Stylus Photo RX430 */
  0x810, /* PM-A900, Stylus Photo RX700 */
  0x811, /* PM-A870, Stylus Photo RX620, Stylus Photo RX630 */
  0x813, /* Stylus CX6500, Stylus CX6600 */
  0x814, /* PM-A700 */
  0x815, /* AcuLaser CX11, AcuLaser CX11NF, LP-A500 */
  0x817, /* LP-M5500, LP-M5500F */
  0x818, /* Stylus CX3700, Stylus CX3800, Stylus CX3810, Stylus DX3800 */
  0x819, /* PX-A650, Stylus CX4700, Stylus CX4800, Stylus DX4800, Stylus DX4850 */
  0x81a, /* PM-A750, Stylus Photo RX520, Stylus Photo RX530 */
  0x81c, /* PM-A890, Stylus Photo RX640, Stylus Photo RX650 */
  0x81d, /* PM-A950 */
  0x81f, /* Stylus CX7700, Stylus CX7800 */
  0x820, /* Stylus CX4100, Stylus CX4200, Stylus DX4200 */
  0x827, /* PM-A820, Stylus Photo RX560, Stylus Photo RX580, Stylus Photo RX590 */
  0x828, /* PM-A970 */
  0x829, /* PM-T990 */
  0x82a, /* PM-A920 */
  0x82b, /* Stylus CX4900, Stylus CX5000, Stylus DX5000 */
  0x82e, /* PX-A720, Stylus CX5900, Stylus CX6000, Stylus DX6000 */
  0x82f, /* PX-A620, Stylus CX3900, Stylus DX4000 */
  0x830, /* ME 200, Stylus CX2800, Stylus CX2900 */
  0x833, /* LP-M5600 */
  0x834, /* LP-M6000 */
  0x835, /* AcuLaser CX21 */
  0x836, /* PM-T960 */
  0x837, /* PM-A940, Stylus Photo RX680, Stylus Photo RX685, Stylus Photo RX690 */
  0x838, /* PX-A640, Stylus CX7300, Stylus CX7400, Stylus DX7400 */
  0x839, /* PX-A740, Stylus CX8300, Stylus CX8400, Stylus DX8400 */
  0x83a, /* PX-FA700, Stylus CX9300F, Stylus CX9400Fax, Stylus DX9400F */
  0x83c, /* PM-A840, PM-A840S, Stylus Photo RX585, Stylus Photo RX595, Stylus Photo RX610 */
  0x841, /* ME 300, PX-401A, Stylus NX100, Stylus SX100, Stylus TX100 */
  0x843, /* LP-M5000 */
  0x844, /* Artisan 800, EP-901A, EP-901F, Stylus Photo PX800FW, Stylus Photo TX800FW */
  0x846, /* Artisan 700, EP-801A, Stylus Photo PX700W, Stylus Photo TX700W */
  0x847, /* ME Office 700FW, PX-601F, Stylus Office BX600FW, Stylus Office TX600FW, Stylus SX600FW, WorkForce 600 */
  0x848, /* ME Office 600F, Stylus Office BX300F, Stylus Office TX300F, Stylus NX300 Series */
  0x849, /* Stylus NX200, Stylus SX200, Stylus SX205, Stylus TX200, Stylus TX203, Stylus TX209 */
  0x84a, /* PX-501A, Stylus NX400, Stylus SX400, Stylus SX405, Stylus TX400 */
  0x84c, /* WorkForce 500 */
  0x84d, /* PX-402A, Stylus NX110 Series, Stylus SX110 Series, Stylus TX110 Series */
  0x84f, /* ME OFFICE 510, Stylus NX210 Series, Stylus SX210 Series, Stylus TX210 Series */
  0x850, /* EP-702A, Stylus Photo PX650 Series, Stylus Photo TX650 Series */
  0x851, /* Stylus NX410 Series, Stylus SX410 Series, Stylus TX410 Series */
  0x852, /* Artisan 710 Series, EP-802A, Stylus Photo PX710W Series, Stylus Photo TX710W Series */
  0x853, /* Artisan 810 Series, EP-902A, Stylus Photo PX810FW Series */
  0x854, /* ME OFFICE 650FN Series, Stylus Office BX310FN Series, Stylus Office TX510FN Series, WorkForce 310 Series */
  0x855, /* PX-602F, Stylus Office BX610FW Series, Stylus Office TX610FW Series, Stylus SX610FW Series, WorkForce 610 Series */
  0x856, /* PX-502A, Stylus NX510 Series, Stylus SX510W Series, Stylus TX550W Series */
  0x85c, /* ME 320 Series, ME 330 Series, Stylus NX125, Stylus NX127, Stylus SX125, Stylus TX120 Series */
  0x85d, /* ME OFFICE 960FWD Series, PX-603F, Stylus Office BX625FWD, Stylus Office TX620FWD Series, Stylus SX620FW Series, WorkForce 630 Series */
  0x85e, /* ME OFFICE 900WD Series, PX-503A, Stylus Office BX525WD, Stylus NX625, Stylus SX525WD, Stylus TX560WD Series, WorkForce 625 */
  0x85f, /* Stylus Office BX320FW Series, Stylus Office TX525FW, WorkForce 520 Series */
  0x860, /* Artisan 835, EP-903A, EP-903F, Stylus Photo PX820FWD Series, Stylus Photo TX820FWD Series */
  0x861, /* Artisan 725, EP-803A, EP-803AW, Stylus Photo PX720WD Series, Stylus Photo TX720WD Series */
  0x862, /* EP-703A, Stylus Photo PX660 Series */
  0x863, /* ME OFFICE 620F Series, Stylus Office BX305F, Stylus Office BX305FW, Stylus Office TX320F Series, WorkForce 320 Series */
  0x864, /* ME OFFICE 560W Series, Stylus NX420 Series, Stylus SX420W Series, Stylus TX420W Series */
  0x865, /* ME OFFICE 520 Series, Stylus NX220 Series, Stylus SX218, Stylus TX220 Series */
  0x866, /* AcuLaser MX20DN, AcuLaser MX20DNF, AcuLaser MX21DNF */
  0x869, /* PX-1600F, WF-7510 Series */
  0x86a, /* PX-673F, Stylus Office BX925FWD, WorkForce 840 Series */
  0x870, /* Stylus Office BX305FW Plus, WorkForce 435 */
  0x871, /* K200 Series */
  0x872, /* K300 Series, WorkForce K301 */
  0x873, /* L200 Series */
  0x878, /* Artisan 635, EP-704A */
  0x879, /* Artisan 837, EP-904A, EP-904F, Stylus Photo PX830FWD Series */
  0x87b, /* Artisan 730 Series, EP-804A, EP-804AR, EP-804AW, Stylus Photo PX730WD Series, Stylus Photo TX730WD Series */
  0x87c, /* PX-1700F, WF-7520 Series */
  0x87d, /* PX-B750F, WP-4511, WP-4515, WP-4521, WP-4525, WP-4530 Series, WP-4540 Series */
  0x87e, /* WP-4590 Series */
  0x87f, /* PX-403A */
  0x880, /* ME OFFICE 570W Series, PX-434A, Stylus NX330 Series, Stylus SX430W Series, Stylus TX430W Series */
  0x881, /* ME OFFICE 535, PX-404A, Stylus SX230 Series, Stylus TX235 */
  0x883, /* ME 340 Series, Stylus NX130 Series, Stylus SX130 Series, Stylus TX130 Series */
  0x884, /* Stylus NX430W Series, Stylus SX440W Series, Stylus TX435W */
  0x885, /* Stylus NX230 Series, Stylus SX235W, Stylus TX230W Series */
  0x88d, /* Epson ME 350 */
  0x88f, /* Stylus Office BX635FWD, WorkForce 645 */
  0x890, /* ME OFFICE 940FW Series, Stylus Office BX630FW Series, WorkForce 545 */
  0x891, /* PX-504A, Stylus Office BX535WD, Stylus NX530 Series, Stylus NX635, Stylus SX535WD */
  0x892, /* Stylus Office BX935FWD, WorkForce 845 */
  0x893, /* EP-774A */
  0x894, /* LP-M5300 Series */
  0x895, /* PX-045A, XP-100 Series */
  0x896, /* ME-301, XP-200 Series */
  0x897, /* ME-303, PX-405A */
  0x898, /* ME-401, PX-435A, XP-300 Series, XP-400 Series */
  0x899, /* PX-605F, PX-675F, WF-3520 Series, WF-3530 Series, WF-3540 Series */
  0x89a, /* EP-905F, XP-850 Series */
  0x89b, /* EP-905A, XP-800 Series */
  0x89c, /* EP-805A, EP-805AR, EP-805AW, XP-750 Series */
  0x89d, /* XP-700 Series */
  0x89e, /* EP-775A, EP-775AW, XP-600 Series */
  0x89f, /* EP-705A */
  0x8a0, /* ME-101 */
  0x8a1, /* L210 Series, L350, L351 */
  0x8a5, /* PX-505F, WF-2510 Series */
  0x8a6, /* PX-535F, WF-2520 Series, WF-2530 Series, WF-2540 Series */
  0x8a7, /* WP-M4525, WP-M4521, PX-K751F, WP-M4595 */
  0x8a8, /* L355, L358 */
  0x8a9, /* L550 Series */
  0x8aa, /* M200 Series */
  0x8ab, /* WF-M1560 Series */
  0x8ac, /* AL-MX300DN Series, AL-MX300DNF Series */
  0x8ad, /* LP-M8040, LP-M8040A, LP-M8040F */
  0x8ae, /* PX-046A, XP-211, XP-212, XP-215 */
  0x8af, /* PX-436A, XP-310 Series */
  0x8b0, /* XP-410 Series */
  0x8b3, /* EP-976A3, XP-950 Series */
  0x8b4, /* EP-906F, XP-810 Series */
  0x8b5, /* EP-806AB, EP-806AR, EP-806AW, XP-710 Series */
  0x8b6, /* EP-776AB, EP-776AW, XP-610 Series */
  0x8b7, /* EP-706A, XP-510 Series */
  0x8b8, /* PX-M740F, PX-M741F, WF-3620 Series, WF-3640 Series */
  0x8b9, /* PX-M5040F, PX-M5041F, WF-7610 Series, WF-7620 Series */
  0x8bd, /* PX-M840F, WF-5620 Series, WF-5690 Series */
  0x8be, /* WF-4630 Series, WF-4640 Series */
  0x8bf, /* PX-437A, XP-320 Series */
  0x8c0, /* PX-047A, XP-225 */
  0x8c1, /* XP-420 Series */
  0x8c3, /* PX-M650A, PX-M650F, WF-2650 Series, WF-2660 Series */
  0x8c4, /* WF-2630 Series */
  0x8c5, /* EP-977A3 */
  0x8c6, /* EP-907F, XP-820 Series, XP-860 Series */
  0x8c7, /* EP-807AB, EP-807AR, EP-807AW, XP-720 Series, XP-760 Series */
  0x8c8, /* EP-777A, XP-520 Series, XP-620 Series */
  0x8c9, /* EP-707A */
  0x8ca, /* L850 Series */
  0x8cd, /* WF-R4640 Series, WF-R5690 Series */
  0x8d0, /* PX-M350F, WF-M5690 Series */
  0				/* last entry - this is used for devices that are specified 
				   in the config file as "usb <vendor> <product>" */
};

int
sanei_epson_getNumberOfUSBProductIds (void)
{
  return sizeof (sanei_epson_usb_product_ids) / sizeof (SANE_Word);
}
