/* -------------------------------------------------------------------- */

/* umax-scanner.c: scanner-definiton file for UMAX scanner driver.
  
   (C) 1997-2004 Oliver Rauch

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

/* -------------------------------------------------------------------- */

#include "umax-scanner.h"

/* ==================================================================== */

/* scanners that are supported because the driver knows missing */
/* inquiry-data */

/* these umax-*.c files are included and not compiled separately */
/* because this way the symbols are not exported */

#include "umax-uc630.c"
#include "umax-uc840.c"
#include "umax-ug630.c"
#include "umax-ug80.c"
#include "umax-uc1200s.c"
#include "umax-uc1200se.c"
#include "umax-uc1260.c"

static inquiry_blk *inquiry_table[] =
{
  &inquiry_uc630,
  &inquiry_uc840,
  &inquiry_ug630,
  &inquiry_ug80,
  &inquiry_uc1200s,
  &inquiry_uc1200se,
  &inquiry_uc1260
};

#define known_inquiry 7

/* ==================================================================== */

/* names of scanners that are supported because */
/* the inquiry_return_block is ok and driver is tested */

static char *scanner_str[] =
{
  "UMAX ",	"Vista-T630 ",
  "UMAX ",	"Vista-S6 ",
  "UMAX ",	"Vista-S6E ",
  "UMAX ",	"UMAX S-6E ",
  "UMAX ",	"UMAX S-6EG ",
  "UMAX ",	"Vista-S8 ",
  "UMAX ",	"UMAX S-12 ",
  "UMAX ",	"UMAX S-12G ",
  "UMAX ",	"SuperVista S-12 ",
  "UMAX ",	"PSD ",
  "UMAX ",	"Astra 600S ",
  "UMAX ",	"Astra 610S ",
  "UMAX ",	"Astra 1200S ",
  "UMAX ",	"Astra 1220S ", 
  "UMAX ",	"Astra 2100S ", 
  "UMAX ",	"Astra 2200 ", 
  "UMAX ",	"Astra 2400S ",
/*  "UMAX ",	"Astra 6400 ", */ /* this is a firewire scanner */
/*  "UMAX ",	"Astra 6450 ", */ /* this is a firewire scanner */
  "UMAX ",	"Mirage D-16L ",
  "UMAX ",	"Mirage II ",
  "UMAX ",	"Mirage IIse ",
  "UMAX ",	"PL-II ",
  "UMAX ",	"Power Look 2000 ",
  "UMAX ",	"PowerLook 2100XL",
  "UMAX ",	"PowerLook III ",
  "UMAX ",	"PowerLook 3000 ",
  "UMAX ",	"Gemini D-16 ",
  "UMAX ",      "PS-2400X ", /* same as LinoHell SAPHIR */
  "LinoHell",	"JADE ",   /* is a Supervista S-12 */
  "LinoHell",	"Office ", /* is a Supervista S-12 */
  "LinoHell",	"Office2 ",
  "LinoHell",	"SAPHIR ", /* same as UMAX PS-2400X */
  "LinoHell",	"SAPHIR2 ",
  "LinoHell",	"SAPHIR3 ", /* 1000x2000 dpi */
/*  "LinoHell",	"SAPHIR4 ", */
  "Linotype",	"SAPHIR4 ", /* Linotype-Hell Saphir Ultra II */
/*  "LinoHell",	"OPAL ", */
  "LinoHell",	"OPAL2 ", /* looks like a UMAX Mirage II */
  "HDM ",	"LS4H1S ", /* Linoscan 1400 */
  "Nikon ",	"AX-110 ", /* is a Vista S6E */
  "Nikon ",	"AX-210 ", /* is a Supervista S12 */
  "KYE ",	"ColorPage-HR5 ", 
  "EPSON ",	"Perfection600 ", 
  "ESCORT ",    "Galleria 600S ", /* is an Astra 600S */
  "EDGE ",	"KTX-9600US ", /* may be an Astra 1220S */
  "TriGem ",	"PowerScanII ", /* is a Supervista S12 */
  "END_OF_LIST"
};

/* ==================================================================== */

