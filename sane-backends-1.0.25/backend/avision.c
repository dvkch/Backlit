/*******************************************************************************
 * SANE - Scanner Access Now Easy.

   avision.c

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

   *****************************************************************************

   This file implements a SANE backend for the Avision SCSI Scanners (like the
   AV 630 / 620 (CS) ...) and some Avision (OEM) USB scanners (like the HP 53xx,
   74xx, Minolta FS-V1 ...) or Fujitsu ScanPartner with the AVISION SCSI-2/3
   or USB command set and written by René Rebe and Meino Cramer.

   
   Copyright 2002 - 2015 by
               	"Ren Rebe" <rene@exactcode.de>

   Copyright 1999, 2000, 2001 by
                "René Rebe" <rene@exactcode.de>
                "Meino Christian Cramer" <mccramer@s.netic.de>

   Copyright 2002 by
                "Jose Paulo Moitinho de Almeida" <moitinho@civil.ist.utl.pt>
   
   Copyright 2010, 2011 by
                "Mike Kelly" <mike@piratehaven.org>

   Additional Contributers:
                "Gunter Wagner"
                  (some fixes and the transparency option)
                "Martin Jelínek" <mates@sirrah.troja.mff.cuni.cz>
                   nice attach debug output
                "Marcin Siennicki" <m.siennicki@cloos.pl>
                   found some typos and contributed fixes for the HP 7400
                "Frank Zago" <fzago@greshamstorage.com>
                   Mitsubishi IDs and report
                Avision INC
                   example code to handle calibration and C5 ASIC specifics
                "Franz Bakan" <fbakan@gmx.net>
                   OS/2 threading support
                "Falk Rohsiepe"
                   Spelling and whitespace as well as HP5370 quirks
   
   Many additional special thanks to:
                Avision INC for providing protocol documentation.
                Avision INC for sponsoring an AV 8000S with ADF.
                Avision Europe and BHS Binkert for sponsoring several more scanners.
                Archivista GmbH, Switzerland, for sponsoring several features
                Roberto Di Cosmo who sponsored a HP 5370 scanner.
                Oliver Neukum who sponsored a HP 5300 USB scanner.
                Matthias Wiedemann for lending his HP 7450C for some weeks.
                Compusoft, C.A. Caracas / Venezuela for sponsoring a
                           HP 7450 scanner and so enhanced ADF support.
                Chris Komatsu for the nice ADF scanning observation.

                All the many other beta-tester and debug-log sender!

                Thanks to all the people and companies above. Without you
                the Avision backend would not be in the shape it is today! ;-)
   
********************************************************************************/

/* SANE-FLOW-DIAGRAMM (from umax.c)
 *
 * - sane_init() : initialize backend, attach scanners(devicename,0)
 * . - sane_get_devices() : query list of scanner-devices
 * . - sane_open() : open a particular scanner-device and attach_scanner(devicename,&dev)
 * . . - sane_set_io_mode : set blocking-mode
 * . . - sane_get_select_fd : get scanner-fd
 * . . - sane_get_option_descriptor() : get option information
 * . . - sane_control_option() : change option values
 * . .
 * . . - sane_start() : start image acquisition
 * . .   - sane_get_parameters() : returns actual scan-parameters
 * . .   - sane_read() : read image-data (from pipe)
 *
 * in ADF mode this is done often:
 * . . - sane_start() : start image acquisition
 * . .   - sane_get_parameters() : returns actual scan-parameters
 * . .   - sane_read() : read image-data (from pipe)
 *
 * . . - sane_cancel() : cancel operation, kill reader_process
 *
 * . - sane_close() : close opened scanner-device, do_cancel, free buffer and handle
 * - sane_exit() : terminate use of backend, free devicename and device-structure
 */

#include "../include/sane/config.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>

#include <math.h>

#define BACKEND_NAME avision
#define BACKEND_BUILD 297 /* avision backend BUILD version */

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_thread.h"
#include "../include/sane/sanei_scsi.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_backend.h"

#include <avision.h>

/* For timeval... */
#ifdef DEBUG
#include <sys/time.h>
#endif

/* Attention: The comments must stay as they are - they are automatically parsed
   to generate the SANE avision.desc file, as well as HTML online content! */

/* Attention2: This device table is part of the source code and as such
   licensed under the terms of the license as listed above (GPL2+). By
   using this data you obviously create derived work! -ReneR */

static Avision_HWEntry Avision_Device_List [] =
  {
    { "AVISION", "AV100CS",
      0, 0,
      "Avision", "AV100CS",
      0},
    /* status="untested" */
    
    { "AVISION", "AV100IIICS",
      0, 0,
      "Avision", "AV100IIICS",
      0},
    /* status="untested" */
    
    { "AVISION", "AV100S",
      0, 0,
      "Avision", "AV100S",
      0},
    /* status="untested" */

    { NULL, NULL,
      0x0638, 0x0A27,
      "Avision", "AV120",
      AV_INT_STATUS},
    /* comment="sheetfed scanner" */
    /* status="complete" */

    { NULL, NULL,
      0x0638, 0x0A3C,
      "Avision", "AV121",
      AV_INT_BUTTON | AV_DOES_KEEP_WINDOW | AV_DOES_KEEP_GAMMA},
    /* comment="sheetfed scanner" */
    /* status="good" */

    { NULL, NULL,
      0x0638, 0x0A33,
      "Avision", "AV122",
      AV_INT_BUTTON | AV_2ND_LINE_INTERLACED | AV_NO_REAR | AV_SOFT_SCALE | AV_DOES_KEEP_WINDOW | AV_DOES_KEEP_GAMMA | AV_REAR_OFFSET},
    /* comment="sheetfed duplex scanner" */
    /* status="good" */
    
    { NULL, NULL,
      0x0638, 0x0A93,
      "Avision", "AV122 C2",
      AV_INT_BUTTON | AV_2ND_LINE_INTERLACED | AV_NO_REAR | AV_SOFT_SCALE | AV_DOES_NOT_KEEP_WINDOW | AV_DOES_KEEP_GAMMA | AV_REAR_OFFSET},
    /* comment="sheetfed duplex scanner" */
    /* status="good" */

    { NULL, NULL,
      0x0638, 0x0A24,
      "Avision", "AV210",
      AV_INT_BUTTON  | AV_ACCEL_TABLE},
    /* comment="sheetfed scanner" */
    /* status="complete" */
    
    { NULL, NULL,
      0x0638, 0x0A25,
      "Avision", "AV210",
      AV_INT_BUTTON  | AV_ACCEL_TABLE | AV_NO_64BYTE_ALIGN},
    /* comment="sheetfed scanner" */
    /* status="complete" */

    { NULL, NULL,
      0x0638, 0x0A3A,
      "Avision", "AV210C2",
      AV_INT_BUTTON | AV_GRAY_MODES},
    /* comment="sheetfed scanner" */
    /* status="complete" */

    { NULL, NULL,
      0x0638, 0x0A2F,
      "Avision", "AV210C2-G",
      AV_INT_BUTTON | AV_GRAY_MODES},
    /* comment="sheetfed scanner" */
    /* status="complete" */

    { NULL, NULL,
      0x0638, 0x1A35,
      "Avision", "AV210D2+",
      AV_INT_BUTTON | AV_USE_GRAY_FILTER},
    /* comment="sheetfed scanner" */
    /* status="complete" */

    { NULL, NULL,
      0x0638, 0x0A23,
      "Avision", "AV220",
      AV_INT_BUTTON | AV_GRAY_MODES},
    /* comment="duplex! sheetfed scanner" */
    /* status="complete" */

    { NULL, NULL,
      0x0638, 0x0A2A,
      "Avision", "AV220C2",
      AV_INT_BUTTON | AV_CANCEL_BUTTON},
    /* comment="duplex! sheetfed scanner" */
    /* status="complete" */

    { NULL, NULL,
      0x0638, 0x0A2B,
      "Avision", "AV220D2",
      AV_INT_BUTTON | AV_CANCEL_BUTTON},
    /* comment="duplex! sheetfed scanner" */
    /* status="complete" */

    { NULL, NULL,
      0x0638, 0x1A31,
      "Avision", "AV220D2+",
      AV_INT_BUTTON | AV_CANCEL_BUTTON | AV_USE_GRAY_FILTER},
    /* comment="duplex! sheetfed scanner" */
    /* status="complete" */

    { NULL, NULL,
      0x0638, 0x0A2C,
      "Avision", "AV220+",
      AV_INT_BUTTON | AV_CANCEL_BUTTON},
    /* comment="duplex! sheetfed scanner" */
    /* status="complete" */

    { NULL, NULL,
      0x0638, 0x0A2D,
      "Avision", "AV220C2-G",
      AV_INT_BUTTON | AV_CANCEL_BUTTON},
    /* comment="duplex! sheetfed scanner" */
    /* status="complete" */

    { NULL, NULL,
      0x0638, 0x0A2E,
      "Avision", "AV220C2-B",
      AV_INT_BUTTON | AV_CANCEL_BUTTON},
    /* comment="duplex! sheetfed scanner" */
    /* status="complete" */

    { NULL, NULL,
      0x0638, 0x0A94,
      "Avision", "AV220-G",
      AV_INT_BUTTON | AV_2ND_LINE_INTERLACED | AV_FIRMWARE},
    /* comment="duplex! sheetfed scanner" */
    /* status="complete" */

    { "AVISION", "AV240SC",
      0, 0,
      "Avision", "AV240SC",
      0},
    /* status="untested" */
    
    { "AVISION", "AV260CS",
      0, 0,
      "Avision", "AV260CS",
      0},
    /* status="untested" */
    
    { "AVISION", "AV360CS",
      0, 0,
      "Avision", "AV360CS",
      0},
    /* status="untested" */
    
    { "AVISION", "AV363CS",
      0, 0,
      "Avision", "AV363CS",
      0},
    /* status="untested" */
    
    { "AVISION", "AV420CS",
      0, 0,
      "Avision", "AV420CS",
      0},
    /* status="untested" */
    
    { "AVISION", "AV6120",
      0, 0,
      "Avision", "AV6120",
      0},
    /* status="untested" */
    
    { NULL, "AV610",
      0x0638, 0x0a18,
      "Avision", "AV610",
      AV_GRAY_CALIB_BLUE | AV_ACCEL_TABLE | AV_NO_64BYTE_ALIGN | AV_INT_BUTTON},
    /* status="good" */

    { NULL, NULL,
      0x0638, 0x0a18,
      "Avision", "AV600U Plus",
      /* If this unit requires the AV_INT_STATUS flag, then we'll need to alter the code to deal with two different devices with the same USB id (AV610 above) */
      AV_GRAY_CALIB_BLUE | AV_ACCEL_TABLE | AV_NO_64BYTE_ALIGN | /* AV_INT_STATUS | */ AV_INT_BUTTON},
    /* status="good" */

    { NULL, NULL,
      0x0638, 0x0a5e,
      "Avision", "AV610C2",
      AV_NO_BACKGROUND | AV_INT_BUTTON}, /* cancel button -> sense abort! */
    /* status="good" */

   { NULL, NULL, 
     0x0638, 0x0a41,
     "Avision", "AM3000 Series",
      0},
    /* comment="MFD" */
    /* status="basic" */

    { NULL, NULL,
      0x0638, 0x0a16,
      "Avision", "DS610CU Scancopier",
      AV_INT_STATUS},
    /* comment="1 pass, 600 dpi, A4" */
    /* status="good" */

    { "AVISION", "AV620CS",
      0, 0,
      "Avision", "AV620CS",
      0},
    /* comment="1 pass, 600 dpi" */
    /* status="complete" */
    
    { "AVISION", "AV620CS Plus",
      0, 0,
      "Avision", "AV620CS Plus",
      0},
    /* comment="1 pass, 1200 dpi" */
    /* status="complete" */
    
    { "AVISION", "AV630CS",
      0, 0,
      "Avision", "AV630CS",
      0},
    /* comment="1 pass, 1200 dpi" */
    /* status="complete" */
    
    { "AVISION", "AV630CSL",
      0, 0,
      "Avision", "AV630CSL",
      0},
    /* comment="1 pass, 1200 dpi" */
    /* status="untested" */
    
    { "AVISION", "AV6240",
      0, 0,
      "Avision", "AV6240",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="complete" */
    
    { NULL, NULL,
      0x0638, 0x0A13,
      "Avision", "AV600U",
      AV_MULTI_CALIB_CMD | AV_ADF_BGR_ORDER_INVERT | AV_SOFT_SCALE | AV_INT_STATUS | AV_NO_BUTTON},
    /* comment="1 pass, 600 dpi" */
    /* status="good" */

    { "AVISION", "AV660S",
      0, 0,
      "Avision", "AV660S",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="untested" */

    { "AVISION", "AV680S",
      0, 0,
      "Avision", "AV680S",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="untested" */
    
    { "AVISION", "AV690U",
      0, 0,
      "Avision", "AV690U",
      0},
    /* comment="1 pass, 2400 dpi" */
    /* status="untested" */
    
    { "AVISION", "AV800S",
      0, 0,
      "Avision", "AV800S",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="untested" */
    
    { "AVISION", "AV810C",
      0, 0,
      "Avision", "AV810C",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="untested" */
    
    { "AVISION", "AV820",
      0, 0,
      "Avision", "AV820",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="untested" */
    
    { "AVISION", "AV820C",
      0, 0,
      "Avision", "AV820C",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="complete" */
    
    { "AVISION", "AV820C Plus",
      0, 0,
      "Avision", "AV820C Plus",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="complete" */
    
    { "AVISION", "AV830C",
      0, 0,
      "Avision", "AV830C",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="complete" */
    
    { "AVISION", "AV830C Plus",
      0, 0,
      "Avision", "AV830C Plus",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="untested" */
    
    { "AVISION", "AV880",
      0, 0,
      "Avision", "AV880",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="untested" */
    
    { "AVISION", "AV880C",
      0, 0,
      "Avision", "AV880C",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="untested" */

    { "AVISION", "AV3200C",
      0, 0,
      "Avision", "AV3200C",
      AV_NON_INTERLACED_DUPLEX_300 | AV_FASTER_WITH_FILTER},
    /* comment="1 pass, ??? dpi" */
    /* status="complete" */

    { "AVISION", "AV3200SU",
      0x0638, 0x0A4E,
      "Avision", "AV3200SU",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="complete" */

    { "AVISION", "AV3730SU",
      0x0638, 0x0A4F,
      "Avision", "AV3730SU",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="complete" */

    { "AVISION", "AV3750SU",
      0x0638, 0x0A65,
      "Avision", "AV3750SU",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="complete" */

    { "AVISION", "AV3800C",
      0, 0,
      "Avision", "AV3800C",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="complete" */

    { "AVISION", "AV3850SU",
      0x0638, 0x0a66,
      "Avision", "AV3850SU",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="complete" */

    { "AVISION", "FB6000E",
      0, 0,
      "Avision", "FB6000E",
      AV_NON_INTERLACED_DUPLEX_300},
    /* comment="1 pass, 1200 dpi, A3 - duplex! - zero edge!" */
    /* status="complete" */

    { NULL, NULL,
      0x0638, 0x0a82,
      "Avision", "FB6080E",
      AV_NON_INTERLACED_DUPLEX_300},
    /* comment="1 pass, 1200 dpi, A3 - duplex! - zero edge!" */
    /* status="complete" */

    { NULL, NULL,
      0x0638, 0x0a84,
      "Avision", "FB2080E",
      0},
    /* comment="1 pass, 600 dpi, zero-edge" ASIC 7 */
    /* status="basic" */

    { "AVISION", "AV8000S",
      0, 0,
      "Avision", "AV8000S",
      AV_DOES_NOT_KEEP_WINDOW},
    /* comment="1 pass, 1200 dpi, A3" */
    /* status="complete" */

    { NULL, NULL,
      0x0638, 0x0a4d,
      "Avision", "AV8050U",
      AV_NON_INTERLACED_DUPLEX_300 | AV_DOES_NOT_KEEP_GAMMA},
    /* comment="1 pass, 1200 dpi, A3 - duplex!" */
    /* status="complete" */

    { "AVISION", "AV8300",
      0x0638, 0x0A40,
      "Avision", "AV8300",
      AV_NON_INTERLACED_DUPLEX_300 | AV_DOES_NOT_KEEP_GAMMA},
    /* comment="1 pass, 1200 dpi, A3 - duplex!" */
    /* status="complete" */

    { "AVISION", "AV8350",
      0x0638, 0x0A68,
      "Avision", "AV8350",
      AV_NON_INTERLACED_DUPLEX_300 | AV_DOES_NOT_KEEP_GAMMA},
    /* comment="1 pass, 1200 dpi, A3 - duplex!" */
    /* status="complete" */

    { NULL, NULL,
      0x0638, 0x0A61,
      "Avision", "IT8300",
      AV_NON_INTERLACED_DUPLEX_300 | AV_ACCEL_TABLE},
    /* comment="1 pass, 1200 dpi, A3 - duplex!, LCD screen, paper sensors" */
    /* status="good" */

    { NULL, NULL,
       0x0638, 0x0AA1,
      "Avision", "@V2500",
      0},
    /* comment="" */
    /* status="untested" */

    { NULL, NULL,
      0x0638, 0x0A45,
      "Avision", "@V5100",
      0},
    /* comment="1 pass, 1200 dpi, A3 - duplex!, LCD screen, paper sensors" */
    /* status="good" */

    { "AVISION", "AVA3",
      0, 0,
      "Avision", "AVA3",
      AV_FORCE_A3},
    /* comment="1 pass, 600 dpi, A3" */
    /* status="basic" */

    /* and possibly more avisions ;-) */
    
    { "HP",      "ScanJet 5300C",
      0x03f0, 0x0701,
      "Hewlett-Packard", "ScanJet 5300C",
      AV_INT_STATUS},
    /* comment="1 pass, 2400 dpi - some FW revisions have x-axis image scaling problems over 1200 dpi" */
    /* status="complete" */

    { "HP",      "ScanJet 5370C",
      0x03f0, 0x0701,
      "Hewlett-Packard", "ScanJet 5370C",
      AV_MULTI_CALIB_CMD | AV_INT_STATUS},
    /* comment="1 pass, 2400 dpi - some FW revisions have x-axis image scaling problems over 1200 dpi" */
    /* status="good" */
    
    { "hp",      "scanjet 7400c",
      0x03f0, 0x0801,
      "Hewlett-Packard", "ScanJet 7400c",
      AV_LIGHT_CHECK_BOGUS | AV_NO_64BYTE_ALIGN | AV_INT_STATUS},
    /* comment="1 pass, 2400 dpi - dual USB/SCSI interface" */
    /* status="good" */
    
#ifdef FAKE_ENTRIES_FOR_DESC_GENERATION
    { "hp",      "scanjet 7450c",
      0x03f0, 0x0801,
      "Hewlett-Packard", "ScanJet 7450c",
      AV_NO_64BYTE_ALIGN | AV_INT_STATUS},
    /* comment="1 pass, 2400 dpi - dual USB/SCSI interface" */
    /* status="good" */
    
    { "hp",      "scanjet 7490c",
      0x03f0, 0x0801,
      "Hewlett-Packard", "ScanJet 7490c",
      AV_NO_64BYTE_ALIGN | AV_INT_STATUS},
    /* comment="1 pass, 1200 dpi - dual USB/SCSI interface" */
    /* status="good" */

#endif
    { "HP",      "C9930A",
      0x03f0, 0x0b01,
      "Hewlett-Packard", "ScanJet 8200",
      AV_ADF_FLIPPING_DUPLEX | AV_FIRMWARE},
    /* comment="1 pass, 4800 (?) dpi - USB 2.0" */
    /* status="good" */

#ifdef FAKE_ENTRIES_FOR_DESC_GENERATION
    { "HP",      "C9930A",
      0x03f0, 0x0b01,
      "Hewlett-Packard", "ScanJet 8250",
      AV_ADF_FLIPPING_DUPLEX | AV_FIRMWARE},
    /* comment="1 pass, 4800 (?) dpi - USB 2.0" */
    /* status="good" */
#endif

    { "HP", "C9930A",
      0x03f0, 0x3905,
      "Hewlett-Packard", "ScanJet 8270",
      AV_ADF_FLIPPING_DUPLEX | AV_FIRMWARE},
    /* comment="1 pass, 4800 (?) dpi - USB 2.0" */
    /* status="good" */

#ifdef FAKE_ENTRIES_FOR_DESC_GENERATION
    { "HP", "C9930A",
      0x03f0, 0x0b01,
      "Hewlett-Packard", "ScanJet 8290",
      AV_ADF_FLIPPING_DUPLEX | AV_FIRMWARE},
    /* comment="1 pass, 4800 (?) dpi - USB 2.0 and SCSI - only SCSI tested so far" */
    /* status="good" */
    
#endif 
    { "HP", "C9930A",
      0x03f0, 0x3805,
      "Hewlett-Packard", "ScanJet 8300",
      0},
    /* comment="1 pass, 4800 (?) dpi - USB 2.0" */
    /* status="good" */

#ifdef FAKE_ENTRIES_FOR_DESC_GENERATION
    { "HP", "C9930A",
      0x03f0, 0x3805,
      "Hewlett-Packard", "ScanJet 8350",
      0},
    /* comment="1 pass, 4800 (?) dpi - USB 2.0" */
    /* status="good" */

    { "HP", "C9930A",
      0x03f0, 0x3805,
      "Hewlett-Packard", "ScanJet 8390",
      0},
    /* comment="1 pass, 4800 (?) dpi - USB 2.0" */
    /* status="good" */

#endif 
    { "Minolta", "#2882",
      0, 0,
      "Minolta", "Dimage Scan Dual I",
      AV_FORCE_FILM | AV_NO_START_SCAN}, /* not AV_FILMSCANNER (no frame control) */
    /* status="basic" */

    { "Minolta", "#2887",
      0, 0,
      "Minolta", "Scan Multi Pro",
      AV_FORCE_FILM | AV_NO_START_SCAN}, /* AV_FILMSCANNER (frame control)? */
    /* status="untested" */
    
    { "MINOLTA", "FS-V1",
      0x0638, 0x026a,
      "Minolta", "Dimage Scan Dual II",
      AV_FILMSCANNER | AV_ONE_CALIB_CMD | AV_12_BIT_MODE},
    /* comment="1 pass, film-scanner" */
    /* status="good" */
    
    { "MINOLTA", "Elite II",
      0x0686, 0x4004,
      "Minolta", "Elite II",
      AV_FILMSCANNER | AV_ONE_CALIB_CMD},
    /* comment="1 pass, film-scanner" */
    /* status="untested" */
    
    { "MINOLTA", "FS-V3",
      0x0686, 0x400d,
      "Minolta", "Dimage Scan Dual III",
      AV_FILMSCANNER | AV_ONE_CALIB_CMD | AV_ACCEL_TABLE},
    /* comment="1 pass, film-scanner" */
    /* status="good" */

    { "MINOLTA", "FS-V4",
      0x0686, 0x400e,
      "Minolta", "Dimage Scan Elite 5400",
      AV_FILMSCANNER | AV_ONE_CALIB_CMD | /*AV_ACCEL_TABLE |*/ AV_NO_START_SCAN},
    /* comment="1 pass, film-scanner" */
    /* status="good" */

    { "QMS", "SC-110",
      0x0638, 0x0a15,
      "Minolta-QMS", "SC-110",
      0},
    /* comment="" */
    /* status="untested" */

    { "QMS", "SC-215",
      0x0638, 0x0a16,
      "Minolta-QMS", "SC-215",
      0},
    /* comment="" */
    /* status="good" */
    
    { "MITSBISH", "MCA-ADFC",
      0, 0,
      "Mitsubishi", "MCA-ADFC",
      0},
    /* status="untested" */
    
    { "MITSBISH", "MCA-S1200C",
      0, 0,
      "Mitsubishi", "S1200C",
      0},
    /* status="untested" */
    
    { "MITSBISH", "MCA-S600C",
      0, 0,
      "Mitsubishi", "S600C",
      0},
    /* status="untested" */
    
    { "MITSBISH", "SS600",
      0, 0,
      "Mitsubishi", "SS600",
      0},
    /* status="good" */
    
    /* The next are all untested ... */
    
    { "FCPA", "ScanPartner",
      0, 0,
      "Fujitsu", "ScanPartner",
      AV_FUJITSU},
    /* status="untested" */

    { "FCPA", "ScanPartner 10",
      0, 0,
      "Fujitsu", "ScanPartner 10",
      AV_FUJITSU},
    /* status="untested" */
    
    { "FCPA", "ScanPartner 10C",
      0, 0,
      "Fujitsu", "ScanPartner 10C",
      AV_FUJITSU},
    /* status="untested" */
    
    { "FCPA", "ScanPartner 15C",
      0, 0,
      "Fujitsu", "ScanPartner 15C",
      AV_FUJITSU},
    /* status="untested" */
    
    { "FCPA", "ScanPartner 300C",
      0, 0,
      "Fujitsu", "ScanPartner 300C",
      0},
    /* status="untested" */
    
    { "FCPA", "ScanPartner 600C",
      0, 0,
      "Fujitsu", "ScanPartner 600C",
      0},
    /* status="untested" */

    { "FCPA", "ScanPartner 620C",
      0, 0,
      "Fujitsu", "ScanPartner 620C",
      AV_LIGHT_CHECK_BOGUS},
    /* status="good" */
    
    { "FCPA", "ScanPartner Jr",
      0, 0,
      "Fujitsu", "ScanPartner Jr",
      0},
    /* status="untested" */
    
    { "FCPA", "ScanStation",
      0, 0,
      "Fujitsu", "ScanStation",
      0},
    /* status="untested" */

    { NULL, NULL,
      0x04c5, 0x1029,
      "Fujitsu", "fi-4010CU",
      0},
    /* status="untested" */

    { NULL, NULL,
      0x04c5, 0x10ef,
      "Fujitsu", "fi-5015C",
      0},
    /* status="good" */

    { NULL, NULL,
      0x040a, 0x6001,
      "Kodak", "i30",
      AV_INT_BUTTON | AV_GRAY_MODES},
    /* status="untested" */

    { NULL, NULL,
      0x040a, 0x6002,
      "Kodak", "i40",
      AV_INT_BUTTON | AV_GRAY_MODES},
    /* status="basic" */
    
    { NULL, NULL,
      0x040a, 0x6003,
      "Kodak", "i50",
      AV_INT_BUTTON},
    /* status="untested" */
    
#ifdef FAKE_ENTRIES_FOR_DESC_GENERATION
    { NULL, NULL,
      0x040a, 0x6003,
      "Kodak", "i55",
      AV_INT_BUTTON},
    /* status="untested" */
#endif
    
    { NULL, NULL,
      0x040a, 0x6004,
      "Kodak", "i60",
      AV_INT_BUTTON},
    /* status="untested" */
    
#ifdef FAKE_ENTRIES_FOR_DESC_GENERATION
    { NULL, NULL,
      0x040a, 0x6004,
      "Kodak", "i65",
      AV_INT_BUTTON},
    /* status="untested" */
#endif
    
    { NULL, NULL,
      0x040a, 0x6005,
      "Kodak", "i80",
      AV_INT_BUTTON},
     /* status="good" */ 
    
    { "iVina", "1200U",
      0x0638, 0x0268,
      "iVina", "1200U",
      0},
    /* status="untested" */
    
    { NULL, NULL,
      0x04a7, 0x0424,
      "Visioneer", "Strobe XP 450",
      AV_INT_BUTTON  | AV_ACCEL_TABLE},
      /* comment="sheetfed scanner" */
      /* status="complete" */

    { NULL, NULL,
      0x04a7, 0x0491,
      "Visioneer", "Strobe XP 450-G",
      AV_INT_BUTTON  | AV_ACCEL_TABLE},
      /* comment="sheetfed scanner" */
      /* status="complete" */
    
    { NULL, NULL,
      0x04a7, 0x0479,
      "Visioneer", "Strobe XP 470",
      AV_INT_BUTTON  | AV_ACCEL_TABLE},
      /* comment="sheetfed scanner" */
      /* status="complete" */

    { NULL, NULL,
      0x04a7, 0x048F,
      "Visioneer", "Strobe XP 470-G",
      AV_INT_BUTTON  | AV_ACCEL_TABLE},
      /* comment="sheetfed scanner" */
      /* status="complete" */
    
    { NULL, NULL,
      0x04a7, 0x0420,
      "Visioneer", "9320",
      0},
      /* comment="sheetfed scanner" */
      /* status="complete" */

    { NULL, NULL,
      0x04a7, 0x0421,
      "Visioneer", "9450",
      AV_MULTI_CALIB_CMD | AV_ADF_BGR_ORDER_INVERT | AV_NO_BUTTON | AV_NO_TUNE_SCAN_LENGTH},
      /* comment="sheetfed scanner" */
      /* status="complete" */

    { NULL, NULL,
      0x04a7, 0x047A,
      "Visioneer", "9450-G",
      0},
      /* comment="sheetfed scanner" */
      /* status="complete" */

    { NULL, NULL,
      0x04a7, 0x0422,
      "Visioneer", "9550",
      0},
      /* comment="sheetfed scanner" */
      /* status="complete" */

    { NULL, NULL,
      0x04a7, 0x0390,
      "Visioneer", "9650",
      0},
      /* comment="sheetfed scanner" */
      /* status="complete" */

    { NULL, NULL,
      0x04a7, 0x047B,
      "Visioneer", "9650-G",
      0},
      /* comment="sheetfed scanner" */
      /* status="complete" */
    
    { NULL, NULL,
      0x04a7, 0x0423,
      "Visioneer", "9750",
      AV_INT_BUTTON},
      /* comment="sheetfed scanner" */
      /* status="complete" */

    { NULL, NULL,
      0x04a7, 0x0493,
      "Visioneer", "9750-G",
      AV_INT_BUTTON},
      /* comment="sheetfed scanner" */
      /* status="complete" */

    { NULL, NULL,
      0x04a7, 0x0497,
      "Visioneer", "Patriot 430",
      AV_INT_BUTTON | AV_2ND_LINE_INTERLACED | AV_NO_REAR | AV_SOFT_SCALE | AV_DOES_KEEP_WINDOW | AV_DOES_KEEP_GAMMA | AV_REAR_OFFSET},
      /* comment="sheetfed scanner" */
      /* status="complete" */

#ifdef FAKE_ENTRIES_FOR_DESC_GENERATION
    { NULL, NULL,
      0x04a7, 0x048F,
      "Visioneer", "Patriot 470",
      AV_INT_BUTTON},
      /* comment="sheetfed scanner" */
      /* status="complete" */
#endif

    { NULL, NULL,
      0x04a7, 0x0498,
      "Visioneer", "Patriot 680",
      AV_INT_BUTTON},
      /* comment="sheetfed scanner" */
      /* status="complete" */

    { NULL, NULL,
      0x04a7, 0x0499,
      "Visioneer", "Patriot 780",
      AV_INT_BUTTON},
      /* comment="sheetfed scanner" */
      /* status="complete" */
    
    { NULL, NULL,
      0x04a7, 0x049C,
      "Xerox", "DocuMate150",
      AV_INT_BUTTON | AV_SOFT_SCALE | AV_DOES_KEEP_WINDOW | AV_DOES_KEEP_GAMMA | AV_BACKGROUND_QUIRK},
    /* status="good" */

    { NULL, NULL,
      0x04a7, 0x0477,
      "Xerox", "DocuMate152",
      AV_INT_BUTTON | AV_2ND_LINE_INTERLACED | AV_NO_REAR | AV_SOFT_SCALE | AV_DOES_KEEP_WINDOW | AV_DOES_KEEP_GAMMA | AV_REAR_OFFSET | AV_BACKGROUND_QUIRK},
    /* status="good" */

    { NULL, NULL,
      0x04a7, 0x049D,
      "Xerox", "DocuMate162",
      AV_INT_BUTTON | AV_2ND_LINE_INTERLACED | AV_NO_REAR | AV_SOFT_SCALE | AV_DOES_KEEP_WINDOW | AV_DOES_KEEP_GAMMA | AV_REAR_OFFSET | AV_BACKGROUND_QUIRK},
    /* status="good" */

    { NULL, NULL,
      0x04a7, 0x0448,
      "Xerox", "DocuMate250",
      AV_INT_BUTTON},
    /* status="good" */

    { NULL, NULL,
      0x04a7, 0x0490,
      "Xerox", "DocuMate250-G",
      AV_INT_BUTTON},
    /* status="good" */
    
    { NULL, NULL,
      0x04a7, 0x0449,
      "Xerox", "DocuMate252",
      AV_INT_BUTTON},
    /* status="good" */

    { NULL, NULL,
      0x04a7, 0x048C,
      "Xerox", "DocuMate252-G",
      AV_INT_BUTTON},
    /* status="good" */
    
    { NULL, NULL,
      0x04a7, 0x0476,
      "Xerox", "DocuMate232",
      AV_INT_BUTTON},
    /* status="good" */

    { NULL, NULL,
      0x04a7, 0x044c,
      "Xerox", "DocuMate262",
      AV_INT_BUTTON},
    /* status="good" */

    { NULL, NULL,
      0x04a7, 0x048D,
      "Xerox", "DocuMate262-G",
      AV_INT_BUTTON},
    /* status="good" */
    
    { NULL, NULL,
      0x04a7, 0x04a7,
      "Xerox", "DocuMate262i",
      AV_INT_BUTTON},
    /* status="good" */

    { NULL, NULL,
      0x04a7, 0x0475,
      "Xerox", "DocuMate272",
      AV_INT_BUTTON},
    /* status="untested" */

    { NULL, NULL,
      0x04a7, 0x048E,
      "Xerox", "DocuMate272-G",
      AV_INT_BUTTON},
    /* status="untested" */
    
    { NULL, NULL,
      0x04a7, 0x0446,
      "Xerox", "DocuMate510",
      AV_INT_BUTTON},
    /* status="untested" */

    { NULL, NULL,
      0x04a7, 0x0495,
      "Xerox", "DocuMate512",
      AV_INT_BUTTON},
    /* status="untested" */

    { NULL, NULL,
      0x04a7, 0x047c,
      "Xerox", "DocuMate510-G",
      AV_INT_BUTTON},
    /* status="untested" */

    { NULL, NULL,
      0x04a7, 0x0447,
      "Xerox", "DocuMate520",
      AV_INT_BUTTON},
    /* status="untested" */

    { NULL, NULL,
      0x04a7, 0x0492,
      "Xerox", "DocuMate520-G",
      AV_INT_BUTTON},
    /* status="untested" */

#ifdef FAKE_ENTRIES_FOR_DESC_GENERATION
    { NULL, NULL,
      0x04a7, 0x0498,
      "Xerox", "DocuMate632",
      AV_INT_BUTTON},
    /* status="untested" */
#endif

    { NULL, NULL,
      0x04a7, 0x0478,
      "Xerox", "DocuMate752",
      AV_INT_BUTTON},
    /* status="untested" */

    { NULL, NULL,
      0x04a7, 0x049A,
      "Xerox", "DocuMate752",
      AV_INT_BUTTON},
    /* status="untested" */

#ifdef FAKE_ENTRIES_FOR_DESC_GENERATION
    { NULL, NULL,
      0x0638, 0x0a16,
      "OKI", "S700 Scancopier",
      0},
    /* comment="1 pass, 600 dpi, A4" */
    /* status="good" */
#endif

    { "B+H", "2000F",
      0, 0,
      "Bell+Howell", "2000F",
      0},
    /* comment="1 pass, ??? dpi, A4" */
    /* status="basic" */

    { NULL, NULL,
      0x0482, 0x0335,
      "Kyocera", "FS-1016MFP",
      0},
    /* comment="1 pass, ??? dpi, A4" */
    /* status="untested" */

    /* More IDs from the Avision dll:
       ArtiScan ProA3
       FB1065
       FB1265
       PHI860S
       PSDC SCSI
       SCSI Scan 19200
       V6240 */

    /* Possibly:
Lexmark 4600 MFP Option MFP Options 
Lexmark 4600 MFP Option (C772n) MFP Options   
Lexmark X215
Lexmark Optra Image X242 
Lexmark X443
Lexmark 3100
Lexmark 3200 
Lexmark X340 MFP Multifunction   
Lexmark X342n MFP Multifunction   
Lexmark X522
Lexmark X630
Lexmark X632E
Lexmark X642e MFP Multifunction   
Lexmark X644e MFP Multifunction   
Lexmark X646dte MFP Multifunction   
Lexmark X646e MFP Multifunction   
Lexmark X646ef MFP Multifunction   
Lexmark X772e Multifunction   
Lexmark X850e MFP Multifunction   
Lexmark X852e MFP Multifunction   
Lexmark X854e MFP Multifunction   
Lexmark X4500 MFP
     */
    
    /* last entry detection */
    { NULL, NULL,
      0, 0,
      NULL, NULL,
      0} 
  };

#if 0
  struct timeval tv;
  #define TIMING(txt) gettimeofday (&tv, NULL);				\
  DBG (4, "%lu: " txt "\n", tv.tv_sec * 1000000 + tv.tv_usec)
#else
  #define TIMING(txt)
#endif

/* used when scanner returns invalid range fields ... */
#define A4_X_RANGE 8.5 /* or 8.25 ? */
#define A4_Y_RANGE 11.8
#define A3_X_RANGE 11.8
#define A3_Y_RANGE 16.5 /* or 17 ? */
#define FILM_X_RANGE 1.0 /* really ? */
#define FILM_Y_RANGE 1.0
#define SHEETFEED_Y_RANGE 14.0

#define AVISION_CONFIG_FILE "avision.conf"

#define STD_INQUIRY_SIZE 0x24
#define AVISION_INQUIRY_SIZE_V1 0x60
#define AVISION_INQUIRY_SIZE_V2 0x88
#define AVISION_INQUIRY_SIZE_MAX AVISION_INQUIRY_SIZE_V2


#define AVISION_BASE_RES 300

/* calibration (shading) defines */

#define INVALID_WHITE_SHADING   0x0000
#define DEFAULT_WHITE_SHADING   0xFFF0

#define MAX_WHITE_SHADING       0xFFFF
/* originally the WHITE_MAP_RANGE was 0x4000 - but this always
 * resulted in slightly too dark images - thus I have chosen
 * 0x4FFF ... */
#define WHITE_MAP_RANGE         0x4FFF

#define INVALID_DARK_SHADING    0xFFFF
#define DEFAULT_DARK_SHADING    0x0000

#define read_constrains(s,var) {\
	if (s->hw->hw->feature_type & AV_NO_64BYTE_ALIGN) {\
		if (var % 64 == 0) var /= 2;\
		if (var % 64 == 0) var += 2;\
	}\
}\

static int num_devices;
static Avision_Device* first_dev;
static Avision_Scanner* first_handle;
static const SANE_Device** devlist = 0;

/* this is a bit hacky to get extra information in the attach callback */
static Avision_HWEntry* attaching_hw = 0;

/* disable the usage of a custom gamma-table */
static SANE_Bool disable_gamma_table = SANE_FALSE;

/* disable the calibration */
static SANE_Bool disable_calibration = SANE_FALSE;
static SANE_Bool force_calibration = SANE_FALSE;

/* force scanable areas to ISO(DIN) A4/A3 */
static SANE_Bool force_a4 = SANE_FALSE;
static SANE_Bool force_a3 = SANE_FALSE;

/* hardware resolutions to interpolate from */
static const int  hw_res_list_c5[] =
  {
    /* tested on AV600U */
    75, 150, 300, 600, 1200, 2400, 4800, /* ... */ 0
  };
static const int  hw_res_list_generic[] =
  {
    50, /* slower than 150 on the AV122/DM152, left for USB 1 host's preview */
    75, /* slower than 150 on the AV122/DM152, left for USB 1 host's */
    150, 200, 300,
    /* 400,*/ /* AV122 simplex y-scaling and duplex interlacing corrupt */
    600, 1200, 2400, 4800,
    /* ... */
    0
  };

static SANE_Bool static_calib_list[3] =
  {
    SANE_FALSE, SANE_FALSE, SANE_FALSE
  };

static const SANE_Range u8_range =
  {
    0, /* minimum */
    255, /* maximum */
    0 /* quantization */
  };

static const SANE_Range percentage_range =
  {
    SANE_FIX (-100), /* minimum */
    SANE_FIX (100), /* maximum */
    SANE_FIX (1) /* quantization */
  };

static const SANE_Range abs_percentage_range =
  {
    SANE_FIX (0), /* minimum */
    SANE_FIX (100), /* maximum */
    SANE_FIX (1) /* quantization */
  };

static const SANE_Range exposure_range =
  {
    0, /* minimum */
    1000, /* maximum */
    1 /* quantization */
  };

static const SANE_Range overscan_range =
  {
    SANE_FIX (0), /* minimum */
    SANE_FIX (4), /* maximum */ /* 4mm, measured on AV122, AV220C2, i40 */
    0 /* quantization */
  };

/* The 0x32 is a random guess based on USB logs. Might need a
   per-device value in the future - 0x32 was tested on the AV122,
   DM152, AV220. */
static const SANE_Range background_range =
  {
    0, /* minimum */
    0x32, /* maximum */
    0 /* quantization */
  };

static const uint8_t test_unit_ready[] =
  {
    AVISION_SCSI_TEST_UNIT_READY, 0x00, 0x00, 0x00, 0x00, 0x00
  };

static const uint8_t get_status[] =
  {
    AVISION_SCSI_GET_DATA_STATUS, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x0c, 0x00
  };

static size_t
max_string_size (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  int i;
  
  DBG (3, "max_string_size:\n");
  
  for (i = 0; strings[i]; ++ i) {
    size = strlen (strings[i]) + 1;
    if (size > max_size)
      max_size = size;
  }
  return max_size;
}

static SANE_Status
constrain_value (Avision_Scanner* s, SANE_Int option, void* value,
		 SANE_Int* info)
{
  DBG (3, "constrain_value:\n");
  return sanei_constrain_value (s->opt + option, value, info);
}

static void debug_print_raw (int dbg_level, char* info, const uint8_t* data,
			     size_t count)
{
  size_t i;
  
  DBG (dbg_level, "%s", info);
  for (i = 0; i < count; ++ i) {
    DBG (dbg_level, "  [%lu] %1d%1d%1d%1d%1d%1d%1d%1db %3oo %3dd %2xx\n",
	 (u_long) i,
	 BIT(data[i],7), BIT(data[i],6), BIT(data[i],5), BIT(data[i],4),
	 BIT(data[i],3), BIT(data[i],2), BIT(data[i],1), BIT(data[i],0),
	 data[i], data[i], data[i]);
  }
}

static void debug_print_hex_raw (int dbg_level, char* info, const uint8_t* data,
				 size_t count)
{
  int address = 0;
  char text [16*3+1];

  DBG (dbg_level, "%s", info);
  while (count) {
    char* t = text;
    int i = 0;
    while (i < 16 && count) {
      t += sprintf (t, "%02x ", *data++);
      count--; i++;
    }
    *--t = 0;
    
    DBG (dbg_level, "  [%08x] %s\n", address, text);
    address += 16;
  }
}

static void debug_print_nvram_data (int dbg_level, char* func,
				    nvram_data* nvram)
{
  DBG (dbg_level, "%s: pad scans:             %d\n",
       func, get_quad(nvram->pad_scans));
  DBG (dbg_level, "%s: ADF simplex scans:     %d\n",
       func, get_quad(nvram->adf_simplex_scans));
  DBG (dbg_level, "%s: ADF duplex scans:      %d\n",
       func, get_quad(nvram->adf_duplex_scans));
  DBG (dbg_level, "%s: flatbed scans:         %d\n",
       func, get_quad(nvram->flatbed_scans));
  
  DBG (dbg_level, "%s: flatbed leading edge:  %d\n",
       func, (int16_t)get_double(nvram->flatbed_leading_edge));
  DBG (dbg_level, "%s: flatbed side edge:     %d\n",
       func, (int16_t)get_double(nvram->flatbed_side_edge));
  DBG (dbg_level, "%s: ADF leading edge:      %d\n",
       func, (int16_t)get_double(nvram->adf_leading_edge));
  DBG (dbg_level, "%s: ADF side edge:         %d\n",
       func, (int16_t)get_double(nvram->adf_side_edge));
  DBG (dbg_level, "%s: ADF rear leading edge: %d\n",
       func, (int16_t)get_double(nvram->adf_rear_leading_edge));
  DBG (dbg_level, "%s: ADF rear side edge:    %d\n",
       func, (int16_t)get_double(nvram->adf_rear_side_edge));
  
  DBG (dbg_level, "%s: born month:            %d\n",
       func, get_double(nvram->born_month));
  DBG (dbg_level, "%s: born day:              %d\n",
       func, get_double(nvram->born_day));
  DBG (dbg_level, "%s: born year:             %d\n",
       func, get_double(nvram->born_year));

  DBG (dbg_level, "%s: first scan month:      %d\n",
       func, get_double(nvram->first_scan_month));
  DBG (dbg_level, "%s: first scan day:        %d\n",
       func, get_double(nvram->first_scan_day));
  DBG (dbg_level, "%s: first scan year:       %d\n",
       func, get_double(nvram->first_scan_year));
  

  DBG (dbg_level, "%s: vert. magnification:   %d\n",
       func, get_double(nvram->vertical_magnification));
  DBG (dbg_level, "%s: horiz. magnification:  %d\n",
       func, get_double(nvram->horizontal_magnification));

  DBG (dbg_level, "%s: CCD type:              %d\n",
       func, nvram->ccd_type);
  DBG (dbg_level, "%s: scan speed:            %d\n",
       func, nvram->scan_speed);
  
  DBG (dbg_level, "%s: serial:                '%.24s'\n", /* 24 chars max */
       func, nvram->serial);
  
  DBG (dbg_level, "%s: power saving time:     %d\n",
       func, get_double(nvram->power_saving_time));

  DBG (dbg_level, "%s: auto feed:             %d\n",
       func, nvram->auto_feed);

  DBG (dbg_level, "%s: roller count:          %d\n",
       func, get_quad(nvram->roller_count));
  DBG (dbg_level, "%s: multifeed count:       %d\n",
       func, get_quad(nvram->multifeed_count));
  DBG (dbg_level, "%s: jam count:             %d\n",
       func, get_quad(nvram->jam_count));

  DBG (dbg_level, "%s: identify info:         '%.16s'\n", /* 16 chars max */
       func, nvram->identify_info);
  DBG (dbg_level, "%s: formal_name:           '%.16s'\n", /* 16 chars max */
       func, nvram->formal_name);
}

static void debug_print_avdimen (int dbg_level, char* func,
				 Avision_Dimensions* avdimen)
{
  DBG (dbg_level, "%s: hw_xres: %d, hw_yres: %d, line_difference: %d\n",
       func, avdimen->hw_xres, avdimen->hw_yres, avdimen->line_difference);
  
  DBG (dbg_level, "%s: tlx: %ld, tly: %ld, brx: %ld, bry: %ld\n",
       func, avdimen->tlx, avdimen->tly,
       avdimen->brx, avdimen->bry);
  
  DBG (dbg_level, "%s: hw_pixel_per_line: %d, hw_lines: %d, hw_bytes_per_line: %d\n",
       func, avdimen->hw_pixels_per_line, avdimen->hw_lines, avdimen->hw_bytes_per_line);

  DBG (dbg_level, "%s: xres: %d, yres: %d\n",
       func, avdimen->xres, avdimen->yres);
}

static void debug_print_params (int dbg_level, char* func, SANE_Parameters* params)
{
  DBG (dbg_level, "%s: pixel_per_line: %d, lines: %d\n",
       func, params->pixels_per_line, params->lines);

  DBG (dbg_level, "%s: depth: %d, bytes_per_line: %d\n",
       func, params->depth, params->bytes_per_line);
}

static void debug_print_calib_format (int dbg_level, char* func,
				      uint8_t* result)
{
  debug_print_raw (dbg_level + 2, "debug_print_calib_format:\n", result, 32);
  
  DBG (dbg_level, "%s: [0-1]  pixels per line: %d\n",
       func, get_double ( &(result[0]) ));
  DBG (dbg_level, "%s: [2]    bytes per channel: %d\n", func, result[2]);
  DBG (dbg_level, "%s: [3]    line count: %d\n", func, result[3]);
  
  DBG (dbg_level, "%s: [4]    FLAG:%s%s%s\n",
       func,
       result[4] == 1?" MUST_DO_CALIBRATION":"",
       result[4] == 2?" SCAN_IMAGE_DOES_CALIBRATION":"",
       result[4] == 3?" NEEDS_NO_CALIBRATION":"");
  
  DBG (dbg_level, "%s: [5]    Ability1:%s%s%s%s%s%s%s%s\n",
       func,
       BIT(result[5],7)?" NONE_PACKED":" PACKED",
       BIT(result[5],6)?" INTERPOLATED":"",
       BIT(result[5],5)?" SEND_REVERSED":"",
       BIT(result[5],4)?" PACKED_DATA":"",
       BIT(result[5],3)?" COLOR_CALIB":"",
       BIT(result[5],2)?" DARK_CALIB":"",
       BIT(result[5],1)?" NEEDS_WHITE_BLACK_SHADING_DATA":"",
       BIT(result[5],0)?" NEEDS_CALIB_TABLE_CHANNEL_BY_CHANNEL":"");
  
  DBG (dbg_level, "%s: [6]    R gain: %d\n", func, result[6]);
  DBG (dbg_level, "%s: [7]    G gain: %d\n", func, result[7]);
  DBG (dbg_level, "%s: [8]    B gain: %d\n", func, result[8]);
  
  DBG (dbg_level, "%s: [9-10] R shading target: %x\n",
       func, get_double ( &(result[9]) ) );
  DBG (dbg_level, "%s: [11-12] G shading target: %x\n",
       func, get_double ( &(result[11]) ) );
  DBG (dbg_level, "%s: [13-14] B shading target: %x\n",
       func, get_double ( &(result[13]) ) );
  
  DBG (dbg_level, "%s: [15-16] R dark shading target: %x\n",
       func, get_double ( &(result[15]) ) );
  DBG (dbg_level, "%s: [17-18] G dark shading target: %x\n",
       func, get_double ( &(result[17]) ) );
  DBG (dbg_level, "%s: [19-20] B dark shading target: %x\n",
       func, get_double ( &(result[19]) ) );

  DBG (dbg_level, "%s: [21]    true-gray gain: %d\n", func, result[21]);
  DBG (dbg_level, "%s: [22-23] true-gray shading target: %x\n",
       func, get_double ( &(result[22]) ) );
  
  DBG (dbg_level, "%s: [24-25] true-gray dark shading target: %x\n",
       func, get_double ( &(result[24]) ) );
}

static void debug_print_accel_info (int dbg_level, char* func,
				    uint8_t* result)
{
  debug_print_raw (dbg_level + 2, "debug_print_accel_info:\n", result, 24);
  
  DBG (dbg_level, "%s: [0-1]   acceleration step count: %d\n",
       func, get_double ( &(result[0]) ));
  DBG (dbg_level, "%s: [2-3]   stable step count: %d\n",
       func, get_double ( &(result[2]) ));
  DBG (dbg_level, "%s: [4-7]   table units: %d\n",
       func, get_quad ( &(result[4]) ));
  DBG (dbg_level, "%s: [8-11]  base units: %d\n",
       func, get_quad ( &(result[8]) ));
  DBG (dbg_level, "%s: [12-13] start speed: %d\n",
       func, get_double ( &(result[12]) ));
  DBG (dbg_level, "%s: [14-15] target speed: %d\n",
       func, get_double ( &(result[14]) ));
  DBG (dbg_level, "%s: [16]    ability:%s%s\n",
       func,
       BIT(result[16],0)?" TWO_BYTES_PER_ELEM":" SINGLE_BYTE_PER_ELEM",
       BIT(result[16],1)?" LOW_HIGH_ORDER":" HIGH_LOW_ORDER");
  DBG (dbg_level, "%s: [17]    table count: %d\n", func, result[17]);
  
}

static void debug_print_window_descriptor (int dbg_level, char* func,
					   command_set_window_window* window)
{
  debug_print_raw (dbg_level + 1, "window_data_header: \n",
		   (uint8_t*)(&window->header),
		   sizeof(window->header));

  debug_print_raw (dbg_level + 1, "window_descriptor: \n",
		   (uint8_t*)(&window->descriptor),
		   sizeof(*window) -
		   sizeof(window->header));
  
  DBG (dbg_level, "%s: [0]     window_id: %d\n", func,
       window->descriptor.winid);
  DBG (dbg_level, "%s: [2-3]   x-axis res: %d\n", func,
       get_double (window->descriptor.xres));
  DBG (dbg_level, "%s: [4-5]   y-axis res: %d\n", func,
       get_double (window->descriptor.yres));
  DBG (dbg_level, "%s: [6-9]   x-axis upper left: %d\n",
       func, get_quad (window->descriptor.ulx));
  DBG (dbg_level, "%s: [10-13] y-axis upper left: %d\n",
       func, get_quad (window->descriptor.uly));
  DBG (dbg_level, "%s: [14-17] window width: %d\n", func,
       get_quad (window->descriptor.width));
  DBG (dbg_level, "%s: [18-21] window length: %d\n", func, 
       get_quad (window->descriptor.length));
  DBG (dbg_level, "%s: [22]    brightness: %d\n", func,
       window->descriptor.brightness);
  DBG (dbg_level, "%s: [23]    threshold: %d\n", func,
       window->descriptor.threshold);
  DBG (dbg_level, "%s: [24]    contrast: %d\n", func,
       window->descriptor.contrast);
  DBG (dbg_level, "%s: [25]    image composition: %x\n", func,
       window->descriptor.image_comp);
  DBG (dbg_level, "%s: [26]    bits per channel: %d\n", func,
       window->descriptor.bpc);
  DBG (dbg_level, "%s: [27-28] halftone pattern: %x\n", func,
       get_double (window->descriptor.halftone));
  DBG (dbg_level, "%s: [29]    padding_and_bitset: %x\n", func,
       window->descriptor.padding_and_bitset);
  DBG (dbg_level, "%s: [30-31] bit ordering: %x\n", func,
       get_double (window->descriptor.bitordering));
  DBG (dbg_level, "%s: [32]    compression type: %x\n", func,
       window->descriptor.compr_type);
  DBG (dbg_level, "%s: [33]    compression argument: %x\n", func,
       window->descriptor.compr_arg);
  DBG (dbg_level, "%s: [34-35] paper length: %x\n", func,
       get_double (window->descriptor.paper_length) );
  DBG (dbg_level, "%s: [40]    vendor id: %x\n", func,
       window->descriptor.vendor_specific);
  DBG (dbg_level, "%s: [41]    param length: %d\n", func,
       window->descriptor.paralen);
  DBG (dbg_level, "%s: [42]    bitset1: %x\n", func,
       window->avision.bitset1);
  DBG (dbg_level, "%s: [43]    highlight: %d\n", func,
       window->avision.highlight);
  DBG (dbg_level, "%s: [44]    shadow: %d\n", func,
       window->avision.shadow);
  DBG (dbg_level, "%s: [45-46] line-width: %d\n", func,
       get_double (window->avision.line_width));
  DBG (dbg_level, "%s: [47-48] line-count: %d\n", func,
       get_double (window->avision.line_count));
  DBG (dbg_level, "%s: [49]    bitset2: %x\n", func,
       window->avision.type.normal.bitset2);
  DBG (dbg_level, "%s: [50]    ir exposure time: %x\n",
       func, window->avision.type.normal.ir_exposure_time);
  
  DBG (dbg_level, "%s: [51-52] r exposure: %x\n", func,
       get_double (window->avision.type.normal.r_exposure_time));
  DBG (dbg_level, "%s: [53-54] g exposure: %x\n", func,
       get_double (window->avision.type.normal.g_exposure_time));
  DBG (dbg_level, "%s: [55-56] b exposure: %x\n", func,
       get_double (window->avision.type.normal.b_exposure_time));
  
  DBG (dbg_level, "%s: [57]    bitset3: %x\n", func,
       window->avision.type.normal.bitset3);
  DBG (dbg_level, "%s: [58]    auto focus: %d\n", func,
       window->avision.type.normal.auto_focus);
  DBG (dbg_level, "%s: [59]    line-width (MSB): %d\n",
       func, window->avision.type.normal.line_width_msb);
  DBG (dbg_level, "%s: [60]    line-count (MSB): %d\n",
       func, window->avision.type.normal.line_count_msb);
  DBG (dbg_level, "%s: [61]    background lines: %d\n",
       func, window->avision.type.normal.background_lines);
}

static int write_pnm_header (FILE* f, color_mode m, int depth, int width, int height)
{
  int maxval = (1 << depth) - 1;
  const char* hdr_str = NULL;
  /* construct PNM header */
  
  switch (m) {
  case AV_THRESHOLDED:
  case AV_DITHERED:
    hdr_str = "P4\n%d %d\n";
    break;
  case AV_GRAYSCALE:
  case AV_GRAYSCALE12:
  case AV_GRAYSCALE16:
    hdr_str = "P5\n%d %d\n%d\n";
    break;
  case AV_TRUECOLOR:
  case AV_TRUECOLOR12:
  case AV_TRUECOLOR16:
    hdr_str = "P6\n%d %d\n%d\n";
    break;
  case  AV_COLOR_MODE_LAST:
    ; /* silence compiler warning */
  }
  
  return fprintf (f, hdr_str, width, height, maxval);
}

static SANE_Status
sense_handler (int fd, u_char* sense, void* arg)
{
  SANE_Status status = SANE_STATUS_IO_ERROR; /* default case */
  
  char* text;
  char textbuf[64];
  
  uint8_t error_code = sense[0] & 0x7f;
  uint8_t sense_key = sense[2] & 0xf;
  uint8_t additional_sense = sense[7];
  
  fd = fd; /* silence gcc */
  arg = arg; /* silence gcc */
  
  DBG (3, "sense_handler:\n");
  
  switch (error_code)
    {
    case 0x70:
      text = "standard sense";
      break;
    case 0x7f:
      text = "Avision-specific sense";
      break;
    default:
      text = "unknown sense";
    }
  
  debug_print_raw (1, "sense_handler: data:\n", sense, 8 + additional_sense);
  
  /* request valid? */
  if (! (sense[0] & (1<<7))) {
    DBG (1, "sense_handler: sense not valid ...\n");
    return status;
  }
  
  switch (sense_key)
    {
    case 0x00:
      status = SANE_STATUS_GOOD;
      text = "ok ?!?";
      break;
    case 0x02:
      text = "NOT READY";
      break;
    case 0x03:
      text = "MEDIUM ERROR (mostly ADF)";
      status = SANE_STATUS_JAMMED;
      break;
    case 0x04:
      text = "HARDWARE ERROR";
      break;
    case 0x05:
      text = "ILLEGAL REQUEST";
      break;
    case 0x06:
      text = "UNIT ATTENTION";
      break;
    case 0x09:
      text = "VENDOR SPECIFIC";
      break;
    case 0x0b:
      text = "ABORTED COMMAND";
      status = SANE_STATUS_CANCELLED; /* AV610C2 cancel button */
      break;
    default:
      sprintf (textbuf, "got unknown sense code 0x%02x", (int)sense_key);
      text = textbuf;
    }
  
  DBG (1, "sense_handler: sense code: %s\n", text);
  
  if (sense[2] & (1<<6))
    DBG (1, "sense_handler: end of scan\n");   
  else 
    DBG (1, "sense_handler: scan has not yet been completed\n");
  
  if (sense[2] & (1<<5))
    DBG (1, "sense_handler: incorrect logical length\n");
  else 
    DBG (1, "sense_handler: correct logical length\n");

  {  
    uint8_t asc = sense[12];
    uint8_t ascq = sense[13];
    
#define ADDITIONAL_SENSE(asc,ascq,txt)			\
    case ( (asc << 8) + ascq): text = txt; break
    
    switch ( (asc << 8) + ascq )
      {
	/* normal */
	ADDITIONAL_SENSE (0x00,0x00, "No additional sense information");
	ADDITIONAL_SENSE (0x00,0x06, "I/O process terminated");
	ADDITIONAL_SENSE (0x15,0x01, "Mechanical positioning error");

	ADDITIONAL_SENSE (0x15,0x02, "Flatbed Home Sensor Error (OKI only");
	ADDITIONAL_SENSE (0x15,0x03, "ADF Home Sensor Error (OKI only)");
	ADDITIONAL_SENSE (0x15,0x04, "Lock Error (OKI only)");
	
	ADDITIONAL_SENSE (0x1a,0x00, "parameter list length error");
	
	ADDITIONAL_SENSE (0x20,0x00, "Invalid command");
	ADDITIONAL_SENSE (0x24,0x00, "Invalid field in CDB");
	ADDITIONAL_SENSE (0x25,0x00, "Logical unit not supported");
	ADDITIONAL_SENSE (0x26,0x00, "Invalid field in parameter list");
	ADDITIONAL_SENSE (0x26,0x01, "parameter not supported");
	ADDITIONAL_SENSE (0x26,0x02, "parameter value invalid");
	ADDITIONAL_SENSE (0x29,0x00, "Power-on, reset or bus device reset occurred");
	ADDITIONAL_SENSE (0x2c,0x02, "Invalid combination of window specified");
	ADDITIONAL_SENSE (0x2f,0x00, "Command cleared by another initiator");

	ADDITIONAL_SENSE (0x3D,0x00, "Invalid Bit in Identify Message");
	
	ADDITIONAL_SENSE (0x43,0x00, "Message error");
	ADDITIONAL_SENSE (0x44,0x00, "Internal target failure");
	ADDITIONAL_SENSE (0x44,0x01, "Flatbed DRAM Error(OKI only)");
	ADDITIONAL_SENSE (0x44,0x02, "ADF DRAM Error(OKI only)");
	ADDITIONAL_SENSE (0x44,0x03, "Write NVRAM Error");
	ADDITIONAL_SENSE (0x47,0x00, "SCSI parity error");
	ADDITIONAL_SENSE (0x49,0x00, "Invalid message error");
	
	ADDITIONAL_SENSE (0x60,0x00, "Lamp failure");
	ADDITIONAL_SENSE (0x60,0x01, "Flatbed Lamp error (Oki only)");
	ADDITIONAL_SENSE (0x60,0x02, "ADF lamp error (Oki only)");
	ADDITIONAL_SENSE (0x62,0x00, "Scan head positioning error");
	
	ADDITIONAL_SENSE (0x80,0x01, "ADF paper jam"; status = SANE_STATUS_JAMMED);
	ADDITIONAL_SENSE (0x80,0x02, "ADF cover open"; status = SANE_STATUS_COVER_OPEN);
	ADDITIONAL_SENSE (0x80,0x03, "ADF chute empty"; status = SANE_STATUS_NO_DOCS);
	ADDITIONAL_SENSE (0x80,0x04, "ADF paper end"; status = SANE_STATUS_EOF);
	ADDITIONAL_SENSE (0x80,0x05, "Multi-feed (AV220,Kodak)");
	ADDITIONAL_SENSE (0x80,0x06, "ADF prefeeding (OKI only)");
	ADDITIONAL_SENSE (0x80,0x07, "Flatbed cover open (OKI only)"; status = SANE_STATUS_COVER_OPEN);
	ADDITIONAL_SENSE (0x80,0x08, "FW module doesn't match with scanner");
        ADDITIONAL_SENSE (0x80,0x09, "Papers fed from multiple trays (DM272)");
        ADDITIONAL_SENSE (0x80,0x0A, "ADF Paper Start");
        ADDITIONAL_SENSE (0x80,0x0B, "Multiple ADF paper End and Start");
        ADDITIONAL_SENSE (0x80,0x0C, "Multiple ADF paper End");
	
        /* film scanner */
	ADDITIONAL_SENSE (0x81,0x00, "ADF/MFP front door open"; status = SANE_STATUS_COVER_OPEN);
	ADDITIONAL_SENSE (0x81,0x01, "ADF holder cartridge open"; status = SANE_STATUS_COVER_OPEN);
	ADDITIONAL_SENSE (0x81,0x02, "ADF no film inside"; status = SANE_STATUS_NO_DOCS);
	ADDITIONAL_SENSE (0x81,0x03, "ADF initial load fail");
	ADDITIONAL_SENSE (0x81,0x04, "ADF film end"; status = SANE_STATUS_NO_DOCS);
	ADDITIONAL_SENSE (0x81,0x05, "ADF forward feed error");
	ADDITIONAL_SENSE (0x81,0x06, "ADF rewind error");
	ADDITIONAL_SENSE (0x81,0x07, "ADF set unload");
	ADDITIONAL_SENSE (0x81,0x08, "ADF adapter error");

	ADDITIONAL_SENSE (0xA0,0x01, "Filter Positioning Error");
	
	ADDITIONAL_SENSE (0x90,0x00, "Scanner busy (FW busy)");
	
      default:
	sprintf (textbuf, "Unknown sense code asc: 0x%02x, ascq: 0x%02x",
		 (int)asc, (int)ascq);
	text = textbuf;
      }
    
#undef ADDITIONAL_SENSE
    
    DBG (1, "sense_handler: sense code: %s\n", text);
    
    /* sense code specific for invalid request
     * it is possible to get a detailed error location here ;-)*/
    if (sense_key == 0x05) {
      if (sense[15] & (1<<7) )
	{
	  if (sense[15] & (1<<6) )
	    DBG (1, "sense_handler: error in command parameter\n");
	  else
	    DBG (1, "sense_handler: error in data parameter\n");
	  
	  DBG (1, "sense_handler: error in parameter byte: %d, %x\n",
	       get_double(&(sense[16])),  get_double(&(sense[16])));
	  
	  /* bit pointer valid ?*/
	  if (sense[15] & (1<<3) )
	    DBG (1, "sense_handler: error in command parameter\n");
	  else
	    DBG (1, "sense_handler: bit pointer invalid\n");
	}
    }
  }
  
  return status;
}

/*
 * Avision scsi/usb multiplexers - to keep the code clean:
 */

static SANE_Status
avision_usb_status (Avision_Connection* av_con, int retry, int timeout)
{
  SANE_Status status = 0;
  uint8_t usb_status[1] = {0};
  size_t count = 0;
  int t_retry = retry;

#define valid_status(status,a) (status == SANE_STATUS_GOOD ? a : 0)
  
  DBG (4, "avision_usb_status: timeout %d, %d retries\n", timeout, retry);
#ifndef HAVE_SANEI_USB_SET_TIMEOUT
#error "You must update include/sane/sanei_usb.h and sanei/sanei_usb.c accordingly!"
#endif
  sanei_usb_set_timeout (timeout);

  /* 1st try bulk transfers - they are more lightweight ... */
  for (;
       count == 0 &&
       (av_con->usb_status == AVISION_USB_BULK_STATUS ||
	av_con->usb_status == AVISION_USB_UNTESTED_STATUS) &&
       retry > 0;
       --retry)
    {
      count = sizeof (usb_status);
      
      DBG (5, "==> (bulk read) going down ...\n");
      status = sanei_usb_read_bulk (av_con->usb_dn, usb_status,
				    &count);
      DBG (5, "<== (bulk read) got: %ld, status: %d\n",
	   (u_long)count, valid_status(status, usb_status[0]));

      if (count > 0) {
	av_con->usb_status = AVISION_USB_BULK_STATUS;
      }
    }
  
  /* reset retry count ... */
  retry = t_retry;
    
  /* 2nd try interrupt status read - if not yet disabled */
  for (;
       count == 0 &&
       (av_con->usb_status == AVISION_USB_INT_STATUS ||
	av_con->usb_status == AVISION_USB_UNTESTED_STATUS) &&
       retry > 0;
       --retry)
  {
    count = sizeof (usb_status);
    
    DBG (5, "==> (interrupt read) going down ...\n");
    status = sanei_usb_read_int (av_con->usb_dn, usb_status,
				 &count);
    DBG (5, "<== (interrupt read) got: %ld, status: %d\n",
	 (u_long)count, valid_status(status, usb_status[0]));
    
    if (count > 0)
      av_con->usb_status = AVISION_USB_INT_STATUS;
  }  
  
  if (status != SANE_STATUS_GOOD)
    return status;
 
  if (count == 0)
    return SANE_STATUS_IO_ERROR;
 
  /* 0 = ok, 2 => request sense, 8 ==> busy, else error */
  switch (usb_status[0])
    {
    case AVISION_USB_GOOD:
      return SANE_STATUS_GOOD;
    case AVISION_USB_REQUEST_SENSE:
      DBG (2, "avision_usb_status: Needs to request sense!\n");
      return SANE_STATUS_INVAL;
    case AVISION_USB_BUSY:
      DBG (2, "avision_usb_status: Busy!\n");
      return SANE_STATUS_DEVICE_BUSY;
    default:
      DBG (1, "avision_usb_status: Unknown!\n");
      return SANE_STATUS_INVAL;
    }
}

static SANE_Status avision_open (const char* device_name,
				 Avision_Connection* av_con,
				 SANEI_SCSI_Sense_Handler sense_handler,
				 void *sense_arg)
{
  if (av_con->connection_type == AV_SCSI) {
    return sanei_scsi_open (device_name, &(av_con->scsi_fd),
			    sense_handler, sense_arg);
  }
  else {
    SANE_Status status;
    status = sanei_usb_open (device_name, &(av_con->usb_dn));
    return status;
  }
}

static SANE_Status avision_open_extended (const char* device_name,
					  Avision_Connection* av_con,
					  SANEI_SCSI_Sense_Handler sense_handler,
					  void *sense_arg, int *buffersize)
{
  if (av_con->connection_type == AV_SCSI) {
    return sanei_scsi_open_extended (device_name, &(av_con->scsi_fd),
				     sense_handler, sense_arg, buffersize);
  }
  else {
    SANE_Status status;
    status = sanei_usb_open (device_name, &(av_con->usb_dn));
    return status;
  }
}

static void avision_close (Avision_Connection* av_con)
{
  if (av_con->connection_type == AV_SCSI) {
    sanei_scsi_close (av_con->scsi_fd);
    av_con->scsi_fd = -1;
  }
  else {
    sanei_usb_close (av_con->usb_dn);
    av_con->usb_dn = -1;
  }
}

static SANE_Bool avision_is_open (Avision_Connection* av_con)
{
  if (av_con->connection_type == AV_SCSI) {
    return av_con->scsi_fd >= 0;
  }
  else {
    return av_con->usb_dn >= 0;
  }
}

static SANE_Status avision_cmd (Avision_Connection* av_con,
				const void* cmd, size_t cmd_size,
				const void* src, size_t src_size,
				void* dst, size_t* dst_size)
{
  if (av_con->connection_type == AV_SCSI) {
    return sanei_scsi_cmd2 (av_con->scsi_fd, cmd, cmd_size,
			    src, src_size, dst, dst_size);
  }
  else {
    SANE_Status status = SANE_STATUS_GOOD;
    
    size_t i, count, out_count;
    /* some commands on some devices need a rather long time to respond */
#define STD_TIMEOUT 30000
#define STD_STATUS_TIMEOUT 10000
    int retry = 4;
    int write_timeout = STD_TIMEOUT;
    int read_timeout = STD_TIMEOUT;
    int status_timeout = STD_STATUS_TIMEOUT;

    /* simply to allow nicer code below */
    const uint8_t* m_cmd = (const uint8_t*)cmd;
    const uint8_t* m_src = (const uint8_t*)src;
    uint8_t* m_dst = (uint8_t*)dst;
    
    /* may I vote for the possibility to use C99 ... */
#define min_usb_size 10
#define max_usb_size 256 * 1024 /* or 0x10000, used by AV Windows driver during background raster read, ... ? */
    
    /* 1st send command data - at least 10 Bytes for USB scanners */
    uint8_t enlarged_cmd [min_usb_size];
    if (cmd_size < min_usb_size) {
      DBG (1, "filling command to have a length of 10, was: %lu\n", (u_long) cmd_size);
      memcpy (enlarged_cmd, m_cmd, cmd_size);
      memset (enlarged_cmd + cmd_size, 0, min_usb_size - cmd_size);
      m_cmd = enlarged_cmd;
      cmd_size = min_usb_size;
    }

    /* per command class timeout tweaks */
    switch (m_cmd[0]) {
      case AVISION_SCSI_INQUIRY:
	read_timeout = 1000; /* quickly timeout on initial detection */
	status_timeout = 1000;
        break;
      case AVISION_SCSI_TEST_UNIT_READY:
        read_timeout = 15000; /* quickly timeout on initial detection */
	status_timeout = 15000;
        break;
    }

    DBG (7, "Timeouts: write: %d, read: %d, status: %d\n",
         write_timeout, read_timeout, status_timeout);

write_usb_cmd:
    if (--retry == 0) {
      DBG (1, "Max retry count reached: I/O error\n");
      return SANE_STATUS_IO_ERROR;
    }

    count = cmd_size;

    sanei_usb_set_timeout (write_timeout);
    DBG (8, "try to write cmd, count: %lu.\n", (u_long) count);
    status = sanei_usb_write_bulk (av_con->usb_dn, m_cmd, &count);
      
    DBG (8, "wrote %lu bytes\n", (u_long) count);
    if (status != SANE_STATUS_GOOD || count != cmd_size) {
      DBG (3, "=== Got error %d trying to write, wrote: %ld. ===\n",
           status, (long)count);
      
      if (status != SANE_STATUS_GOOD) /* == SANE_STATUS_EOF) */ {
	DBG (3, "try to read status to clear the FIFO\n");
	status = avision_usb_status (av_con, 1, 500);
	if (status != SANE_STATUS_GOOD) {
	  DBG (3, "=== Got error %d trying to read status. ===\n", status);
	  return SANE_STATUS_IO_ERROR;
	}
	else
	  goto write_usb_cmd;
      } else {
	DBG (3, "Retrying to send command\n");
	goto write_usb_cmd;
      }
      
      return SANE_STATUS_IO_ERROR;
    }
    
    /* 2nd send command data (if any) */
    for (i = 0; i < src_size; ) {
      
      count = src_size - i;
      /* if (count > max_usb_size)
  	   count = max_usb_size; */
      
      DBG (8, "try to write src, count: %lu.\n", (u_long) count);
      sanei_usb_set_timeout (write_timeout);
      status = sanei_usb_write_bulk (av_con->usb_dn, &(m_src[i]), &count);
      
      DBG (8, "wrote %lu bytes\n", (u_long) count);
      if (status == SANE_STATUS_GOOD) {
	i += count;
      }
      else {
	goto write_usb_cmd;
      }
    }

    /* 3rd: read the resulting data (payload) (if any) */
    if (status == SANE_STATUS_GOOD && dst != NULL && *dst_size > 0) {
      out_count = 0;
      sanei_usb_set_timeout (read_timeout);
      while (out_count < *dst_size) {
	count = (*dst_size - out_count);
	
	DBG (8, "try to read %lu bytes\n", (u_long) count);
        status = sanei_usb_read_bulk(av_con->usb_dn, &(m_dst[out_count]),
				     &count);
	DBG (8, "read %lu bytes\n", (u_long) count);

	if (count == 1 && (*dst_size - out_count > 1)) {
	  DBG (1, "Got 1 byte - status? (%d) Resending.\n", m_dst[out_count]);
	  goto write_usb_cmd;
        }
	else if (count > 0) {
          out_count += count;
	}
	else {
	  DBG (1, "No data arrived.\n");
	  goto write_usb_cmd;
	}
      }
    }
    
    /* last: read the device status via a pseudo interrupt transfer
     * this is needed - otherwise the scanner will hang ... */
    sanei_usb_set_timeout (status_timeout);
    status = avision_usb_status (av_con, /*retry*/ 1, status_timeout);
    /* next i/o hardening attempt - and yes this gets ugly ... */
    if (status != SANE_STATUS_GOOD && status != SANE_STATUS_INVAL)
      goto write_usb_cmd;
   
    if (status == SANE_STATUS_INVAL) {
      struct {
	command_header header;
	uint8_t pad[4];
      } sense_cmd;
      
      uint8_t sense_buffer[22];
      
      DBG (3, "Error during status read!\n");
      DBG (3, "=== Try to request sense ===\n");
      
      /* we can not call avision_cmd recursively - we might ending in
	 an endless recursion requesting sense for failing request
	 sense transfers ...*/
      
      memset (&sense_cmd, 0, sizeof (sense_cmd) );
      memset (&sense_buffer, 0, sizeof (sense_buffer) );
      sense_cmd.header.opc = AVISION_SCSI_REQUEST_SENSE;
      sense_cmd.header.len = sizeof (sense_buffer);
      
      count = sizeof(sense_cmd);
      
      DBG (8, "try to write %lu bytes\n", (u_long) count);
      sanei_usb_set_timeout (write_timeout);
      status = sanei_usb_write_bulk (av_con->usb_dn,
				     (uint8_t*) &sense_cmd, &count);
      DBG (8, "wrote %lu bytes\n", (u_long) count);
      
      if (status != SANE_STATUS_GOOD) {
	DBG (3, "=== Got error %d trying to request sense! ===\n", status);
      }
      else {
	count = sizeof (sense_buffer);
	
	DBG (8, "try to read %lu bytes sense data\n", (u_long) count);
	sanei_usb_set_timeout (read_timeout);
	status = sanei_usb_read_bulk(av_con->usb_dn, sense_buffer, &count);
	DBG (8, "read %lu bytes sense data\n", (u_long) count);
	
	/* we need to read out the status from the scanner i/o buffer */
	status = avision_usb_status (av_con, 1, status_timeout);
	
	/* some scanner return NEED_SENSE even after reading it */
	if (status != SANE_STATUS_GOOD && status != SANE_STATUS_INVAL)
	  DBG (3, "=== Got error %d trying to read sense! ===\n", status);
	else {
	  /* read complete -> call our sense handler */
	  status = sense_handler (-1, sense_buffer, 0);
	}
      } /* end read sense data */
    } /* end request sense */
    return status;
  } /* end cmd usb */
}

/* A bubble sort for the calibration. It only sorts the first third
 * and returns an average of the top 2/3 values. The input data is
 * 16bit big endian and the count is the count of the words - not
 * bytes! */

static uint16_t
bubble_sort (uint8_t* sort_data, size_t count)
{
  size_t i, j, limit, k;
  double sum = 0.0;
  
  limit = count / 3;
  
  for (i = 0; i < limit; ++i)
    {
      uint16_t ti = 0;
      uint16_t tj = 0;
      
      for (j = (i + 1); j < count; ++j)
	{
	  ti = get_double ((sort_data + i*2));
	  tj = get_double ((sort_data + j*2));
	  
	  if (ti > tj) {
	    set_double ((sort_data + i*2), tj);
	    set_double ((sort_data + j*2), ti);
	  }
	}
    }
  
  for (k = 0, i = limit; i < count; ++i) {
    sum += get_double ((sort_data + i*2));
    ++ k;
  }
  
  /* DBG (7, "bubble_sort: %d values for average\n", k); */
  
  if (k > 0) /* if avg to compute */
    return (uint16_t) (sum / k);
  else
    return (uint16_t) (sum); /* always zero? */
}

static SANE_Status
add_color_mode (Avision_Device* dev, color_mode mode, SANE_String name)
{
  int i;
  DBG (3, "add_color_mode: %d %s\n", mode, name);
  
  for (i = 0; i < AV_COLOR_MODE_LAST; ++i)
    {
      if (dev->color_list [i] == 0) {
	dev->color_list [i] = strdup (name);
	dev->color_list_num [i] = mode;
	return SANE_STATUS_GOOD;
      } else if (strcmp (dev->color_list [i], name) == 0) {
	/* already in list */
	return SANE_STATUS_GOOD;
      }
    }
  
  DBG (3, "add_color_mode: failed\n");
  return SANE_STATUS_NO_MEM;
}

static int
last_color_mode (Avision_Device* dev)
{
  int i = 1;
  
  while (dev->color_list [i] != 0 && i < AV_COLOR_MODE_LAST)
    ++i;
  
  /* we are off by one */
  --i;
  
  return i;
}

static color_mode
match_color_mode (Avision_Device* dev, SANE_String name)
{
  int i;
  DBG (3, "match_color_mode:\n");

  for (i = 0; i < AV_COLOR_MODE_LAST; ++i)
    {
      if (dev->color_list [i] != 0 && strcmp (dev->color_list [i], name) == 0) {
	DBG (3, "match_color_mode: found at %d mode: %d\n",
	     i, dev->color_list_num [i]);
	return dev->color_list_num [i];
      }
    }
  
  DBG (3, "match_color_mode: source mode invalid\n");
  return AV_GRAYSCALE;
}

static SANE_Bool
color_mode_is_shaded (color_mode mode)
{
  return mode >= AV_GRAYSCALE;
}

static SANE_Bool
color_mode_is_color (color_mode mode)
{
  return mode >= AV_TRUECOLOR;
}

static SANE_Bool
is_adf_scan (Avision_Scanner* s)
{
  return s->hw->scanner_type == AV_SHEETFEED || (s->hw->scanner_type == AV_FLATBED && s->source_mode_dim == AV_ADF_DIM);
    
}

static SANE_Status
add_source_mode (Avision_Device* dev, source_mode mode, SANE_String name)
{
  int i;
  
  for (i = 0; i < AV_SOURCE_MODE_LAST; ++i)
    {
      if (dev->source_list [i] == 0) {
        dev->source_list [i] = strdup (name);
        dev->source_list_num [i] = mode;
        return SANE_STATUS_GOOD;
      } else if (strcmp (dev->source_list [i], name) == 0) {
	/* already in list */
	return SANE_STATUS_GOOD;
      }
    }
  
  return SANE_STATUS_NO_MEM;
}

static source_mode
match_source_mode (Avision_Device* dev, SANE_String name)
{
  int i;
  
  DBG (3, "match_source_mode: \"%s\"\n", name);

  for (i = 0; i < AV_SOURCE_MODE_LAST; ++i)
    {
      if (dev->source_list [i] != 0 && strcmp (dev->source_list [i], name) == 0) {
	DBG (3, "match_source_mode: found at %d mode: %d\n",
	     i, dev->source_list_num [i]);
	return dev->source_list_num [i];
      }
    }
  
  DBG (3, "match_source_mode: source mode invalid\n");
  return AV_NORMAL;
}

static source_mode_dim
match_source_mode_dim (source_mode sm)
{
  DBG (3, "match_source_mode_dim: %d\n", sm);
  
  switch (sm) {
  case AV_NORMAL:
    return AV_NORMAL_DIM;
  case AV_TRANSPARENT:
    return AV_TRANSPARENT_DIM;
  case AV_ADF:
  case AV_ADF_REAR:
  case AV_ADF_DUPLEX:
    return AV_ADF_DIM;
  default:
    DBG (3, "match_source_mode_dim: source mode invalid\n");
    return AV_NORMAL_DIM;
  }
}

static int
get_pixel_boundary (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  int boundary;
  
  switch (s->c_mode) {
  case AV_TRUECOLOR:
  case AV_TRUECOLOR12:
  case AV_TRUECOLOR16:
    boundary = dev->inquiry_color_boundary;
    break;
  case AV_GRAYSCALE:
  case AV_GRAYSCALE12:
  case AV_GRAYSCALE16:
    boundary = dev->inquiry_gray_boundary;
    break;
  case AV_DITHERED:
    if (dev->inquiry_asic_type != AV_ASIC_C5)
      boundary = 32;
    else
      boundary = dev->inquiry_dithered_boundary;
    break;
  case AV_THRESHOLDED:
    if (dev->inquiry_asic_type != AV_ASIC_C5)
      boundary = 32;
    else
      boundary = dev->inquiry_thresholded_boundary;
    break;
  default:
    boundary = 8;
  }
  
  return boundary;
}

static SANE_Status
compute_parameters (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;

  int boundary = get_pixel_boundary (s);
  SANE_Bool gray_mode = color_mode_is_shaded (s->c_mode);
  
  /* interlaced duplex (higher end) or flipping paper (HP8xxx)? */
  s->avdimen.interlaced_duplex = s->source_mode == AV_ADF_DUPLEX &&
                                 dev->inquiry_duplex_interlaced;
  
  /* for infra-red we use the same code path es for interlaced
     duplex */
  if (s->val[OPT_IR].w)
    s->avdimen.interlaced_duplex = 1;

#ifdef AVISION_ENHANCED_SANE
  /* quick fix for Microsoft Office Products ... */
  switch (s->c_mode)
  {
    case AV_THRESHOLDED:
    case AV_DITHERED:
       /* our backend already has this restriction - so this line is for
          documentation purposes only */
      boundary = boundary > 32 ? boundary : 32;
      break;
    case AV_GRAYSCALE:
    case AV_GRAYSCALE12:
    case AV_GRAYSCALE16:
      boundary = boundary > 4 ? boundary : 4;
      break;
    case AV_TRUECOLOR:
    case AV_TRUECOLOR12:
    case AV_TRUECOLOR16:
      /* 12 bytes for 24bit color - 48bit is untested w/ Office */
      boundary = boundary > 4 ? boundary : 4;
      break;
  }
#endif
  
  DBG (3, "sane_compute_parameters:\n");
    
  DBG (3, "sane_compute_parameters: boundary %d, gray_mode: %d, \n",
       boundary, gray_mode);
      
  /* TODO: Implement different x/y resolutions support */
  s->avdimen.xres = s->val[OPT_RESOLUTION].w;
  s->avdimen.yres = s->val[OPT_RESOLUTION].w;
  
  /* soft scale ? */
  if (dev->hw->feature_type & AV_SOFT_SCALE) {
    /* find supported hardware resolution */
    const int* hw_res;
    const int* hw_res_list =
      dev->inquiry_asic_type == AV_ASIC_C5 ? hw_res_list_c5 : hw_res_list_generic;
    
    for (hw_res = hw_res_list; *hw_res && *hw_res < s->avdimen.xres; ++hw_res)
      /* just iterate */;
    s->avdimen.hw_xres = *hw_res;

    for (hw_res = hw_res_list; *hw_res && *hw_res < s->avdimen.yres; ++hw_res)
      /* just iterate */;
    s->avdimen.hw_yres = *hw_res;
    
    DBG (3, "sane_compute_parameters: soft scale, hw res: %dx%d\n",
	 s->avdimen.hw_xres,
	 s->avdimen.hw_yres);
    
    if (!s->avdimen.hw_xres || ! s->avdimen.hw_yres) {
      DBG (1, "sane_compute_parameters: no matching HW res for: %dx%d\n",
	   s->avdimen.xres,
	   s->avdimen.yres);
      return SANE_STATUS_INVAL;
    }
  }
  else {
    s->avdimen.hw_xres = s->val[OPT_RESOLUTION].w;
    s->avdimen.hw_yres = s->val[OPT_RESOLUTION].w;
  }
  
  DBG (3, "sane_compute_parameters: tlx: %f, tly: %f, brx: %f, bry: %f\n",
       SANE_UNFIX (s->val[OPT_TL_X].w), SANE_UNFIX (s->val[OPT_TL_Y].w),
       SANE_UNFIX (s->val[OPT_BR_X].w), SANE_UNFIX (s->val[OPT_BR_Y].w));
      
  /* window parameter in pixel */
  s->avdimen.tlx = s->avdimen.hw_xres * SANE_UNFIX (s->val[OPT_TL_X].w)
    / MM_PER_INCH;
  s->avdimen.tly = s->avdimen.hw_yres * SANE_UNFIX (s->val[OPT_TL_Y].w)
    / MM_PER_INCH;
  s->avdimen.brx = s->avdimen.hw_xres * SANE_UNFIX (s->val[OPT_BR_X].w)
    / MM_PER_INCH;
  s->avdimen.bry = s->avdimen.hw_yres * SANE_UNFIX (s->val[OPT_BR_Y].w)
    / MM_PER_INCH;
      
  /* line difference */
  if (color_mode_is_color (s->c_mode) &&
      dev->inquiry_needs_software_colorpack &&
      dev->inquiry_line_difference)
    {
      s->avdimen.line_difference =
	(dev->inquiry_line_difference * s->avdimen.hw_yres) / dev->inquiry_optical_res;
      
      s->avdimen.bry += 2 * s->avdimen.line_difference;
	
      /* limit bry + line_difference to real scan boundary */
      {
	long y_max = dev->inquiry_y_ranges[s->source_mode_dim] *
	  s->avdimen.hw_yres / MM_PER_INCH;
	DBG (3, "sane_compute_parameters: y_max: %ld, bry: %ld, line_difference: %d\n",
	     y_max, s->avdimen.bry, s->avdimen.line_difference);
	    
	if (s->avdimen.bry + 2 * s->avdimen.line_difference > y_max) {
	  DBG (1, "sane_compute_parameters: bry limited!\n");
	  s->avdimen.bry = y_max - 2 * s->avdimen.line_difference;
	}
      }
	  
    } /* end if needs software colorpack */
  else {
    s->avdimen.line_difference = 0;
  }
  
  /* add overscan */
  if (dev->inquiry_tune_scan_length && is_adf_scan (s)) {
    /* some extra effort for precise rounding ... */
    int overscan = (s->avdimen.hw_yres *
		    (SANE_UNFIX (s->val[OPT_OVERSCAN_TOP].w) +
		     SANE_UNFIX (s->val[OPT_OVERSCAN_BOTTOM].w)) + (MM_PER_INCH - 1)
		    ) / MM_PER_INCH;
    DBG (3, "sane_compute_parameters: overscan lines: %d\n", overscan);
    s->avdimen.bry += overscan;
  }
  
  /* rear offset compensation */
  if (s->avdimen.interlaced_duplex && dev->hw->feature_type & AV_REAR_OFFSET) {
    const double offset = 0.5; /* in current affected models 1/2 inch */
    s->avdimen.rear_offset = (int) (offset * s->avdimen.hw_yres);
    DBG (1, "sane_compute_parameters: rear_offset: %d!\n", s->avdimen.rear_offset);
    /* we do not limit against the bottom-y here, as rear offset always
       applies to ADF scans, only */
  }
  else {
    s->avdimen.rear_offset = 0;
  }

  memset (&s->params, 0, sizeof (s->params));
      
  s->avdimen.hw_pixels_per_line = (s->avdimen.brx - s->avdimen.tlx);
  s->avdimen.hw_pixels_per_line -= s->avdimen.hw_pixels_per_line % boundary;
  
  s->avdimen.hw_lines = (s->avdimen.bry - s->avdimen.tly -
			 2 * s->avdimen.line_difference);
  
  if (s->avdimen.interlaced_duplex && dev->scanner_type != AV_FILM)
    s->avdimen.hw_lines -= s->avdimen.hw_lines % dev->read_stripe_size;
  
  s->params.pixels_per_line = s->avdimen.hw_pixels_per_line * s->avdimen.xres / s->avdimen.hw_xres;
  s->params.lines = s->avdimen.hw_lines * s->avdimen.xres / s->avdimen.hw_xres;
  if (is_adf_scan (s))
    /* we can't know how many lines we'll see with an ADF because that depends on the paper length */
    s->params.lines = -1;
  if (s->c_mode == AV_THRESHOLDED || s->c_mode == AV_DITHERED)
    s->params.pixels_per_line -= s->params.pixels_per_line % 8;
  
  debug_print_avdimen (1, "sane_compute_parameters", &s->avdimen);
  
  switch (s->c_mode)
    {
    case AV_THRESHOLDED:
      s->params.format = SANE_FRAME_GRAY;
      s->avdimen.hw_bytes_per_line = s->avdimen.hw_pixels_per_line / 8;
      s->params.bytes_per_line = s->params.pixels_per_line / 8;
      s->params.depth = 1;
      break;
    case AV_DITHERED:
      s->params.format = SANE_FRAME_GRAY;
      s->avdimen.hw_bytes_per_line = s->avdimen.hw_pixels_per_line / 8;
      s->params.bytes_per_line = s->params.pixels_per_line / 8;
      s->params.depth = 1;
      break;
    case AV_GRAYSCALE:
      s->params.format = SANE_FRAME_GRAY;
      s->avdimen.hw_bytes_per_line = s->avdimen.hw_pixels_per_line;
      s->params.bytes_per_line = s->params.pixels_per_line;
      s->params.depth = 8;
      break;
    case AV_GRAYSCALE12:
    case AV_GRAYSCALE16:
      s->params.format = SANE_FRAME_GRAY;
      s->avdimen.hw_bytes_per_line = s->avdimen.hw_pixels_per_line * 2;
      s->params.bytes_per_line = s->params.pixels_per_line * 2;
      s->params.depth = 16;
      break;
    case AV_TRUECOLOR:
      s->params.format = SANE_FRAME_RGB;
      s->avdimen.hw_bytes_per_line = s->avdimen.hw_pixels_per_line * 3;
      s->params.bytes_per_line = s->params.pixels_per_line * 3;
      s->params.depth = 8;
      break;
    case AV_TRUECOLOR12:
    case AV_TRUECOLOR16:
      s->params.format = SANE_FRAME_RGB;
      s->avdimen.hw_bytes_per_line = s->avdimen.hw_pixels_per_line * 3 * 2;
      s->params.bytes_per_line = s->params.pixels_per_line * 3 * 2;
      s->params.depth = 16;
      break;
    default:
      DBG (1, "Invalid mode. %d\n", s->c_mode);
      return SANE_STATUS_INVAL;
    } /* end switch */
  
  s->params.last_frame = SANE_TRUE;
  
  debug_print_params (1, "sane_compute_parameters", &s->params);
  return SANE_STATUS_GOOD;
}

static SANE_Status
inquiry (Avision_Connection av_con, uint8_t* data, size_t len)
{
  SANE_Status status;
  command_header inquiry;
  int try = 2;
  
  DBG (3, "inquiry: length: %ld\n", (long)len);
 
  memset (&inquiry, 0, sizeof(inquiry));
  inquiry.opc = AVISION_SCSI_INQUIRY;
  inquiry.len = len;
  
  do {
    size_t size = inquiry.len;
    
    DBG (3, "inquiry: inquiring ...\n");
    status = avision_cmd (&av_con, &inquiry, sizeof (inquiry), 0, 0,
			  data, &size);
    if (status == SANE_STATUS_GOOD && size == inquiry.len)
      break;
    
    DBG (1, "inquiry: inquiry failed (%s)\n", sane_strstatus (status));
    --try;
  } while (try > 0);
  
  return status;
}

static SANE_Status
wait_ready (Avision_Connection* av_con, int delay)
{
  SANE_Status status;
  int try;
  
  for (try = 0; try < 10; ++ try)
    {
      DBG (3, "wait_ready: sending TEST_UNIT_READY\n");
      status = avision_cmd (av_con, test_unit_ready, sizeof (test_unit_ready),
			    0, 0, 0, 0);
      sleep (delay);
      
      switch (status)
	{
	default:
	  /* Ignore errors while waiting for scanner to become ready.
	     Some SCSI drivers return EIO while the scanner is
	     returning to the home position.  */
	  DBG (1, "wait_ready: test unit ready failed (%s)\n",
	       sane_strstatus (status));
	  /* fall through */
	case SANE_STATUS_DEVICE_BUSY:
	  break;
	case SANE_STATUS_GOOD:
	  return status;
	}
    }
  DBG (1, "wait_ready: timed out after %d attempts\n", try);
  return SANE_STATUS_INVAL;
}

static SANE_Status
wait_4_light (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  
  /* read stuff */
  struct command_read rcmd;
  char* light_status[] =
    { "off", "on", "warming up", "needs warm up test", 
      "light check error", "RESERVED" };
  
  SANE_Status status;
  uint8_t result;
  int try;
  size_t size = 1;
  
  DBG (3, "wait_4_light: getting light status.\n");
  
  memset (&rcmd, 0, sizeof (rcmd));
  
  rcmd.opc = AVISION_SCSI_READ;
  rcmd.datatypecode = 0xa0; /* get light status */
  set_double (rcmd.datatypequal, dev->data_dq);
  set_triple (rcmd.transferlen, size);
  
  for (try = 0; try < 90; ++ try) {
    
    DBG (5, "wait_4_light: read bytes %lu\n", (u_long) size);
    status = avision_cmd (&s->av_con, &rcmd, sizeof (rcmd), 0, 0, &result, &size);
    
    if (status != SANE_STATUS_GOOD || size != sizeof (result)) {
      DBG (1, "wait_4_light: read failed (%s)\n", sane_strstatus (status));
      return status;
    }
    
    DBG (3, "wait_4_light: command is %d. Result is %s\n",
	 status, light_status[(result>4)?5:result]);
    
    if (result == 1) {
      return SANE_STATUS_GOOD;
    }
    else if (dev->hw->feature_type & AV_LIGHT_CHECK_BOGUS) {
      DBG (3, "wait_4_light: scanner marked as returning bogus values in device-list!!\n");
      return SANE_STATUS_GOOD;
    }
    else {
      struct command_send scmd;
      uint8_t light_on = 1;
      
      /* turn on the light */
      DBG (3, "wait_4_light: setting light status.\n");
      
      memset (&scmd, 0, sizeof (scmd));
      
      scmd.opc = AVISION_SCSI_SEND;
      scmd.datatypecode = 0xa0; /* send light status */
      set_double (scmd.datatypequal, dev->data_dq);
      set_triple (scmd.transferlen, size);
      
      status = avision_cmd (&s->av_con, &scmd, sizeof (scmd),
			    &light_on, sizeof (light_on), 0, 0);
      
      if (status != SANE_STATUS_GOOD) {
	DBG (1, "wait_4_light: send failed (%s)\n", sane_strstatus (status));
	return status;
      }
    }
    sleep (1);
  }
  
  DBG (1, "wait_4_light: timed out after %d attempts\n", try);
  return SANE_STATUS_DEVICE_BUSY;
}

static SANE_Status
set_power_save_time (Avision_Scanner* s, int time)
{
  struct {
    struct command_send cmd;
    uint8_t time[2];
  } scmd;

  Avision_Device* dev = s->hw;
  SANE_Status status;

  DBG (3, "set_power_save_time: time %d\n", time);

  memset (&scmd, 0, sizeof (scmd));
  scmd.cmd.opc = AVISION_SCSI_SEND;
  scmd.cmd.datatypecode = 0xA2; /* power-saving timer */
  set_double (scmd.cmd.datatypequal, dev->data_dq);
  set_triple (scmd.cmd.transferlen, sizeof (scmd.time) );

  set_double (scmd.time, time);

  status = avision_cmd (&s->av_con, &scmd.cmd, sizeof (scmd.cmd),
                        &scmd.time, sizeof (scmd.time), 0, 0);
  if (status != SANE_STATUS_GOOD)
    DBG (1, "set_power_save_time: send_data (%s)\n", sane_strstatus (status));
  return status;
}

static SANE_Status
get_firmware_status (Avision_Connection* av_con)
{
  /* read stuff */
  struct command_read rcmd;
  size_t size;
  SANE_Status status;

  firmware_status result;
  
  DBG (3, "get_firmware_status\n");
 
  size = sizeof (result);
 
  memset (&rcmd, 0, sizeof (rcmd));
  rcmd.opc = AVISION_SCSI_READ;
 
  rcmd.datatypecode = 0x90; /* firmware status */
  set_double (rcmd.datatypequal, 0); /* dev->data_dq not available */
  set_triple (rcmd.transferlen, size);
 
  status = avision_cmd (av_con, &rcmd, sizeof (rcmd), 0, 0, &result, &size);
  if (status != SANE_STATUS_GOOD || size != sizeof (result)) {
    DBG (1, "get_firmware_status: read failed (%s)\n",
	 sane_strstatus (status));
    return (status);
  }
 
  debug_print_raw (6, "get_firmware_status: raw data:\n", (uint8_t*)&result, size);
 
  DBG (3, "get_firmware_status: [0]  needs firmware %x\n", result.download_firmware);
  DBG (3, "get_firmware_status: [1]  side edge: %d\n", get_double ( result.first_effective_pixel_flatbed ));
  DBG (3, "get_firmware_status: [3]  side edge: %d\n", get_double ( result.first_effective_pixel_adf_front ));
  DBG (3, "get_firmware_status: [5]  side edge: %d\n", get_double ( result.first_effective_pixel_adf_rear ));

  return SANE_STATUS_GOOD;
}

static SANE_Status
get_flash_ram_info (Avision_Connection* av_con)
{
  /* read stuff */
  struct command_read rcmd;
  size_t size;
  SANE_Status status;
  uint8_t result[40];
  
  DBG (3, "get_flash_ram_info\n");
 
  size = sizeof (result);
 
  memset (&rcmd, 0, sizeof (rcmd));
  rcmd.opc = AVISION_SCSI_READ;
 
  rcmd.datatypecode = 0x6a; /* flash ram information */
  set_double (rcmd.datatypequal, 0); /* dev->data_dq not available */
  set_triple (rcmd.transferlen, size);
 
  status = avision_cmd (av_con, &rcmd, sizeof (rcmd), 0, 0, result, &size);
  if (status != SANE_STATUS_GOOD || size != sizeof (result)) {
    DBG (1, "get_flash_ram_info: read failed (%s)\n",
	 sane_strstatus (status));
    return (status);
  }
 
  debug_print_raw (6, "get_flash_ram_info: raw data:\n", result, size);
  
  DBG (3, "get_flash_ram_info: [0]    data type %x\n", result [0]);  
  DBG (3, "get_flash_ram_info: [1]    Ability1:%s%s%s%s%s%s%s%s\n",
       BIT(result[1],7)?" RESERVED_BIT7":"",
       BIT(result[1],6)?" RESERVED_BIT6":"",
       BIT(result[1],5)?" FONT(r/w)":"",
       BIT(result[1],4)?" FPGA(w)":"",
       BIT(result[1],3)?" FMDBG(r)":"",
       BIT(result[1],2)?" RAWLINE(r)":"",
       BIT(result[1],1)?" FIRMWARE(r/w)":"",
       BIT(result[1],0)?" CTAB(r/w)":"");
  
  DBG (3, "get_flash_ram_info: [2-5]   size CTAB: %d\n",
       get_quad ( &(result[2]) ) );

  DBG (3, "get_flash_ram_info: [6-9]   size FIRMWARE: %d\n",
       get_quad ( &(result[6]) ) );

  DBG (3, "get_flash_ram_info: [10-13] size RAWLINE: %d\n",
       get_quad ( &(result[10]) ) );

  DBG (3, "get_flash_ram_info: [14-17] size FMDBG: %d\n",
       get_quad ( &(result[14]) ) );

  DBG (3, "get_flash_ram_info: [18-21] size FPGA: %d\n",
       get_quad ( &(result[18]) ) );

  DBG (3, "get_flash_ram_info: [22-25] size FONT: %d\n",
       get_quad ( &(result[22]) ) );

  DBG (3, "get_flash_ram_info: [26-29] size RESERVED: %d\n",
       get_quad ( &(result[26]) ) );

  DBG (3, "get_flash_ram_info: [30-33] size RESERVED: %d\n",
       get_quad ( &(result[30]) ) );
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
get_nvram_data (Avision_Scanner* s, nvram_data* nvram)
{
  /* read stuff */
  struct command_send rcmd;
  
  size_t size;
  SANE_Status status;
  
  DBG (3, "get_nvram_data\n");
  
  size = sizeof (*nvram);
  
  memset (&rcmd, 0, sizeof (rcmd));
  memset (nvram, 0, size);
  
  rcmd.opc = AVISION_SCSI_READ;
  
  rcmd.datatypecode = 0x69; /* Read NVM RAM data */
  set_double (rcmd.datatypequal, 0); /* dev->data_dq not available */
  set_triple (rcmd.transferlen, size);
 
  status = avision_cmd (&s->av_con, &rcmd, sizeof (rcmd), 0, 0,
			nvram, &size);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "get_nvram_data: read failed (%s)\n",
	 sane_strstatus (status));
    return (status);
  }
  
  debug_print_nvram_data (5, "get_nvram_data", nvram);
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
get_and_parse_nvram (Avision_Scanner* s, char* str, int n)
{
  SANE_Status status;
  int i = 0;
  int x;
  nvram_data nvram;
  uint8_t inquiry_result [AVISION_INQUIRY_SIZE_V1];
  
  status = inquiry (s->av_con, inquiry_result, sizeof(inquiry_result));
  if (status == SANE_STATUS_GOOD) {
    i += snprintf (str+i, n-i, "Vendor: %.8s",
		   inquiry_result+8);
    i += snprintf (str+i, n-i, "\nModel: %.16s",
		   inquiry_result+16);
    i += snprintf (str+i, n-i, "\nFirmware: %.4s",
		   inquiry_result+32);
  }
  
  if (!s->hw->inquiry_nvram_read)
    return SANE_STATUS_GOOD;
  
  status = get_nvram_data (s, &nvram);
  if (status == SANE_STATUS_GOOD)
    {
      if (nvram.serial[0])
	i += snprintf (str+i, n-i, "\nSerial: %.24s",
		       nvram.serial);
      
      if (nvram.born_year)
	i += snprintf (str+i, n-i, "\nManufacturing date: %d-%d-%d",
		       get_double(nvram.born_year),
		       get_double(nvram.born_month),
		       get_double(nvram.born_day));
      if (nvram.first_scan_year)
	i += snprintf (str+i, n-i, "\nFirst scan date: %d-%d-%d",
		       get_double(nvram.first_scan_year),
		       get_double(nvram.first_scan_month),
		       get_double(nvram.first_scan_day));
      
      x = get_quad (nvram.flatbed_scans);
      if (x)
	i += snprintf (str+i, n-i, "\nFlatbed scans: %d", x);
      x = get_quad (nvram.pad_scans);
      if (x)
	i += snprintf (str+i, n-i, "\nPad scans: %d", x);
      x = get_quad (nvram.adf_simplex_scans);
      if (x)
	i += snprintf (str+i, n-i, "\nADF simplex scans: %d", x);
      x = get_quad (nvram.adf_duplex_scans);
      if (x)
	i += snprintf (str+i, n-i, "\nADF duplex scans: %d", x);
    }
  
  return status;
}

static SANE_Status
get_power_save_time (Avision_Scanner* s, SANE_Word* time)
{
  SANE_Status status;
  nvram_data nvram;
  
  DBG (3, "get_power_save_time\n");
  
  if (!s->hw->inquiry_nvram_read)
    return SANE_STATUS_INVAL;
  
  status = get_nvram_data (s, &nvram);
  
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "get_power_save_time: read nvram failed (%s)\n", sane_strstatus (status));
    return status;
  }
  
  *time = get_double (nvram.power_saving_time);
  
  return SANE_STATUS_GOOD;
}

#ifdef NEEDED

static SANE_Status
send_nvram_data (Avision_Connection* av_con)
{
  /* read stuff */
  struct command_send scmd;
  size_t size;
  SANE_Status status;
  
  DBG (3, "send_nvram_data\n");
 
  size = sizeof (c7_nvram);
 
  memset (&scmd, 0, sizeof (scmd));
  scmd.opc = AVISION_SCSI_SEND;
  
  scmd.datatypecode = 0x85; /* nvram data */
  set_double (scmd.datatypequal, 0); /* dev->data_dq not available */
  set_triple (scmd.transferlen, size);
 
  status = avision_cmd (av_con, &scmd, sizeof (scmd), &c7_nvram, size,
			0, 0);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "send_nvram_data: send failed (%s)\n",
	 sane_strstatus (status));
    return (status);
  }
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
send_flash_ram_data (Avision_Connection* av_con)
{
  /* read stuff */
  struct command_send scmd;
  size_t size;
  SANE_Status status;
  
  DBG (3, "send_flash_ram_data\n");
 
  size = sizeof (c7_flash_ram);
 
  memset (&scmd, 0, sizeof (scmd));
  scmd.opc = AVISION_SCSI_SEND;
  
  scmd.datatypecode = 0x86; /* flash data */
  set_double (scmd.datatypequal, 0);
  set_triple (scmd.transferlen, size);
 
  status = avision_cmd (av_con, &scmd, sizeof (scmd), &c7_flash_ram, size,
			0, 0);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "send_flash_ram_data: send failed (%s)\n",
	 sane_strstatus (status));
    return (status);
  }
  
  return SANE_STATUS_GOOD;
}
#endif


static SANE_Status
adf_reset (Avision_Scanner* s)
{
  SANE_Status status;
  Avision_Device* dev = s->hw;
  struct command_send scmd;
  struct command_read rcmd;
  uint8_t payload[4];
  size_t size;
  size_t n;
  int i;
  DBG (3, "adf_reset\n");

  /* loop twice */
  for (i=1; i >= 0; i--) {
    n=i;
    memset (&scmd, 0, sizeof (scmd));
    memset (&payload, 0, sizeof (payload));
    scmd.opc = AVISION_SCSI_SEND;
    scmd.datatypecode = 0xD0; /* unknown */
    set_double (scmd.datatypequal, 0);
    size = 2;
    set_triple (scmd.transferlen, size);
    payload[1] = 0x10 * i;  /* write 0x10 the first time, 0x00 the second */
    status = avision_cmd (&s->av_con, &scmd, sizeof (scmd), payload, size, 0, 0);
    if (status != SANE_STATUS_GOOD) {
      DBG (1, "adf_reset: write %d failed (%s)\n", (2-i),
	 sane_strstatus (status));
      return (status);
    }
    DBG (3, "adf_reset: write %d complete.\n", (2-i));

    memset (&rcmd, 0, sizeof (rcmd));
    memset (&payload, 0, sizeof (payload));
    rcmd.opc = AVISION_SCSI_READ;
    rcmd.datatypecode = 0x69; /* Read NVRAM data */
    set_double (rcmd.datatypequal, dev->data_dq);
    size = 4 - i; /* read 3 bytes the first time, 4 the second */
    set_triple (rcmd.transferlen, size);
    status = avision_cmd (&s->av_con, &rcmd, sizeof (rcmd), 0, 0, payload, &size);
    if (status != SANE_STATUS_GOOD || size != (4-n)) {
      DBG (1, "adf_reset: read %lu failed (%s)\n", (2-n),
	 sane_strstatus (status));
      return (status);
    }
    debug_print_raw (3, "adf_reset: raw data:\n", payload, size);
  }
  return SANE_STATUS_GOOD;
}


static SANE_Status
get_accessories_info (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  int try = 3;
  
  /* read stuff */
  struct command_read rcmd;
  size_t size;
  SANE_Status status;
  uint8_t result[8];
  
  char* adf_model[] =
    { "Origami", "Oodles", "HP9930", "unknown" };
  const int adf_models = sizeof (adf_model) / sizeof(char*) - 1;
  
  DBG (3, "get_accessories_info\n");
  
  size = sizeof (result);
 
  memset (&rcmd, 0, sizeof (rcmd));
  rcmd.opc = AVISION_SCSI_READ;
 
  rcmd.datatypecode = 0x64; /* detect accessories */
  set_double (rcmd.datatypequal, dev->data_dq);
  set_triple (rcmd.transferlen, size);
 
  /* after resetting the ADF unit, try reprobing it again */
  RETRY:

  status = avision_cmd (&s->av_con, &rcmd, sizeof (rcmd), 0, 0, result, &size);
  if (status != SANE_STATUS_GOOD || size != sizeof (result)) {
    DBG (1, "get_accessories_info: read failed (%s)\n",
         sane_strstatus (status));
    return (status);
  }

  debug_print_raw (6, "get_accessories_info: raw data:\n", result, size);

  DBG (3, "get_accessories_info: [0]  ADF: %x\n", result[0]);
  DBG (3, "get_accessories_info: [1]  Light Box: %x\n", result[1]);

  DBG (3, "get_accessories_info: [2]  ADF model: %d (%s)\n",
       result [2],
       adf_model[ (result[2] < adf_models) ? result[2] : adf_models ]);

  dev->inquiry_adf |= result [0];

  if (dev->hw->feature_type & AV_ADF_FLIPPING_DUPLEX)
  {
    if (result[0] == 1)
    {
      dev->inquiry_duplex = 1;
      dev->inquiry_duplex_interlaced = 0;
    } else if (result[0] == 0 && result[2] != 0) {
      /* Sometimes the scanner will report that there is no ADF attached, yet
       * an ADF model number will still be reported.  This happens on the
       * HP8200 series and possibly others.  In this case we need to reset the
       * the adf and try reading it again.
       */
      DBG (3, "get_accessories_info: Found ADF model number but the ADF-present flag is not set. Trying to recover...\n");
      status = adf_reset (s);
      if (status != SANE_STATUS_GOOD) {
        DBG (3, "get_accessories_info: Failed to reset ADF: %s\n", sane_strstatus (status));
        return status;
      }
      DBG (1, "get_accessories_info: Waiting while ADF firmware resets...\n");
      sleep(3);
      status = wait_ready (&s->av_con, 1);
      if (status != SANE_STATUS_GOOD) {
        DBG (1, "get_accessories_info: wait_ready() failed: %s\n", sane_strstatus (status));
        return status;
      }
      if (try) {
        try--;
        goto RETRY;
      }
      DBG (1, "get_accessories_info: Maximum retries attempted, ADF unresponsive.\n");
      return SANE_STATUS_UNSUPPORTED;
    }
  }

  /* only honor a 1, some scanner without adapter set 0xff */
  if (result[1] == 1)
    dev->inquiry_light_box = 1;

  return SANE_STATUS_GOOD;
}


/* Returns a pointer to static char* strings or NULL for cancel (we do
   not want to start memcmp'ing for the cancel case). */
static const char*
string_for_button (Avision_Scanner* s, int button)
{
  static char buffer [16];
  Avision_Device* dev = s->hw;

  /* dev->sane.model
     dev->inquiry_asic_type */

  if (dev->inquiry_buttons == 1)
    goto return_scan;

  /* simplex / duplex buttons */
  if (strcmp (dev->sane.vendor, "Xerox") == 0 ||
      strcmp (dev->sane.vendor, "Visioneer") == 0 ||
      strcmp (dev->sane.model, "AV121") == 0 ||
      strcmp (dev->sane.model, "AV122") == 0
      )
    {
      switch (button)
	{
	case 1: return "simplex";
	case 2: return "duplex";
	}
    }
  
  if (strcmp (dev->sane.model, "AV210C2") == 0 ||
      strcmp (dev->sane.model, "AV210D2+") == 0 ||
      strcmp (dev->sane.model, "AV220C2") == 0 ||
      strcmp (dev->sane.model, "AV610C2") == 0
      )
    {
      if (button == 1)
	return NULL; /* cancel */
      else
	goto return_scan;
    }
  
  /* those are unique, right now */
  if (strcmp (dev->sane.model, "AV610") == 0)
    {
      switch (button)
	{
	case 0: return "email";
	case 1: return "copy";
	case 2: return "scan";
	}
    }
  
  /* last resort */
  snprintf (buffer, sizeof (buffer), "button%d", button);
  return buffer;
  
 return_scan:
  return "scan";
}

static SANE_Status
get_button_status (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  
  /* read stuff */
  struct command_read rcmd;
  size_t size;
  SANE_Status status;
  /* was only 6 in an old SPEC - maybe we need a feature override :-( -ReneR */
  struct {
     uint8_t press_state;
     uint8_t buttons[5];
     uint8_t display; /* AV220 et.al. 7 segment LED display */
     uint8_t reserved[9];
  } result;

  unsigned int i;
  
  DBG (3, "get_button_status:\n");
  
  size = sizeof (result);
  
  /* AV220 et.al. */
  if (! (dev->hw->feature_type & AV_INT_BUTTON))
    {
      memset (&rcmd, 0, sizeof (rcmd));
      rcmd.opc = AVISION_SCSI_READ;
      
      rcmd.datatypecode = 0xA1; /* button status */
      set_double (rcmd.datatypequal, dev->data_dq);
      set_triple (rcmd.transferlen, size);
      
      status = avision_cmd (&s->av_con, &rcmd, sizeof (rcmd), 0, 0,
			    (uint8_t*)&result, &size);
      if (status != SANE_STATUS_GOOD || size != sizeof (result)) {
	DBG (1, "get_button_status: read failed (%s)\n", sane_strstatus (status));
	return status;
      }
    }
  else
    {
      /* only try to read the first 8 bytes ...*/
      size = 8;
      
      /* no SCSI equivalent */
      /* either there was a button press and this completes quickly
         or there is no point waiting for a future press */
      sanei_usb_set_timeout (100); /* 10th of a second */
      DBG (5, "==> (interrupt read) going down ...\n");
      status = sanei_usb_read_int (s->av_con.usb_dn, (uint8_t*)&result,
				   &size);
      DBG (5, "==> (interrupt read) got: %ld\n", (long)size);
      
      if (status != SANE_STATUS_GOOD) {
	DBG (1, "get_button_status: interrupt read failed (%s)\n",
	     sane_strstatus (status));
	return SANE_STATUS_GOOD;
      }
      
      if (size < sizeof (result))
	memset ((char*)result.buttons + size, 0, sizeof (result) - size);
      
      /* hack to fill in meaningful values for the AV 210 / 610 and
	 under some conditions the AV 220 */
      if (size == 1) { /* AV 210, AV 610 */
	DBG (1, "get_button_status: just one byte, filling the rest\n");
	
	if (result.press_state > 0) {
	  debug_print_raw (6, "get_button_status: raw data\n",
			   (uint8_t*)&result, size);
	  result.buttons[0] = result.press_state;
	  result.press_state = 0x80 | 1;
	  size = 2;
	}
	else /* nothing pressed */
	  return SANE_STATUS_GOOD;
      }
      else if (size >= 8 && result.press_state == 0) { /* AV 220 */
	
	debug_print_raw (6, "get_button_status: raw data\n",
		   (uint8_t*)&result, size);
	
	DBG (1, "get_button_status: zero buttons  - filling values ...\n");
	
	/* simulate button press of the last button ... */
	result.press_state = 0x80 | 1;
	result.buttons[0] = dev->inquiry_buttons; /* 1 based */
      }
    }
  
  debug_print_raw (6, "get_button_status: raw data\n",
		   (uint8_t*)&result, size);
  
  DBG (3, "get_button_status: [0]  Button status: %x\n", result.press_state);
  for (i = 0; i < 5; ++i)
    DBG (3, "get_button_status: [%d]  Button number %d: %x\n", i+1, i,
	 result.buttons[i]);
  DBG (3, "get_button_status: [7]  Display: %d\n", result.display);
  
  {
    char* message_begin = s->val[OPT_MESSAGE].s;
    char* message_end = s->val[OPT_MESSAGE].s + s->opt[OPT_MESSAGE].size;
    char* message = message_begin;
    
#define add_token(format,value) do {				     \
      int n = snprintf (message, message_end - message, "%s" format, \
                        message == message_begin ? "" : ":", value); \
      message += n > 0 ? n : 0;					     \
    } while (0)
    
    if (result.display > 0)
      add_token ("%d", result.display);
    
    if (result.press_state >> 7) /* AV220 et.al. bit 6 is long/short press? */
      {
      
	const unsigned int buttons_pressed = result.press_state & 0x7F;
	DBG (3, "get_button_status: %d button(s) pressed\n", buttons_pressed);
            
	/* reset the hardware button status */
	if (! (dev->hw->feature_type & AV_INT_BUTTON))
	  {
	    struct command_send scmd;
	    uint8_t button_reset = 1;
	  
	    DBG (3, "get_button_status: resetting status\n");
	  
	    memset (&scmd, 0, sizeof (scmd));
	  
	    scmd.opc = AVISION_SCSI_SEND;
	    scmd.datatypecode = 0xA1; /* button control */
	    set_double (scmd.datatypequal, dev->data_dq);
	    set_triple (scmd.transferlen, size);
	  
	    status = avision_cmd (&s->av_con, &scmd, sizeof (scmd),
				  &button_reset, sizeof (button_reset), 0, 0);
	  
	    if (status != SANE_STATUS_GOOD) {
	      DBG (1, "get_button_status: send failed (%s)\n",
		   sane_strstatus (status));
	      return status;
	    }
	  }
	
	for (i = 0; i < buttons_pressed; ++i) {
	  const unsigned int button = result.buttons[i] - 1; /* 1 based ... */
	  DBG (3, "get_button_status: button %d pressed\n", button);
	  if (button >= dev->inquiry_buttons) {
	    DBG (1, "get_button_status: button %d not allocated as not indicated in inquiry\n",
		 button);
	  }
	  else {
	    const char* label = string_for_button (s, button);
	    if (label)
	      add_token ("%s", label);
	    else
	      return SANE_STATUS_CANCELLED;
	  }
	}
      }
    else
      DBG (3, "get_button_status: no button pressed\n");
  }
  
  return SANE_STATUS_GOOD;
#undef add_token
}

static SANE_Status
get_frame_info (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  
  /* read stuff */
  struct command_read rcmd;
  size_t size;
  SANE_Status status;
  uint8_t result[8];
  size_t i;
  
  DBG (3, "get_frame_info:\n");
  
  size = sizeof (result);
 
  memset (&rcmd, 0, sizeof (rcmd));
  rcmd.opc = AVISION_SCSI_READ;
 
  rcmd.datatypecode = 0x87; /* film holder sense */
  set_double (rcmd.datatypequal, dev->data_dq);
  set_triple (rcmd.transferlen, size);
 
  status = avision_cmd (&s->av_con, &rcmd, sizeof (rcmd), 0, 0, result, &size);
  if (status != SANE_STATUS_GOOD || size != sizeof (result)) {
    DBG (1, "get_frame_info: read failed (%s)\n", sane_strstatus (status));
    return (status);
  }
  
  debug_print_raw (6, "get_frame_info: raw data\n", result, size);
  
  DBG (3, "get_frame_info: [0]  Holder type: %s\n",
       (result[0]==1)?"APS":
       (result[0]==2)?"Film holder (35mm)":
       (result[0]==3)?"Slide holder":
       (result[0]==0xff)?"Empty":"unknown");
  DBG (3, "get_frame_info: [1]  Current frame number: %d\n", result[1]);
  DBG (3, "get_frame_info: [2]  Frame amount: %d\n", result[2]);
  DBG (3, "get_frame_info: [3]  Mode: %s\n", BIT(result[3],4)?"APS":"Not APS");
  DBG (3, "get_frame_info: [3]  Exposures (if APS): %s\n", 
       ((i=(BIT(result[3],3)<<1)+BIT(result[2],2))==0)?"Unknown":
       (i==1)?"15":(i==2)?"25":"40");
  DBG (3, "get_frame_info: [3]  Film Type (if APS): %s\n", 
       ((i=(BIT(result[1],3)<<1)+BIT(result[0],2))==0)?"Unknown":
       (i==1)?"B&W Negative":(i==2)?"Color slide":"Color Negative");

  dev->holder_type = result[0];
  dev->current_frame = result[1];
 
  dev->frame_range.min = 1;
  dev->frame_range.quant = 1;
  
  if (result[0] != 0xff)
    dev->frame_range.max = result[2];
  else
    dev->frame_range.max = 1;
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
get_duplex_info (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  
  /* read stuff */
  struct command_read rcmd;

  struct {
     uint8_t mode;
     uint8_t color_line_difference[2];
     uint8_t gray_line_difference[2];
     uint8_t lineart_line_difference[2];
     uint8_t image_info;
  } result;
  
  size_t size;
  SANE_Status status;
  
  DBG (3, "get_duplex_info:\n");
  
  size = sizeof (result);
 
  memset (&rcmd, 0, sizeof (rcmd));
  rcmd.opc = AVISION_SCSI_READ;
 
  rcmd.datatypecode = 0xB1; /* read duplex info */
  set_double (rcmd.datatypequal, dev->data_dq);
  set_triple (rcmd.transferlen, size);
 
  status = avision_cmd (&s->av_con, &rcmd, sizeof (rcmd), 0, 0,
			&result, &size);
  if (status != SANE_STATUS_GOOD || size != sizeof (result)) {
    DBG (1, "get_duplex_info: read failed (%s)\n", sane_strstatus (status));
    return (status);
  }
  
  debug_print_raw (6, "get_duplex_info: raw data\n", (uint8_t*)&result, size);
  
  DBG (3, "get_duplex_info: [0]    Mode: %s%s\n",
       BIT(result.mode,0)?"MERGED_PAGES":"",
       BIT(result.mode,1)?"2ND_PAGE_FOLLOWS":"");
  DBG (3, "get_duplex_info: [1-2]  Color line difference: %d\n",
       get_double(result.color_line_difference));
  DBG (3, "get_duplex_info: [3-4]  Gray line difference: %d\n",
       get_double(result.gray_line_difference));
  DBG (3, "get_duplex_info: [5-6]  Lineart line difference: %d\n",
       get_double(result.lineart_line_difference));
  /* isn't this supposed to be result.info ?!? */
  DBG (3, "get_duplex_info: [7]    Mode: %s%s%s%s\n",
       BIT(result.image_info,0)?" FLATBED_BGR":" FLATBED_RGB",
       BIT(result.image_info,1)?" ADF_BGR":" ADF_RGB",
       BIT(result.image_info,2)?" FLATBED_NEEDS_MIRROR_IMAGE":"",
       BIT(result.image_info,3)?" ADF_NEEDS_MIRROR_IMAGE":"");
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
set_frame (Avision_Scanner* s, SANE_Word frame)
{
  struct {
    struct command_send cmd;
    uint8_t data[8];
  } scmd;
  
  Avision_Device* dev = s->hw;
  SANE_Status status;
  
  DBG (3, "set_frame: request frame %d\n", frame);
  
  /* Better check the current status of the film holder, because it
     can be changed between scans. */
  status = get_frame_info (s);
  if (status != SANE_STATUS_GOOD)
    return status;
  
  /* No film holder? */
  if (dev->holder_type == 0xff) {
    DBG (1, "set_frame: No film holder!!\n");
    return SANE_STATUS_INVAL;
  }
  
  /* Requesting frame 0xff indicates eject/rewind */
  if (frame != 0xff && (frame < 1 || frame > dev->frame_range.max) ) {
    DBG (1, "set_frame: Illegal frame (%d) requested (min=1, max=%d)\n",
	 frame, dev->frame_range.max); 
    return SANE_STATUS_INVAL;
  }
  
  memset (&scmd, 0, sizeof (scmd));
  scmd.cmd.opc = AVISION_SCSI_SEND;
  scmd.cmd.datatypecode = 0x87; /* send film holder "sense" */
  set_double (scmd.cmd.datatypequal, dev->data_dq);
  set_triple (scmd.cmd.transferlen, sizeof (scmd.data) );
  
  scmd.data[0] = dev->holder_type;
  scmd.data[1] = frame; 
  
  status = avision_cmd (&s->av_con, &scmd.cmd, sizeof (scmd.cmd),
			&scmd.data, sizeof (scmd.data), 0, 0);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "set_frame: send_data (%s)\n", sane_strstatus (status));
  }  

  return status;
}

static SANE_Status
attach (SANE_String_Const devname, Avision_ConnectionType con_type,
        Avision_Device** devp)
{
  uint8_t result [AVISION_INQUIRY_SIZE_MAX];
  int model_num;

  Avision_Device* dev;
  SANE_Status status;
  
  Avision_Connection av_con;

  char mfg [9];
  char model [17];
  char rev [5];
  
  unsigned int i;
  char* s;
  SANE_Bool found;
  
  DBG (3, "attach:\n");
  memset (result, 0, sizeof(result));
  
  for (dev = first_dev; dev; dev = dev->next)
    if (strcmp (dev->sane.name, devname) == 0) {
      if (devp)
	*devp = dev;
      return SANE_STATUS_GOOD;
    }

  av_con.connection_type = con_type;
  if (av_con.connection_type == AV_USB)
    av_con.usb_status = AVISION_USB_UNTESTED_STATUS;
  
  /* set known USB status type */
  if (attaching_hw && attaching_hw->feature_type & AV_INT_STATUS)
    av_con.usb_status = AVISION_USB_INT_STATUS;
  
  DBG (3, "attach: opening %s\n", devname);
  status = avision_open (devname, &av_con, sense_handler, 0);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "attach: open failed (%s)\n", sane_strstatus (status));
    return SANE_STATUS_INVAL;
  }
  
  /* first: get the standard inquiry? */
  status = inquiry (av_con, result, AVISION_INQUIRY_SIZE_V1);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "attach: 1st inquiry failed (%s)\n", sane_strstatus (status));
    return status;
  }
  
  /* copy string information - and zero terminate them c-style */
  memcpy (&mfg, result + 8, 8);
  mfg [8] = 0;
  memcpy (&model, result + 16, 16);
  model [16] = 0;
  memcpy (&rev, result + 32, 4);
  rev [4] = 0;
  
  /* shorten strings (-1 for last index
     -1 for last 0; >0 because one char at least) */
  for (i = sizeof (mfg) - 2; i > 0; i--) {
    if (mfg[i] == 0x20)
      mfg[i] = 0;
    else
      break;
  }
  for (i = sizeof (model) - 2; i > 0; i--) {
    if (model[i] == 0x20)
      model[i] = 0;
    else
      break;
  }
  
  DBG (1, "attach: Inquiry gives mfg=%s, model=%s, product revision=%s.\n",
       mfg, model, rev);
  
  model_num = 0;
  found = 0;
  /* while not at at end of list NULL terminator */
  while (Avision_Device_List[model_num].real_mfg != NULL ||
         Avision_Device_List[model_num].scsi_mfg != NULL)
  {
    int matches = 0, match_count = 0; /* count number of matches */
    DBG (1, "attach: Checking model: %d\n", model_num);
    
    if (Avision_Device_List[model_num].scsi_mfg) {
      ++match_count;
      if (strcmp(mfg, Avision_Device_List[model_num].scsi_mfg) == 0)
        ++matches;
    }
    if (Avision_Device_List[model_num].scsi_model) {
      ++match_count;
      if (strcmp(model, Avision_Device_List[model_num].scsi_model) == 0)
        ++matches;
    }
    
    /* we need 2 matches (mfg, model) for SCSI entries, or the ones available
       for "we know what we are looking for" USB entries */
    if ((attaching_hw == &(Avision_Device_List [model_num]) &&
         matches == match_count) ||
	matches == 2)
    {
      DBG (1, "attach: Scanner matched entry: %d: \"%s\", \"%s\", 0x%.4x, 0x%.4x\n",
           model_num,
	   Avision_Device_List[model_num].scsi_mfg,
	   Avision_Device_List[model_num].scsi_model,
	   Avision_Device_List[model_num].usb_vendor,
	   Avision_Device_List[model_num].usb_product);
      found = 1;
      break;
    }
    ++model_num;
  }
  
  if (!found) {
    DBG (0, "attach: \"%s\" - \"%s\" not yet in whitelist!\n", mfg, model);
    DBG (0, "attach: You might want to report this output.\n");
    DBG (0, "attach: To: rene@exactcode.de (the Avision backend author)\n");
    
    status = SANE_STATUS_INVAL;
    goto close_scanner_and_return;
  }
  
  /* second: maybe ask for the firmware status and flash ram info */
  if (Avision_Device_List [model_num].feature_type & AV_FIRMWARE)
  {
      DBG (3, "attach: reading firmware status\n");
      status = get_firmware_status (&av_con);
      if (status != SANE_STATUS_GOOD) {
	DBG (1, "attach: get firmware status failed (%s)\n",
	     sane_strstatus (status));
	goto close_scanner_and_return;
      }
      
      DBG (3, "attach: reading flash ram info\n");
      status = get_flash_ram_info (&av_con);
      if (status != SANE_STATUS_GOOD) {
	DBG (1, "attach: get flash ram info failed (%s)\n",
	     sane_strstatus (status));
	goto close_scanner_and_return;
      }
      
#ifdef FIRMWARE_DATABASE_INCLUDED
      /* Send new NV-RAM (firmware) data */
      status = send_nvram_data (&av_con);
      if (status != SANE_STATUS_GOOD)
	goto close_scanner_and_return;
#endif
  }
  
  /* third: get the extended Avision inquiry */
  status = inquiry (av_con, result, AVISION_INQUIRY_SIZE_V1);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "attach: avision v1 inquiry failed (%s)\n", sane_strstatus (status));
    goto close_scanner_and_return;
  }

  dev = malloc (sizeof (*dev));
  if (!dev) {
    status = SANE_STATUS_NO_MEM;
    goto close_scanner_and_return;
  }
  
  memset (dev, 0, sizeof (*dev));

  dev->hw = &Avision_Device_List[model_num];
  
  dev->sane.name   = strdup (devname);
  dev->sane.vendor = dev->hw->real_mfg   ? dev->hw->real_mfg   : strdup (mfg);
  dev->sane.model  = dev->hw->real_model ? dev->hw->real_model : strdup (model);
  dev->connection.connection_type = av_con.connection_type;
  dev->connection.usb_status = av_con.usb_status;
  
  /* and finally Avision even extended this one later on
     the AV220C2 does not grok this */
  dev->inquiry_asic_type = (int) result[91];
  if (dev->inquiry_asic_type == AV_ASIC_C6)
    {
      status = inquiry (av_con, result, AVISION_INQUIRY_SIZE_V2);
      if (status != SANE_STATUS_GOOD) {
	DBG (1, "attach: avision v2 inquiry failed (%s)\n", sane_strstatus (status));
	goto close_scanner_and_return;
      }
    }
  
  debug_print_raw (6, "attach: raw data:\n", result, sizeof (result) );
  
  DBG (3, "attach: [8-15]  Vendor id.:      '%8.8s'\n", result+8);
  DBG (3, "attach: [16-31] Product id.:     '%16.16s'\n", result+16);
  DBG (3, "attach: [32-35] Product rev.:    '%4.4s'\n", result+32);
  
  i = (result[36] >> 4) & 0x7;
  switch (result[36] & 0x07) {
  case 0:
    s = " RGB"; break;
  case 1:
    s = " BGR"; break;
  default:
    s = " unknown (RESERVED)";
  }
  DBG (3, "attach: [36]    Bitfield:%s%s%s%s%s%s%s color plane\n",
       BIT(result[36],7)?" ADF":"",
       (i==0)?" B&W only":"",
       BIT(i, 1)?" 3-pass color":"",
       BIT(i, 2)?" 1-pass color":"",
       BIT(i, 2) && BIT(i, 0) ?" 1-pass color (ScanPartner only)":"",
       BIT(result[36],3)?" IS_NOT_FLATBED":"",
       s);

  DBG (3, "attach: [37]    Optical res.:    %d00 dpi\n", result[37]);
  DBG (3, "attach: [38]    Maximum res.:    %d00 dpi\n", result[38]);
  
  DBG (3, "attach: [39]    Bitfield1:%s%s%s%s%s%s\n",
       BIT(result[39],7)?" TRANS":"",
       BIT(result[39],6)?" Q_SCAN":"",
       BIT(result[39],5)?" EXTENDED_RES":"",
       BIT(result[39],4)?" SUPPORTS_CALIB":"",
       BIT(result[39],2)?" NEW_PROTOCOL":"",
       (result[39] & 0x03) == 0x03 ? " AVISION":" OEM");
  
  DBG (3, "attach: [40-41] X res. in gray:  %d dpi\n",
       get_double ( &(result[40]) ));
  DBG (3, "attach: [42-43] Y res. in gray:  %d dpi\n",
       get_double ( &(result[42]) ));
  DBG (3, "attach: [44-45] X res. in color: %d dpi\n",
       get_double ( &(result[44]) ));
  DBG (3, "attach: [46-47] Y res. in color: %d dpi\n",
       get_double ( &(result[46]) ));
  DBG (3, "attach: [48-49] USB max read:    %d\n",
get_double ( &(result[48] ) ));

  DBG (3, "attach: [50]    ESA1:%s%s%s%s%s%s%s%s\n",
       BIT(result[50],7)?" LIGHT_CONTROL":"",
       BIT(result[50],6)?" BUTTON_CONTROL":"",
       BIT(result[50],5)?" NEED_SW_COLORPACK":"",
       BIT(result[50],4)?" SW_CALIB":"",
       BIT(result[50],3)?" NEED_SW_GAMMA":"",
       BIT(result[50],2)?" KEEPS_GAMMA":"",
       BIT(result[50],1)?" KEEPS_WINDOW_CMD":"",
       BIT(result[50],0)?" XYRES_DIFFERENT":"");
  DBG (3, "attach: [51]    ESA2:%s%s%s%s%s%s%s%s\n",
       BIT(result[51],7)?" EXPOSURE_CTRL":"",
       BIT(result[51],6)?" NEED_SW_TRIGGER_CAL":"",
       BIT(result[51],5)?" NEED_WHITE_PAPER_CALIB":"",
       BIT(result[51],4)?" SUPPORTS_QUALITY_SPEED_CAL":"",
       BIT(result[51],3)?" NEED_TRANSP_CAL":"",
       BIT(result[51],2)?" HAS_PUSH_BUTTON":"",
       BIT(result[51],1)?" NEW_CAL_METHOD_3x3_MATRIX_(NO_GAMMA_TABLE)":"",
       BIT(result[51],0)?" ADF_MIRRORS_IMAGE":"");
  DBG (3, "attach: [52]    ESA3:%s%s%s%s%s%s%s%s\n",
       BIT(result[52],7)?" GRAY_WHITE":"",
       BIT(result[52],6)?" SUPPORTS_GAIN_CONTROL":"",
       BIT(result[52],5)?" SUPPORTS_TET":"", /* "Text Enhanced Technology" */
       BIT(result[52],4)?" 3x3COL_TABLE":"",
       BIT(result[52],3)?" 1x3FILTER":"",
       BIT(result[52],2)?" INDEX_COLOR":"",
       BIT(result[52],1)?" POWER_SAVING_TIMER":"",
       BIT(result[52],0)?" NVM_DATA_REC":"");
   
  /* print some more scanner features/params */
  DBG (3, "attach: [53]    line difference (software color pack): %d\n", result[53]);
  DBG (3, "attach: [54]    color mode pixel boundary: %d\n", result[54]);
  DBG (3, "attach: [55]    gray mode pixel boundary: %d\n", result[55]);
  DBG (3, "attach: [56]    4bit gray mode pixel boundary: %d\n", result[56]);
  DBG (3, "attach: [57]    lineart mode pixel boundary: %d\n", result[57]);
  DBG (3, "attach: [58]    halftone mode pixel boundary: %d\n", result[58]);
  DBG (3, "attach: [59]    error-diffusion mode pixel boundary: %d\n", result[59]);
  
  DBG (3, "attach: [60]    channels per pixel:%s%s%s\n",
       BIT(result[60],7)?" 1":"",
       BIT(result[60],6)?" 3":"",
       (result[60] & 0x3F) != 0 ? " RESERVED":"");
  
  DBG (3, "attach: [61]    bits per channel:%s%s%s%s%s%s%s\n", 
       BIT(result[61],7)?" 1":"",
       BIT(result[61],6)?" 4":"",
       BIT(result[61],5)?" 6":"",
       BIT(result[61],4)?" 8":"",
       BIT(result[61],3)?" 10":"",
       BIT(result[61],2)?" 12":"",
       BIT(result[61],1)?" 16":"");
  
  DBG (3, "attach: [62]    scanner type:%s%s%s%s%s%s\n", 
       BIT(result[62],7)?" Flatbed":"",
       BIT(result[62],6)?" Roller (ADF)":"",
       BIT(result[62],5)?" Flatbed (ADF)":"",
       BIT(result[62],4)?" Roller":"", /* does not feed multiple pages, AV25 */
       BIT(result[62],3)?" Film scanner":"",
       BIT(result[62],2)?" Duplex":"");
  
  DBG (3, "attach: [75-76] Max shading target : %x\n",
       get_double ( &(result[75]) ));
  
  DBG (3, "attach: [77-78] Max X of transparency: %d dots * base_dpi\n",
       get_double ( &(result[77]) ));
  DBG (3, "attach: [79-80] Max Y of transparency: %d dots * base_dpi\n",
       get_double ( &(result[79]) ));
  
  DBG (3, "attach: [81-82] Max X of flatbed:      %d dots * base_dpi\n",
       get_double ( &(result[81]) ));
  DBG (3, "attach: [83-84] Max Y of flatbed:      %d dots * base_dpi\n",
       get_double ( &(result[83]) ));
  
  DBG (3, "attach: [85-86] Max X of ADF:          %d dots * base_dpi\n",
       get_double ( &(result[85]) ));
  DBG (3, "attach: [87-88] Max Y of ADF:          %d dots * base_dpi\n",
       get_double ( &(result[87]) )); /* 0xFFFF means unlimited length */
  
  DBG (3, "attach: [89-90] Res. in Ex. mode:      %d dpi\n",
       get_double ( &(result[89]) ));
  
  DBG (3, "attach: [91]    ASIC:     %d\n", result[91]);
   
  DBG (3, "attach: [92]    Buttons:  %d\n", result[92]);
  
  DBG (3, "attach: [93]    ESA4:%s%s%s%s%s%s%s%s\n",
       BIT(result[93],7)?" SUPPORTS_ACCESSORIES_DETECT":"",
       BIT(result[93],6)?" ADF_IS_BGR_ORDERED":"",
       BIT(result[93],5)?" NO_SINGLE_CHANNEL_GRAY_MODE":"",
       BIT(result[93],4)?" SUPPORTS_FLASH_UPDATE":"",
       BIT(result[93],3)?" SUPPORTS_ASIC_UPDATE":"",
       BIT(result[93],2)?" SUPPORTS_LIGHT_DETECT":"",
       BIT(result[93],1)?" SUPPORTS_READ_PRNU_DATA":"",
       BIT(result[93],0)?" FLATBED_MIRRORS_IMAGE":"");
  
  DBG (3, "attach: [94]    ESA5:%s%s%s%s%s%s%s%s\n",
       BIT(result[94],7)?" IGNORE_LINE_DIFFERENCE_FOR_ADF":"",
       BIT(result[94],6)?" NEEDS_SW_LINE_COLOR_PACK":"",
       BIT(result[94],5)?" SUPPORTS_DUPLEX_SCAN":"",
       BIT(result[94],4)?" INTERLACED_DUPLEX_SCAN":"",
       BIT(result[94],3)?" SUPPORTS_TWO_MODE_ADF_SCANS":"",
       BIT(result[94],2)?" SUPPORTS_TUNE_SCAN_LENGTH":"",
       BIT(result[94],1)?" SUPPORTS_SWITCH_STRIP_FOR_DESKEW":"", /* Kodak i80 only */
       BIT(result[94],0)?" SEARCHES_LEADING_SIDE_EDGE_BY_FIRMWARE":"");
  
  DBG (3, "attach: [95]    ESA6:%s%s%s%s%s%s%s%s\n",
       BIT(result[95],7)?" SUPPORTS_PAPER_SIZE_AUTO_DETECTION":"",
       BIT(result[95],6)?" SUPPORTS_DO_HOUSEKEEPING":"", /* Kodak i80 only */
       BIT(result[95],5)?" SUPPORTS_PAPER_LENGTH_SETTING":"", /* AV220, Kodak */
       BIT(result[95],4)?" SUPPORTS_PRE_GAMMA_LINEAR_CORRECTION":"",
       BIT(result[95],3)?" SUPPORTS_PREFEEDING":"", /*  OKI S9800 */
       BIT(result[95],2)?" SUPPORTS_GET_BACKGROUND_RASTER":"", /* AV220 et.al. */
       BIT(result[95],1)?" SUPPORTS_NVRAM_RESET":"",
       BIT(result[95],0)?" SUPPORTS_BATCH_SCAN":"");
  
  DBG (3, "attach: [128]    ESA7:%s%s%s%s%s%s%s%s\n",
       BIT(result[128],7)?" SUPPORTS_ADF_CONTINUOUS":"",
       BIT(result[128],6)?" SUPPORTS_YCbCr_COLOR":"",
       BIT(result[128],5)?" SUPPORTS_ADF_3PASS":"",
       BIT(result[128],4)?" SUPPORTS_TUNE_SCAN_LENGTH_HORIZ":"",
       BIT(result[128],3)?" SUPPORTS_READ_WRITE_ABILITY_PARAMETER":"",
       BIT(result[128],2)?" SUPPORTS_JOB_CONTROL":"",
       BIT(result[128],1)?" SUPPORTS_INF_LENGTH":"",
       BIT(result[128],0)?" ULTRA_SONIC_DOUBLE_FEED_DETECTION":"");
  
  DBG (3, "attach: [129]    YCbCr:%s%s%s%s%s%s%s%s\n",
       BIT(result[129],7)?" YCC4:2:0":"",
       BIT(result[129],6)?" YCC(profile2)":"",
       BIT(result[129],5)?" YCC(profile3)":"",
       BIT(result[129],4)?" YCC(profile4)":"",
       BIT(result[129],3)?" JPEG(profile1)":"",
       BIT(result[129],2)?" JPEG(profile2)":"",
       BIT(result[129],1)?" JPEG(profile3)":"",
       BIT(result[129],0)?" JPEG(profile4)":"");
  
  /* I have no idea how film scanner could reliably be detected -ReneR */
  if (dev->hw->feature_type & AV_FILMSCANNER) {
    dev->scanner_type = AV_FILM;
    dev->sane.type = "film scanner";
  }
  else if ( BIT(result[62],6) || BIT(result[62],4) ) {
    dev->scanner_type = AV_SHEETFEED;
    dev->sane.type =  "sheetfed scanner";
  }
  else {
    dev->scanner_type = AV_FLATBED;
    dev->sane.type = "flatbed scanner";
  }

  dev->inquiry_new_protocol = BIT (result[39],2);
  dev->inquiry_asic_type = (int) result[91];

  dev->inquiry_nvram_read = BIT(result[52],0);
  dev->inquiry_power_save_time = BIT(result[52],1);
  
  dev->inquiry_adf = BIT (result[62], 5);
  dev->inquiry_duplex = BIT (result[62], 2) || BIT (result[94], 5);
  dev->inquiry_duplex_interlaced = BIT(result[62],2) || BIT (result[94], 4); 
  /* the first avision scanners (AV3200) do not set the interlaced bit */
  if (dev->inquiry_duplex && dev->inquiry_asic_type < AV_ASIC_C6)
    dev->inquiry_duplex_interlaced = 1;

  dev->inquiry_paper_length = BIT (result[95], 5);
  dev->inquiry_batch_scan = BIT (result[95], 0); /* AV122, DM152 */
  
  dev->inquiry_detect_accessories = BIT (result[93], 7);
 
  dev->inquiry_needs_calibration = BIT (result[50], 4);

  dev->inquiry_keeps_window = BIT (result[50], 1);
  if (Avision_Device_List [model_num].feature_type & AV_DOES_NOT_KEEP_WINDOW)
    dev->inquiry_keeps_window = 0;
  if (Avision_Device_List [model_num].feature_type & AV_DOES_KEEP_WINDOW)
    dev->inquiry_keeps_window = 1;
  
  dev->inquiry_needs_gamma = BIT (result[50], 3);
  dev->inquiry_keeps_gamma = BIT (result[50], 2);
  if (Avision_Device_List [model_num].feature_type & AV_DOES_NOT_KEEP_GAMMA)
    dev->inquiry_keeps_gamma = 0;
  if (Avision_Device_List [model_num].feature_type & AV_DOES_KEEP_GAMMA)
    dev->inquiry_keeps_gamma = 1;

  dev->inquiry_3x3_matrix = BIT (result[51], 1);
  dev->inquiry_needs_software_colorpack = BIT (result[50],5);
  
  dev->inquiry_needs_line_pack = BIT (result[94], 6);
  
  dev->inquiry_adf_need_mirror = BIT (result[51], 0);
  dev->inquiry_adf_bgr_order = BIT (result[93], 6);
  if (Avision_Device_List [model_num].feature_type & AV_ADF_BGR_ORDER_INVERT)
    dev->inquiry_adf_bgr_order = ! dev->inquiry_adf_bgr_order;
  
  dev->inquiry_light_detect = BIT (result[93], 2);
  dev->inquiry_light_control = BIT (result[50], 7);
  dev->inquiry_button_control = BIT (result[50], 6) | BIT (result[51],2);
  
  dev->inquiry_exposure_control = BIT(result[51],7);
  dev->inquiry_max_shading_target = get_double ( &(result[75]) );
  
  dev->inquiry_color_boundary = result[54];
  if (dev->inquiry_color_boundary == 0)
    dev->inquiry_color_boundary = 8;
  
  dev->inquiry_gray_boundary = result[55];
  if (dev->inquiry_gray_boundary == 0)
    dev->inquiry_gray_boundary = 8;
  
  dev->inquiry_dithered_boundary = result[59];
  if (dev->inquiry_dithered_boundary == 0)
    dev->inquiry_dithered_boundary = 8;
  
  dev->inquiry_thresholded_boundary = result[57];
  if (dev->inquiry_thresholded_boundary == 0)
    dev->inquiry_thresholded_boundary = 8;

  dev->inquiry_line_difference = result[53];
  /* compensation according to real world hardware */
  switch (dev->inquiry_asic_type)
  {
    case AV_ASIC_C2: /* HP 5300 */
    case AV_ASIC_C5: /* HP 53xx R2 */
      dev->inquiry_line_difference /= 2; /* HP 5300 */
      break;
    case AV_ASIC_C7:
      dev->inquiry_line_difference *= 2; /* AV610C2 */
      break;
    default:
      ;
  }
  
  if (dev->inquiry_new_protocol) {
    dev->inquiry_optical_res = get_double ( &(result[89]) );
    dev->inquiry_max_res = get_double ( &(result[44]) );
  }
  else {
    dev->inquiry_optical_res = result[37] * 100;
    dev->inquiry_max_res = result[38] * 100;
  }

  /* fixup max res */
  if (dev->inquiry_optical_res > dev->inquiry_max_res) {
    DBG (1, "Inquiry optical resolution > max_resolution, adjusting!\n");
    dev->inquiry_max_res = dev->inquiry_optical_res;
  }
  
  if (dev->inquiry_optical_res == 0)
    {
      DBG (1, "Inquiry optical resolution is invalid!\n");
      if (dev->hw->feature_type & AV_FORCE_FILM)
	dev->inquiry_optical_res = 2438; /* verify */
      if (dev->scanner_type == AV_SHEETFEED)
	dev->inquiry_optical_res = 300;
      else
	dev->inquiry_optical_res = 600;
    }
  if (dev->inquiry_max_res == 0) {
    DBG (1, "Inquiry max resolution is invalid, using 1200 dpi!\n");
    dev->inquiry_max_res = 1200;
  }
  
  DBG (1, "attach: optical resolution set to: %d dpi\n", dev->inquiry_optical_res);
  DBG (1, "attach: max resolution set to: %d dpi\n", dev->inquiry_max_res);

  if (BIT(result[60],6))
    dev->inquiry_channels_per_pixel = 3;
  else if (BIT(result[60],7))
    dev->inquiry_channels_per_pixel = 1;
  else if ( ((result[36] >> 4) & 0x7) > 0)
    dev->inquiry_channels_per_pixel = 3;
  else
    dev->inquiry_channels_per_pixel = 1;
  
  if (BIT(result[61],1))
    dev->inquiry_bits_per_channel = 16;
  else if (BIT(result[61],2))
    dev->inquiry_bits_per_channel = 12;
  else if (BIT(result[61],3))
    dev->inquiry_bits_per_channel = 10;
  else if (BIT(result[61],4))
    dev->inquiry_bits_per_channel = 8;
  else if (BIT(result[61],5))
    dev->inquiry_bits_per_channel = 6;
  else if (BIT(result[61],6))
    dev->inquiry_bits_per_channel = 4;
  else if (BIT(result[61],7))
    dev->inquiry_bits_per_channel = 1;
  else
    dev->inquiry_bits_per_channel = 8; /* default for old scanners */

  if (dev->hw->feature_type & AV_12_BIT_MODE)
    dev->inquiry_bits_per_channel = 12;

  if (! (dev->hw->feature_type & AV_GRAY_MODES))
    dev->inquiry_no_gray_modes = BIT(result[93],5);

  DBG (1, "attach: max channels per pixel: %d, max bits per channel: %d\n",
    dev->inquiry_channels_per_pixel, dev->inquiry_bits_per_channel);

  if (! (dev->hw->feature_type & AV_NO_BUTTON))
    dev->inquiry_buttons = result[92];
  
  /* get max x/y ranges for the different modes */
  {
    double base_dpi; /* TODO: make int */
    if (dev->scanner_type != AV_FILM) {
      base_dpi = AVISION_BASE_RES;
    } else {
      /* ZP: The right number is 2820, whether it is 40-41, 42-43, 44-45,
       * 46-47 or 89-90 I don't know but I would bet for the last !
       * ReneR: OK. We use it via the optical_res which we need anyway ...
       */
      base_dpi = dev->inquiry_optical_res;
    }
    
    /* .1 to slightly increase the size to match the one of American standard paper
       formats that would otherwise be .1 mm too large to scan ... */
    dev->inquiry_x_ranges [AV_NORMAL_DIM] =
      (double)get_double (&(result[81])) * MM_PER_INCH / base_dpi + .1;
    dev->inquiry_y_ranges [AV_NORMAL_DIM] =
      (double)get_double (&(result[83])) * MM_PER_INCH / base_dpi;
  
    dev->inquiry_x_ranges [AV_TRANSPARENT_DIM] =
      (double)get_double (&(result[77])) * MM_PER_INCH / base_dpi + .1;
    dev->inquiry_y_ranges [AV_TRANSPARENT_DIM] =
      (double)get_double (&(result[79])) * MM_PER_INCH / base_dpi;
  
    dev->inquiry_x_ranges [AV_ADF_DIM] =
      (double)get_double (&(result[85])) * MM_PER_INCH / base_dpi + .1;
    dev->inquiry_y_ranges [AV_ADF_DIM] =
      (double)get_double (&(result[87])) * MM_PER_INCH / base_dpi;
  }
  
  dev->inquiry_tune_scan_length = BIT(result[94],2);
  if (Avision_Device_List [model_num].feature_type & AV_NO_TUNE_SCAN_LENGTH)
    dev->inquiry_tune_scan_length = 0;

  dev->inquiry_background_raster = BIT(result[95],2);
  
  if (dev->hw->feature_type & AV_NO_BACKGROUND)
    dev->inquiry_background_raster = 0;
  
  if (dev->inquiry_background_raster) {
    dev->inquiry_background_raster_pixel =
      get_double(&(result[85])) * dev->inquiry_optical_res / AVISION_BASE_RES;
  }
  
  /* check if x/y ranges are valid :-((( */
  {
    source_mode_dim mode;

    for (mode = AV_NORMAL_DIM; mode < AV_SOURCE_MODE_DIM_LAST; ++ mode)
      {
	if (dev->inquiry_x_ranges [mode] != 0 &&
	    dev->inquiry_y_ranges [mode] != 0)
	  {
	    DBG (3, "attach: x/y-range for mode %d is valid!\n", mode);
	    if (force_a4) {
	      DBG (1, "attach: \"force_a4\" found! Using default (ISO A4).\n");
	      dev->inquiry_x_ranges [mode] = A4_X_RANGE * MM_PER_INCH;
	      dev->inquiry_y_ranges [mode] = A4_Y_RANGE * MM_PER_INCH;
	    } else if (force_a3) {
	      DBG (1, "attach: \"force_a3\" found! Using default (ISO A3).\n");
	      dev->inquiry_x_ranges [mode] = A3_X_RANGE * MM_PER_INCH;
	      dev->inquiry_y_ranges [mode] = A3_Y_RANGE * MM_PER_INCH;
	    }
	  }
	else /* mode is invalid */
	  {
	    DBG (1, "attach: x/y-range for mode %d is invalid! Using a default.\n", mode);
	    if (dev->hw->feature_type & AV_FORCE_A3) {
	      dev->inquiry_x_ranges [mode] = A3_X_RANGE * MM_PER_INCH;
	      dev->inquiry_y_ranges [mode] = A3_Y_RANGE * MM_PER_INCH;
	    }
	    else if (dev->hw->feature_type & AV_FORCE_FILM) {
	      dev->inquiry_x_ranges [mode] = FILM_X_RANGE * MM_PER_INCH;
	      dev->inquiry_y_ranges [mode] = FILM_Y_RANGE * MM_PER_INCH;
	    }
	    else {
	      dev->inquiry_x_ranges [mode] = A4_X_RANGE * MM_PER_INCH;
	      
	      if (dev->scanner_type == AV_SHEETFEED)
		dev->inquiry_y_ranges [mode] = SHEETFEED_Y_RANGE * MM_PER_INCH;
	      else
		dev->inquiry_y_ranges [mode] = A4_Y_RANGE * MM_PER_INCH;
	    }
	  }
	DBG (1, "attach: Mode %d range is now: %f x %f mm.\n",
	     mode,
	     dev->inquiry_x_ranges [mode], dev->inquiry_y_ranges [mode]);
      } /* end for all modes */
  }
  
  /* We need a bigger buffer for USB devices, since they seem to have
     a firmware bug and do not support reading the calibration data in
     tiny chunks */
  if (av_con.connection_type == AV_USB)
    dev->scsi_buffer_size = 1024 * 1024; /* or 0x10000, used by AV Windows driver during background raster read, ... ? */
  else
    dev->scsi_buffer_size = sanei_scsi_max_request_size;

  if (dev->inquiry_asic_type > AV_ASIC_C7 && dev->inquiry_asic_type < AV_ASIC_OA980)
    dev->read_stripe_size = 16;
  else if (dev->inquiry_asic_type >= AV_ASIC_C5)
    dev->read_stripe_size = 32;
  else  /* tested on AV3200 with it's max of 300dpi @color */
    dev->read_stripe_size = 8; /* maybe made dynamic on scan res ... */
  
  /* normally the data_dq is 0x0a0d - but some newer scanner hang with it ... */
  if (dev->inquiry_new_protocol) /* TODO: match on ASIC? which model hung? */
    dev->data_dq = 0x0a0d;
  else
    dev->data_dq = 0;
  
  avision_close (&av_con);
  
  ++ num_devices;
  dev->next = first_dev;
  first_dev = dev;
  if (devp)
    *devp = dev;
  
  return SANE_STATUS_GOOD;
  
 close_scanner_and_return:
  avision_close (&av_con);
  
  return status;
}


static SANE_Status
get_tune_scan_length (Avision_Scanner* s)
{
  SANE_Status status;
  int i;
  
  struct command_read rcmd;
  size_t size;
  
  struct max_value {
    uint8_t max [2];
  } payload;
  
  /* turn on the light */
  DBG (3, "get_tune_scan_length:\n");

  memset (&rcmd, 0, sizeof (rcmd));
  size = sizeof (payload);
  
  rcmd.opc = AVISION_SCSI_READ;
  rcmd.datatypecode = 0xD2; /* Read General Ability/Parameter */
  
  for (i = 1; i <= 8; ++i) {
    memset (&payload, 0, sizeof (payload));
    
    set_double (rcmd.datatypequal, i); /* type */
    set_triple (rcmd.transferlen, size);
    
    status = avision_cmd (&s->av_con, &rcmd, sizeof (rcmd),
			  0, 0, &payload, &size);
    
    if (status != SANE_STATUS_GOOD) {
      DBG (1, "get_tune_scan_length: read %d failed (%s)\n", i, sane_strstatus (status));
      return status;
    }
    DBG (1, "get_tune_scan_length: %d: %d\n", i, get_double (payload.max));
  }
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
send_tune_scan_length (Avision_Scanner* s)
{
  int top, bottom;
  
  SANE_Status status;
  size_t size;
  struct command_send scmd;
  struct truncate_attach {
    uint8_t vertical [2];
 /*   uint8_t horizontal [2]; not send by the Windows driver, yet */
  } payload;
  
  DBG (3, "send_tune_scan_length:\n");
  
  memset (&scmd, 0, sizeof (scmd));
  
  size = sizeof (payload);
  scmd.opc = AVISION_SCSI_SEND;
  scmd.datatypecode = 0x96; /* Attach/Truncate head(left) of scan length */
  set_triple (scmd.transferlen, size);
  
  /* the SPEC says optical DPI, but real world measuring suggests it is 1200
     as in the window descriptor */
  top = 1200 * SANE_UNFIX (s->val[OPT_OVERSCAN_TOP].w) / MM_PER_INCH;
  DBG (3, "send_tune_scan_length: top: %d\n", top);
  
  set_double (scmd.datatypequal, 0x0001); /* attach, 0x000 is shorten */
  set_double (payload.vertical, top);
  /* set_double (payload.horizontal, 0); */
  
  /* we alway send it, even for 0 as the scanner keeps it in RAM and
     previous runs could already have set something */
  
  status = avision_cmd (&s->av_con, &scmd, sizeof (scmd),
			&payload, sizeof (payload), 0, 0);
  
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "send_tune_scan_length: send top/left failed (%s)\n", sane_strstatus (status));
    return status;
  }

  scmd.datatypecode = 0x95; /* Attach/Truncate tail(right) of scan length */
  bottom = 1200 * SANE_UNFIX (s->val[OPT_OVERSCAN_BOTTOM].w) / MM_PER_INCH;
  DBG (3, "send_tune_scan_length: bottom: %d\n", bottom);

  set_double (payload.vertical, bottom);
  /*set_double (payload.horizontal, 0); */
  
  size = sizeof (payload);
  status = avision_cmd (&s->av_con, &scmd, sizeof (scmd),
			&payload, sizeof (payload), 0, 0);
  
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "send_tune_scan_length: send bottom/right failed (%s)\n", sane_strstatus (status));
    return status;
  }
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
additional_probe (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  
  /* we should wait until the scanner is ready before we
     perform further actions */
  
  SANE_Status status;
  /* try to retrieve additional accessory information */
  if (dev->inquiry_detect_accessories) {
    status = get_accessories_info (s);
    if (status != SANE_STATUS_GOOD)
      return status;
  }
  
  /* for a film scanner try to retrieve additional frame information */
  if (dev->scanner_type == AV_FILM) {
    status = get_frame_info (s);
    if (status != SANE_STATUS_GOOD)
      return status;
  }
  
  /* no scanner did support this so far: tried on AV220, DM152 */
  if (0 && dev->inquiry_duplex) {
    status = get_duplex_info (s);
    if (status != SANE_STATUS_GOOD)
      return status;
  }
  
  /* get overscan ("head/tail tune") information: hangs AV220, zeros on AV122 */
  if (0 && dev->inquiry_tune_scan_length) {
    status = get_tune_scan_length (s);
    if (status != SANE_STATUS_GOOD)
      return status;
  }
  
  /* create dynamic *-mode entries */
  if (!dev->inquiry_no_gray_modes)
  {
    if (dev->inquiry_bits_per_channel > 0) {
      add_color_mode (dev, AV_THRESHOLDED, SANE_VALUE_SCAN_MODE_LINEART);
      add_color_mode (dev, AV_DITHERED, "Dithered");
    }

    if (dev->inquiry_bits_per_channel >= 8)
      add_color_mode (dev, AV_GRAYSCALE, SANE_VALUE_SCAN_MODE_GRAY);

    if (dev->inquiry_bits_per_channel == 12)
      add_color_mode (dev, AV_GRAYSCALE12, "12bit Gray");
  
    if (dev->inquiry_bits_per_channel >= 16)
      add_color_mode (dev, AV_GRAYSCALE16, "16bit Gray");
  }

  if (dev->inquiry_channels_per_pixel > 1) {
    add_color_mode (dev, AV_TRUECOLOR, SANE_VALUE_SCAN_MODE_COLOR);

    if (dev->inquiry_bits_per_channel == 12)
      add_color_mode (dev, AV_TRUECOLOR12, "12bit Color");
    
    if (dev->inquiry_bits_per_channel >= 16)
      add_color_mode (dev, AV_TRUECOLOR16, "16bit Color");
  }
  
  /* now choose the default mode - avoiding the 12/16 bit modes */
  dev->color_list_default = last_color_mode (dev);
  if (dev->inquiry_bits_per_channel > 8 && dev->color_list_default > 0) {
    dev->color_list_default--;
  }
  
  if (dev->scanner_type == AV_SHEETFEED)
    {
      add_source_mode (dev, AV_ADF, "ADF Front");
    }
  else
    {
      add_source_mode (dev, AV_NORMAL, "Normal");
      
      if (dev->inquiry_light_box)
	add_source_mode (dev, AV_TRANSPARENT, "Transparency");
      
      if (dev->inquiry_adf)
        add_source_mode (dev, AV_ADF, "ADF Front");
    }
  
  if (dev->inquiry_duplex) {
    if (dev->inquiry_duplex_interlaced && !(dev->hw->feature_type & AV_NO_REAR))
      add_source_mode (dev, AV_ADF_REAR, "ADF Back");
    add_source_mode (dev, AV_ADF_DUPLEX, "ADF Duplex");
  }
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
get_calib_format (Avision_Scanner* s, struct calibration_format* format)
{
  SANE_Status status;
  
  struct command_read rcmd;
  uint8_t result [32];
  size_t size;
  
  DBG (3, "get_calib_format:\n");

  size = sizeof (result);
  
  memset (&rcmd, 0, sizeof (rcmd));
  rcmd.opc = AVISION_SCSI_READ;
  rcmd.datatypecode = 0x60; /* get calibration format */
  set_double (rcmd.datatypequal, s->hw->data_dq);
  set_triple (rcmd.transferlen, size);
  
  DBG (3, "get_calib_format: read_data: %lu bytes\n", (u_long) size);
  status = avision_cmd (&s->av_con, &rcmd, sizeof (rcmd), 0, 0, result, &size);
  if (status != SANE_STATUS_GOOD || size != sizeof (result) ) {
    DBG (1, "get_calib_format: read calib. info failed (%s)\n",
	 sane_strstatus (status) );
    return status;
  }
  
  debug_print_calib_format (3, "get_calib_format", result);
  
  format->pixel_per_line = get_double (&(result[0]));
  format->bytes_per_channel = result[2];
  format->lines = result[3];
  format->flags = result[4];
  format->ability1 = result[5];
  format->r_gain = result[6];
  format->g_gain = result[7];
  format->b_gain = result[8];
  format->r_shading_target = get_double (&(result[9]));
  format->g_shading_target = get_double (&(result[11]));
  format->b_shading_target = get_double (&(result[13]));
  format->r_dark_shading_target = get_double (&(result[15]));
  format->g_dark_shading_target = get_double (&(result[17]));
  format->b_dark_shading_target = get_double (&(result[19]));
  
  /* now translate to normal! */
  /* firmware return R--RG--GB--B with 3 line count */
  /* software format it as 1 line if true color scan */
  /* only line interleave format to be supported */
  
  if (color_mode_is_color (s->c_mode) || BIT(format->ability1, 3)) {
    format->channels = 3;
    format->lines /= 3; /* line interleave */
  }
  else
    format->channels = 1;
  
  DBG (3, "get_calib_format: channels: %d\n", format->channels);
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
get_calib_data (Avision_Scanner* s, uint8_t data_type,
		uint8_t* calib_data,
		size_t calib_size)
{
  SANE_Status status;
  uint8_t *calib_ptr;
  
  size_t get_size, data_size, chunk_size;
  
  struct command_read rcmd;
  
  chunk_size = calib_size;
  
  DBG (3, "get_calib_data: type %x, size %lu, chunk_size: %lu\n",
       data_type, (u_long) calib_size, (u_long) chunk_size);
  
  memset (&rcmd, 0, sizeof (rcmd));
  
  rcmd.opc = AVISION_SCSI_READ;
  rcmd.datatypecode = data_type;
  set_double (rcmd.datatypequal, s->hw->data_dq);
  
  calib_ptr = calib_data;
  get_size = chunk_size;
  data_size = calib_size;
  
  while (data_size) {
    if (get_size > data_size)
      get_size = data_size;

    read_constrains(s, get_size);

    set_triple (rcmd.transferlen, get_size);

    DBG (3, "get_calib_data: Reading %ld bytes calibration data\n",
         (long)get_size);
 
    status = avision_cmd (&s->av_con, &rcmd,
			  sizeof (rcmd), 0, 0, calib_ptr, &get_size);
    if (status != SANE_STATUS_GOOD) {
      DBG (1, "get_calib_data: read data failed (%s)\n",
	   sane_strstatus (status));
      return status;
    }

    DBG (3, "get_calib_data: Got %ld bytes calibration data\n", (long)get_size);

    data_size -= get_size;
    calib_ptr += get_size;
  }
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
set_calib_data (Avision_Scanner* s, struct calibration_format* format,
		uint8_t* dark_data, uint8_t* white_data)
{
  Avision_Device* dev = s->hw;
  
  const int elements_per_line = format->pixel_per_line * format->channels;
  
  SANE_Status status;
  
  uint8_t send_type;
  uint16_t send_type_q;
  
  struct command_send scmd;
  
  int i;
  size_t out_size;
  
  DBG (3, "set_calib_data:\n");
  
  send_type = 0x82; /* download calibration data */
  
  /* do we use a color mode? */
  if (format->channels > 1) {
    send_type_q = 0x12; /* color calib data */
  }
  else {
    if (dev->hw->feature_type & AV_GRAY_CALIB_BLUE)
      send_type_q = 0x2; /* gray/bw calib data on the blue channel (AV610) */
    else
      send_type_q = 0x11; /* gray/bw calib data */
  }
  
  memset (&scmd, 0x00, sizeof (scmd));
  scmd.opc = AVISION_SCSI_SEND;
  scmd.datatypecode = send_type;
  
  /* data corrections due to dark calibration data merge */
  if (BIT (format->ability1, 2) ) {
    DBG (3, "set_calib_data: merging dark calibration data\n");
    for (i = 0; i < elements_per_line; ++i) {
      uint16_t value_orig = get_double_le (white_data + i*2);
      uint16_t value_new = value_orig;
      
      value_new &= 0xffc0;
      value_new |= (get_double_le (dark_data + i*2) >> 10) & 0x3f;
      
      DBG (9, "set_calib_data: element %d, dark difference %d\n",
	   i, value_orig - value_new);
      
      set_double_le ((white_data + i*2), value_new);
    }
  }
  
  out_size = format->pixel_per_line * 2;
  
  /* send data in one command? */
  /* FR: HP5370 reports one-pass, but needs multi (or other format in single) */
  if (format->channels == 1 ||
      ( ( (dev->hw->feature_type & AV_ONE_CALIB_CMD) ||
	  ! BIT(format->ability1, 0) ) && 
	! (dev->hw->feature_type & AV_MULTI_CALIB_CMD) ) )
    /* one command (most scanners) */
    {
      size_t send_size = elements_per_line * 2;
      DBG (3, "set_calib_data: all channels in one command\n");
      DBG (3, "set_calib_data: send_size: %lu\n", (u_long) send_size);
      
      memset (&scmd, 0, sizeof (scmd) );
      scmd.opc = AVISION_SCSI_SEND;
      scmd.datatypecode = send_type;
      set_double (scmd.datatypequal, send_type_q);
      set_triple (scmd.transferlen, send_size);

      status = avision_cmd (&s->av_con, &scmd, sizeof (scmd),
			    (char*) white_data, send_size, 0, 0);
      /* not return immediately to free mem at the end */
    }
  else /* send data channel by channel (some USB ones) */
    {
      int conv_out_size = format->pixel_per_line * 2;
      uint16_t* conv_out_data; /* here it is save to use 16bit data
				   since we only move whole words around */
      
      DBG (3, "set_calib_data: channels in single commands\n");
      
      conv_out_data =  (uint16_t*) malloc (conv_out_size);
      if (!conv_out_data) {
	status = SANE_STATUS_NO_MEM;
      }
      else {
	int channel;
	for (channel = 0; channel < 3; ++ channel)
	  {
	    int i;
	    
	    /* no need for endianness handling since whole word copy */
	    uint16_t* casted_avg_data = (uint16_t*) white_data;
	    
	    DBG (3, "set_calib_data_calibration: channel: %i\n", channel);
	    
	    for (i = 0; i < format->pixel_per_line; ++ i)
	      conv_out_data [i] = casted_avg_data [i * 3 + channel];
	    
	    DBG (3, "set_calib_data: sending %i bytes now\n",
		 conv_out_size);
	    
	    memset (&scmd, 0, sizeof (scmd));
	    scmd.opc = AVISION_SCSI_SEND;
	    scmd.datatypecode = send_type; /* send calibration data */
	    set_double (scmd.datatypequal, channel);
	    set_triple (scmd.transferlen, conv_out_size);
	    
	    status = avision_cmd (&s->av_con, &scmd, sizeof (scmd),
				  conv_out_data, conv_out_size, 0, 0);
	    if (status != SANE_STATUS_GOOD) {
	      DBG (3, "set_calib_data: send_data failed (%s)\n",
		   sane_strstatus (status));
	      /* not return immediately to free mem at the end */
	    }
	  } /* end for each channel */
	free (conv_out_data);
      } /* end else send calib data*/
    }
  
  return SANE_STATUS_GOOD;
}

/* Sort data pixel by pixel and average first 2/3 of the data.
   The caller has to free return pointer. R,G,B pixels
   interleave to R,G,B line interleave.
   
   The input data data is in 16 bits little endian, always.
   That is a = b[1] << 8 + b[0] in all system.

   We convert it to SCSI high-endian (big-endian) since we use it all
   over the place anyway .... - Sorry for this mess. */

static uint8_t*
sort_and_average (struct calibration_format* format, uint8_t* data)
{
  const int elements_per_line = format->pixel_per_line * format->channels;
  const int stride = format->bytes_per_channel * elements_per_line;
  int i, line;
    
  uint8_t *sort_data, *avg_data;
  
  DBG (1, "sort_and_average:\n");
  
  if (!format || !data)
    return NULL;
  
  sort_data = malloc (format->lines * 2);
  if (!sort_data)
    return NULL;
  
  avg_data = malloc (elements_per_line * 2);
  if (!avg_data) {
    free (sort_data);
    return NULL;
  }
  
  /* for each pixel */
  for (i = 0; i < elements_per_line; ++ i)
    {
      uint8_t* ptr1 = data + i * format->bytes_per_channel;
      uint16_t temp;
      
      /* copy all lines for pixel i into the linear array sort_data */
      for (line = 0; line < format->lines; ++ line) {
	uint8_t* ptr2 = ptr1 + line * stride; /* pixel */
	
	if (format->bytes_per_channel == 1)
	  temp = 0xffff * *ptr2 / 255;
	else
	  temp = get_double_le (ptr2);	  /* little-endian! */
	set_double ((sort_data + line*2), temp); /* store big-endian */
	/* DBG (7, "ReneR to sort: %x\n", temp); */
      }
      
      temp = bubble_sort (sort_data, format->lines);
      /* DBG (7, "ReneR averaged: %x\n", temp); */
      set_double ((avg_data + i*2), temp); /* store big-endian */
    }
  
  free ((void *) sort_data);
  return avg_data;
}

/* shading data is 16bits little endian format when send/read from firmware */
static void
compute_dark_shading_data (Avision_Scanner* s,
			   struct calibration_format* format, uint8_t* data)
{
  uint16_t map_value = DEFAULT_DARK_SHADING;
  uint16_t rgb_map_value[3];
  
  int elements_per_line, i;
  
  DBG (3, "compute_dark_shading_data:\n");
  
  if (s->hw->inquiry_max_shading_target != INVALID_DARK_SHADING)
    map_value = s->hw->inquiry_max_shading_target << 8;
  
  rgb_map_value[0] = format->r_dark_shading_target;
  rgb_map_value[1] = format->g_dark_shading_target;
  rgb_map_value[2] = format->b_dark_shading_target;
  
  for (i = 0; i < format->channels; ++i) {
    if (rgb_map_value[i] == INVALID_DARK_SHADING)
      rgb_map_value[i] = map_value;
  }
  
  if (format->channels == 1) {
    /* set to green, TODO: should depend on color drop-out and true-gray -ReneR */
    rgb_map_value[0] = rgb_map_value[1] = rgb_map_value[2] = rgb_map_value[1];
  }
  
  elements_per_line = format->pixel_per_line * format->channels;

  /* Check line interleave or pixel interleave. */
  /* It seems no ASIC use line interleave right now. */
  /* Avision SCSI protocol document has bad description. */
  for (i = 0; i < elements_per_line; ++i)
    {
      uint16_t tmp_data = get_double_le((data + i*2));
      if (tmp_data > rgb_map_value[i % 3]) {
	set_double ((data + i*2), tmp_data - rgb_map_value[i % 3]);
      }
      else {
	set_double ((data + i*2), 0);
      }
    }
}

static void
compute_white_shading_data (Avision_Scanner* s,
			    struct calibration_format* format, uint8_t* data)
{
  int i;
  uint16_t inquiry_mst = DEFAULT_WHITE_SHADING;
  uint16_t mst[3];
  
  int elements_per_line = format->pixel_per_line * format->channels;
  
  /* debug counter */
  int values_invalid = 0;
  int values_limitted = 0;
  
  DBG (3, "compute_white_shading_data:\n");
  
  if (s->hw->inquiry_max_shading_target != INVALID_WHITE_SHADING)
    inquiry_mst = s->hw->inquiry_max_shading_target << 4;
  
  mst[0] = format->r_shading_target;
  mst[1] = format->g_shading_target;
  mst[2] = format->b_shading_target;
  
  for (i = 0; i < 3; ++i) {
    if (mst[i] == INVALID_WHITE_SHADING) /* mst[i] > MAX_WHITE_SHADING) */  {
      DBG (3, "compute_white_shading_data: target %d invalid (%x) using inquiry (%x)\n",
	   i, mst[i], inquiry_mst);
      mst[i] = inquiry_mst;
    }
    /* some firmware versions seems to return the bytes swapped? */
    else if (mst[i] < 0x110) {
      uint8_t* swap_mst = (uint8_t*) &mst[i];
      uint8_t low_nibble_mst = swap_mst [0];
      swap_mst [0] = swap_mst[1];
      swap_mst [1] = low_nibble_mst;

      DBG (3, "compute_white_shading_data: target %d: bytes swapped.\n", i);
    }
    if (mst[i] < DEFAULT_WHITE_SHADING / 2) {
      DBG (3, "compute_white_shading_data: target %d: too low (%d) using default (%d).\n",
	   i, mst[i], DEFAULT_WHITE_SHADING);
      mst[i] = DEFAULT_WHITE_SHADING;
    }
    else
      DBG (3, "compute_white_shading_data: target %d: %x\n", i, mst[0]);
  }
  
  /* some Avision example code was present here until SANE/Avision
   * BUILD 57. */

  if (format->channels == 1) {
    /* set to green, TODO: should depend on color drop-out and true-gray -ReneR */
    mst[0] = mst[1] = mst[2] = mst[1];
  }
  
  /* calculate calibration data */
  for (i = 0; i < elements_per_line; ++ i)
    {
      int result;
      /* calculate calibration value for pixel i */
      uint16_t tmp_data = get_double((data + i*2));
      
      if (tmp_data == INVALID_WHITE_SHADING) {
       	tmp_data = DEFAULT_WHITE_SHADING;
	++ values_invalid;
      }
      
      result = ( (int)mst[i % 3] * WHITE_MAP_RANGE / (tmp_data + 0.5));
      
      /* sanity check for over-amplification, clipping */
      if (result > MAX_WHITE_SHADING) {
	result = WHITE_MAP_RANGE;
	++ values_limitted;
      }
      
      /* for visual debugging ... */
      if (static_calib_list [i % 3] == SANE_TRUE)
	result = 0xA000;
      
      /* the output to the scanner will be 16 bit little endian again */
      set_double_le ((data + i*2), result);
    }
  DBG (3, "compute_white_shading_data: %d invalid, %d limited\n",
       values_invalid, values_limitted);
}

/* old_r_calibration was here until SANE/Avision BUILD 90 */

static SANE_Status
normal_calibration (Avision_Scanner* s)
{
  SANE_Status status;
  
  struct calibration_format calib_format;
  
  int calib_data_size, calib_bytes_per_line;
  uint8_t read_type;
  uint8_t *calib_tmp_data;
  
  DBG (1, "normal_calibration:\n");
  
  /* get calibration format and data */
  status = get_calib_format (s, &calib_format);
  if (status != SANE_STATUS_GOOD)
    return status;
  
  /* check if need do calibration */
  if (calib_format.flags != 1) {
    DBG (1, "normal_calibration: Scanner claims no calibration needed -> skipped!\n");
    return SANE_STATUS_GOOD;
  }

  /* calculate calibration data size for read from scanner */
  /* size = lines * bytes_per_channel * pixels_per_line * channel */
  calib_bytes_per_line = calib_format.bytes_per_channel *
    calib_format.pixel_per_line * calib_format.channels;
  
  calib_data_size = calib_format.lines * calib_bytes_per_line;
  
  calib_tmp_data = malloc (calib_data_size);
  if (!calib_tmp_data)
    return SANE_STATUS_NO_MEM;
  
  /* check if we need to do dark calibration (shading) */
  if (BIT(calib_format.ability1, 3))
    {
      DBG (1, "normal_calibration: reading dark data\n");
      /* read dark calib data */
      status = get_calib_data (s, 0x66, calib_tmp_data, calib_data_size);
      
      if (status != SANE_STATUS_GOOD) {
	free (calib_tmp_data);
	return status;
      }
      
      /* process dark data: sort and average. */
      
      if (s->dark_avg_data) {
	free (s->dark_avg_data);
	s->dark_avg_data = 0;
      }
      s->dark_avg_data = sort_and_average (&calib_format, calib_tmp_data);
      if (!s->dark_avg_data) {
	free (calib_tmp_data);
	return SANE_STATUS_NO_MEM;
      }
      compute_dark_shading_data (s, &calib_format, s->dark_avg_data);
    }
  
  /* do we use a color mode? */
  if (calib_format.channels > 1) {
    DBG (3, "normal_calibration: using color calibration\n");
    read_type = 0x62; /* read color calib data */
  }
  else {
    DBG (3, "normal_calibration: using gray calibration\n");
    read_type = 0x61; /* gray calib data */
  }
  
  /* do white calibration: read gray or color data */
  status = get_calib_data (s, read_type, calib_tmp_data, calib_data_size);
  
  if (status != SANE_STATUS_GOOD) {
    free (calib_tmp_data);
    return status;
  }
  
  if (0) /* debug */
    {
      FILE* f = NULL;
      f = fopen ("calibration-white.pnm", "w");
      write_pnm_header (f, AV_GRAYSCALE, calib_format.bytes_per_channel * 8,
			calib_format.pixel_per_line,
			calib_format.lines * calib_format.channels);
      
      fwrite (calib_tmp_data, 1, calib_data_size, f);
      fclose (f);
    }
  
  if (s->white_avg_data) {
    free (s->white_avg_data);
    s->white_avg_data = 0;
  }
  s->white_avg_data = sort_and_average (&calib_format, calib_tmp_data);
  if (!s->white_avg_data) {
    free (calib_tmp_data);
    return SANE_STATUS_NO_MEM;
  }
  
  /* decrease white average data (if dark average data is present) */
  if (s->dark_avg_data) {
    int elements_per_line = calib_format.pixel_per_line * calib_format.channels;
    int i;

    DBG (1, "normal_calibration: dark data present - decreasing white average data\n");
    
    for (i = 0; i < elements_per_line; ++ i) {
      s->white_avg_data[i] -= s->dark_avg_data[i];
    }
  }
  
  compute_white_shading_data (s, &calib_format, s->white_avg_data);
  
  status = set_calib_data (s, &calib_format,
			   s->dark_avg_data, s->white_avg_data);
  
  free (calib_tmp_data);
  return status;
}

/* next was taken from the GIMP and is a bit modified ... ;-)
 * original Copyright (C) 1995 Spencer Kimball and Peter Mattis
 */
static double 
brightness_contrast_func (double brightness, double contrast, double value)
{
  double nvalue;
  double power;
  
  /* apply brightness */
  if (brightness < 0.0)
    value = value * (1.0 + brightness);
  else
    value = value + ((1.0 - value) * brightness);

  /* apply contrast */
  if (contrast < 0.0)
    {
      if (value > 0.5)
	nvalue = 1.0 - value;
      else
	nvalue = value;
      if (nvalue < 0.0)
	nvalue = 0.0;
      nvalue = 0.5 * pow (nvalue * 2.0 , (double) (1.0 + contrast));
      if (value > 0.5)
	value = 1.0 - nvalue;
      else
	value = nvalue;
    }
  else
    {
      if (value > 0.5)
	nvalue = 1.0 - value;
      else
	nvalue = value;
      if (nvalue < 0.0)
	nvalue = 0.0;
      power = (contrast == 1.0) ? 127 : 1.0 / (1.0 - contrast);
      nvalue = 0.5 * pow (2.0 * nvalue, power);
      if (value > 0.5)
	value = 1.0 - nvalue;
      else
	value = nvalue;
    }
  return value;
}

static SANE_Status
send_gamma (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  SANE_Status status = SANE_STATUS_GOOD;
  
  int invert_table = 0;
  
  size_t gamma_table_raw_size;
  size_t gamma_table_size;
  size_t gamma_values;
  
  struct command_send scmd;
  uint8_t *gamma_data;
  
  int color; /* current color */
  size_t i; /* big table index */
  size_t j; /* little table index */
  size_t k; /* big table sub index */
  double v1, v2;
  
  double brightness;
  double contrast;
  
  if (dev->inquiry_asic_type != AV_ASIC_OA980)
    invert_table = (s->c_mode == AV_THRESHOLDED) || (s->c_mode == AV_DITHERED);
  
  switch (dev->inquiry_asic_type)
    {
    case AV_ASIC_Cx:
    case AV_ASIC_C1:
      gamma_table_raw_size = 4096;
      gamma_table_size = 2048;
      break;
    case AV_ASIC_C5:
      gamma_table_raw_size = 256;
      gamma_table_size = 256;
      break;
      break;
    case AV_ASIC_OA980:
      gamma_table_raw_size = 4096;
      gamma_table_size = 4096;
      break;
    case AV_ASIC_OA982:
      gamma_table_raw_size = 256;
      gamma_table_size = 256;
      break;
    default:
      gamma_table_raw_size = 512; /* SPEC claims: 256 ... ? */
      gamma_table_size = 512;
    }
  
  gamma_values = gamma_table_size / 256;
  
  DBG (3, "send_gamma: table_raw_size: %lu, table_size: %lu\n",
       (u_long) gamma_table_raw_size, (u_long) gamma_table_size);
  DBG (3, "send_gamma: values: %lu, invert_table: %d\n",
       (u_long) gamma_values, invert_table);
  
  /* prepare for emulating contrast, brightness ... via the gamma-table */
  brightness = SANE_UNFIX (s->val[OPT_BRIGHTNESS].w);
  brightness /= 100;
  contrast = SANE_UNFIX (s->val[OPT_CONTRAST].w);
  contrast /= 100;
  
  DBG (3, "send_gamma: brightness: %f, contrast: %f\n", brightness, contrast);
  
  gamma_data = malloc (gamma_table_raw_size);
  if (!gamma_data)
    return SANE_STATUS_NO_MEM;
  
  memset (&scmd, 0, sizeof (scmd) );
  
  scmd.opc = AVISION_SCSI_SEND;
  scmd.datatypecode = 0x81; /* 0x81 for download gamma table */
  set_triple (scmd.transferlen, gamma_table_raw_size);
  
  for (color = 0; color < 3 && status == SANE_STATUS_GOOD; ++ color)
    {
      /* color: 0=red; 1=green; 2=blue */
      set_double (scmd.datatypequal, color);
      
      i = 0; /* big table index */
      for (j = 0; j < 256; ++ j) /* little table index */
	{
	  /* calculate mode dependent values v1 and v2
	   * v1 <- current value for table
	   * v2 <- next value for table (for interpolation)
	   */
	  switch (s->c_mode)
	    {
	    case AV_TRUECOLOR:
	    case AV_TRUECOLOR12:
	    case AV_TRUECOLOR16:
	      {
		v1 = (double) s->gamma_table [1 + color][j];
		if (j == 255)
		  v2 = (double) v1;
		else
		  v2 = (double) s->gamma_table [1 + color][j + 1];
	      }
	      break;
	    default:
	      /* for all other modes: */
	      {
		v1 = (double) s->gamma_table [0][j];
		if (j == 255)
		  v2 = (double) v1;
		else
		  v2 = (double) s->gamma_table [0][j + 1];
	      }
	    } /*end switch */
	  
	  /* Emulate brightness and contrast (at least the Avision AV6[2,3]0
	   * as well as many others do not have a hardware implementation,
	   * --$. The function was taken from the GIMP source - maybe I'll
	   * optimize it in the future (when I have spare time). */
	  
	  v1 /= 255;
	  v2 /= 255;
	      
	  v1 = (brightness_contrast_func (brightness, contrast, v1) );
	  v2 = (brightness_contrast_func (brightness, contrast, v2) );
	      
	  v1 *= 255;
	  v2 *= 255;
	  
	  if (invert_table) {
	    v1 = 255 - v1;
	    v2 = 255 - v2;
	    if (v1 <= 0)
	      v1 = 0;
	    if (v2 <= 0)
	      v2 = 0;
	  }
	  
	  for (k = 0; k < gamma_values; ++ k, ++ i) {
	    gamma_data [i] = (uint8_t)
	      (((v1 * (gamma_values - k)) + (v2 * k) ) / (double) gamma_values);
	  }
	}
      /* fill the gamma table - (e.g.) if 11bit (old protocol) table */
      {
	size_t t_i = i-1;
	if (i < gamma_table_raw_size) {
	  DBG (4, "send_gamma: (old protocol) - filling the table.\n");
	  for ( ; i < gamma_table_raw_size; ++ i)
	    gamma_data [i] = gamma_data [t_i];
	}
      }
      
      DBG (4, "send_gamma: sending %lu bytes gamma table.\n",
	   (u_long) gamma_table_raw_size);
      status = avision_cmd (&s->av_con, &scmd, sizeof (scmd),
			    gamma_data, gamma_table_raw_size, 0, 0);
      
      if (status != SANE_STATUS_GOOD) {
	DBG (1, "send_gamma: gamma table upload failed: %s\n",
	     sane_strstatus (status));
      }
    }
  free (gamma_data);
  return status;
}

static SANE_Status
send_3x3_matrix (Avision_Scanner* s)
{
  SANE_Status status;
  
#define SIGN_BIT 0x1000
#define INT_PART 10
  
  struct matrix_cmd
  {
    struct command_send scmd;
    struct matrix_3x3 matrix;
  } cmd;
  
  /* 04 00  00 00  00 00 
     00 00  04 00  00 00
     00 00  00 00  04 00 */
  
  int i, a_i;
  static const double c5_matrix[] =
    { 1.000, 0.000, 0.000,
      0.000, 1.000, 0.000,
      0.000, 0.000, 1.000 };
  
  double a_f, b_f;
  uint16_t m;
  
  DBG (3, "send_3x3_matrix:\n");
  
  memset (&cmd, 0, sizeof (cmd));
  
  for (i = 0; i < 9; i++)
    {
      m = 0;
      a_f = c5_matrix[i];
      if (a_f < 0) {
	m |= SIGN_BIT;
	a_f = -a_f;
      }
      
      a_i = (int) a_f; /* integer */
      b_f = a_f - (double) a_i;  /* float */
      m |= ((a_i & 0x3) << INT_PART);
      m |= (uint16_t) (b_f * 1024);
      set_double (((uint8_t*)(&cmd.matrix.v[i])), m);
    }
  
  cmd.scmd.opc = AVISION_SCSI_SEND;
  cmd.scmd.datatypecode = 0x83; /* 0x83 for 3x3 color matrix */
  set_triple (cmd.scmd.transferlen, sizeof (struct matrix_3x3));
  
  if (1) {
    DBG (3, "send_3x3_matrix: sending matrix split into two commands\n");
    status = avision_cmd (&s->av_con, &cmd.scmd, sizeof (cmd.scmd),
                          &cmd.matrix, sizeof(cmd.matrix), 0, 0);
  }
  else {
    DBG (3, "send_3x3_matrix: sending matrix in one command\n");
    status = avision_cmd (&s->av_con, &cmd, sizeof (cmd), 0, 0, 0, 0);
  }
  
  return status;
}

static SANE_Status
get_acceleration_info (Avision_Scanner* s, struct acceleration_info* info)
{
  SANE_Status status;
  
  struct command_read rcmd;
  uint8_t result [24];
  size_t size;
  
  DBG (3, "get_acceleration_info:\n");

  size = sizeof (result);
  
  memset (&rcmd, 0, sizeof (rcmd));
  rcmd.opc = AVISION_SCSI_READ;
  rcmd.datatypecode = 0x6c; /* get acceleration information */
  set_double (rcmd.datatypequal, s->hw->data_dq);
  set_triple (rcmd.transferlen, size);
  
  DBG (3, "get_acceleration_info: read_data: %lu bytes\n", (u_long) size);
  status = avision_cmd (&s->av_con, &rcmd, sizeof (rcmd), 0, 0, result, &size);
  if (status != SANE_STATUS_GOOD || size != sizeof (result) ) {
    DBG (1, "get_acceleration_info: read accel. info failed (%s)\n",
	 sane_strstatus (status) );
    return status;
  }
  
  debug_print_accel_info (3, "get_acceleration_info", result);
  
  info->total_steps = get_double (&(result[0]));
  info->stable_steps = get_double (&(result[2]));
  info->table_units = get_quad (&(result[4]));
  info->base_units = get_quad (&(result[8]));
  info->start_speed = get_double (&(result[12]));
  info->target_speed = get_double (&(result[14]));
  info->ability = result[16];
  info->table_count = result[17];
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
send_acceleration_table (Avision_Scanner* s)
{
  SANE_Status status;
  
  struct command_send scmd;
  int table = 0;
  int i;
  struct acceleration_info accel_info = accel_info;
  uint8_t* table_data;
  
  DBG (3, "send_acceleration_table:\n");

  do {
    status = get_acceleration_info (s, &accel_info);
    
    if (accel_info.table_count == 0) {
      DBG (3, "send_acceleration_table: device does not need tables\n");
      return SANE_STATUS_GOOD;
    }
    
    if (accel_info.target_speed > accel_info.start_speed ||
        accel_info.target_speed == 0 ||
        accel_info.total_steps <= accel_info.stable_steps) {
      DBG (1, "send_acceleration_table: table does not look right.\n");
      return SANE_STATUS_INVAL;
    }
    
    if (accel_info.ability != 0) {
      DBG (1, "send_acceleration_table: ability non-zero - insert code\n");
      return SANE_STATUS_INVAL;
    }
    
    /* so far I assume we have one byte tables as used in the C6 ASIC ... */
    table_data = malloc (accel_info.total_steps + 1000);
    
    memset (&scmd, 0x00, sizeof (scmd));
    scmd.opc = AVISION_SCSI_SEND;
    scmd.datatypecode = 0x6c; /* send acceleration table */
      
    set_double (scmd.datatypequal, table);
    set_triple (scmd.transferlen, accel_info.total_steps);
    
    /* construct the table - Warning: This code is derived from Avision
       sample code and is a bit scary! I have no idea why the scanner
       needs such a dumb table and also do not know /why/ it has to be
       constructed this way. "Works for me" -ReneR */
    {
      float low_lim = 0.001;
      float up_lim  = 1.0;

      uint16_t accel_steps = accel_info.total_steps - accel_info.stable_steps + 1;
      
      /* acceleration ramp */
      while ((up_lim - low_lim) > 0.0001)
      {
	float mid = (up_lim + low_lim) / 2; /* accel rate */
	
	uint16_t now_count = accel_info.start_speed;
	
	uint16_t i = 0;
	
	float now_count_f = now_count;
	table_data [i++] = (uint8_t) accel_info.start_speed;

	while (now_count != accel_info.target_speed)
	  {
	    now_count_f = now_count_f - (now_count_f -
					 accel_info.target_speed) * mid;
	    now_count = (uint16_t)(now_count_f + 0.5);
	    table_data[i++] = (uint8_t) now_count;
	  }
	
	
	if (i == accel_steps)
	  break;
	if (i > accel_steps)
	  low_lim = mid;
	else
	  up_lim = mid;
      }
      
      /* fill stable steps */
      for (i = accel_steps; i < accel_info.total_steps; i++)
	  table_data [i] = table_data [i-1];
      
      debug_print_hex_raw (5, "send_acceleration_table: first pass:\n",
			   table_data, accel_info.total_steps);
      
      /* maybe post fix-up */
      {
	int add_count;
	
	/* count total steps in table */
	int table_total = 0;
	for (i = 0; i < accel_info.total_steps; i++)
	  table_total += table_data [i];
	
	i = 0;
	if (((table_total * accel_info.table_units) % accel_info.base_units) == 0)
	  add_count = 0;
	else
	  add_count = (accel_info.base_units -
		       ((table_total*accel_info.table_units) % accel_info.base_units))
	    / accel_info.table_units;
	
	/* add_count should not be bigger than 255 */
	if (add_count > 255) {
	  DBG (1, "send_acceleration_table: add_count limited, was: %d\n", add_count);
	  add_count = 255;
	}
	for (i = 0; i < accel_info.total_steps - 1 && add_count > 0; i++)
	  {
	    uint16_t temp_count = 255 - table_data [i];
	    temp_count = temp_count > add_count ? add_count : temp_count;
	    
	    table_data [i] += (uint8_t) temp_count;
	    add_count -= temp_count;
	  }
      }
    }
    
    debug_print_hex_raw (5, "send_acceleration_table: fixed up:\n",
			 table_data, accel_info.total_steps);
    
    /* decrease all by one ... */
    for (i = 0; i < accel_info.total_steps; i++) {
      table_data[i]--;
    }
    
    DBG (1, "send_acceleration_table: sending table %d\n", table);
    
    debug_print_hex_raw (5, "send_acceleration_table: final:\n",
			 table_data, accel_info.total_steps);
    
    status = avision_cmd (&s->av_con, &scmd, sizeof (scmd),
			  (char*) table_data, accel_info.total_steps,
			  0, 0);
    if (status != SANE_STATUS_GOOD) {
      DBG (3, "send_acceleration_table: send_data failed (%s)\n",
	   sane_strstatus (status));
    }
    
    free (table_data); table_data = 0;
    
    table++;
  } while (table < accel_info.table_count);
  
  
  return status;
}

static SANE_Status
set_window (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  SANE_Status status;
  int base_dpi_abs, base_dpi_rel;
  int transferlen;
  int paralen;
  
  int bytes_per_line;
  int line_count;
  
  struct {
    struct command_set_window cmd;
    struct command_set_window_window window;
  } cmd;
  
  DBG (1, "set_window:\n");
  
  /* plain old scanners, the C3 ASIC HP 53xx and the C6 ASIC HP 74xx
     and up do use 1200 as base - only the C5 differs */
  switch (dev->inquiry_asic_type) {
  case AV_ASIC_C5:
    base_dpi_abs = 1200;
    /* round down to the next multiple of 300 */
    base_dpi_rel = s->avdimen.hw_xres - s->avdimen.hw_xres % 300;
    if (base_dpi_rel > dev->inquiry_optical_res)
      base_dpi_rel = dev->inquiry_optical_res;
    else if (s->avdimen.hw_xres <= 150)
      base_dpi_rel = 150;
    break;
  default:
    base_dpi_abs = 1200;
    base_dpi_rel = 1200;
  }
  
  DBG (2, "set_window: base_dpi_abs: %d, base_dpi_rel: %d\n", base_dpi_abs, base_dpi_rel);
  
  /* wipe out anything */
  memset (&cmd, 0, sizeof (cmd) );
  cmd.window.descriptor.winid = AV_WINID; /* normally defined to be zero */
  
  /* optional parameter length to use */
  paralen = sizeof (cmd.window.avision) - sizeof (cmd.window.avision.type);
  
  DBG (2, "set_window: base paralen: %d\n", paralen);
  
  if (dev->hw->feature_type & AV_FUJITSU)
    paralen += sizeof (cmd.window.avision.type.fujitsu);
  else if (!dev->inquiry_new_protocol)
    paralen += sizeof (cmd.window.avision.type.old);
  else
    paralen += sizeof (cmd.window.avision.type.normal);
  
  DBG (2, "set_window: final paralen: %d\n", paralen);
  
  transferlen = sizeof (cmd.window)
    - sizeof (cmd.window.avision) + paralen;
  
  DBG (2, "set_window: transferlen: %d\n", transferlen);
  
  /* command setup */
  cmd.cmd.opc = AVISION_SCSI_SET_WINDOW;
  set_triple (cmd.cmd.transferlen, transferlen);
  set_double (cmd.window.header.desclen,
	      sizeof (cmd.window.descriptor) + paralen);
  
  /* resolution parameters */
  set_double (cmd.window.descriptor.xres, s->avdimen.hw_xres);
  set_double (cmd.window.descriptor.yres, s->avdimen.hw_yres);
  
  /* upper left corner x/y as well as width/length in inch * base_dpi
     - avdimen are world pixels */
  set_quad (cmd.window.descriptor.ulx, s->avdimen.tlx * base_dpi_abs / s->avdimen.hw_xres);
  set_quad (cmd.window.descriptor.uly, s->avdimen.tly * base_dpi_abs / s->avdimen.hw_yres);
  
  set_quad (cmd.window.descriptor.width,
	    s->avdimen.hw_pixels_per_line * base_dpi_rel / s->avdimen.hw_xres + 1);
  line_count = s->avdimen.hw_lines + 2 * s->avdimen.line_difference + s->avdimen.rear_offset;
  set_quad (cmd.window.descriptor.length,
	    line_count * base_dpi_rel / s->avdimen.hw_yres + 1);
  
  /* interlaced duplex scans are twice as long */
  if (s->avdimen.interlaced_duplex && dev->scanner_type != AV_FILM) {
    DBG (2, "set_window: interlaced duplex scan, doubled line count\n");
    line_count *= 2;
  }
  
  bytes_per_line = s->avdimen.hw_bytes_per_line;
  
  set_double (cmd.window.avision.line_width, bytes_per_line);
  set_double (cmd.window.avision.line_count, line_count);
  
  /* here go the most significant bits if bigger than 16 bit */
  if (dev->inquiry_new_protocol && !(dev->hw->feature_type & AV_FUJITSU) ) {
    DBG (2, "set_window: large data-transfer support (>16bit)!\n");
    cmd.window.avision.type.normal.line_width_msb =
      bytes_per_line >> 16;
    cmd.window.avision.type.normal.line_count_msb =
      line_count >> 16;
  }
  
  if (dev->inquiry_background_raster)
    cmd.window.avision.type.normal.background_lines = s->val[OPT_BACKGROUND].w;
  
  /* scanner should use our line-width and count */
  SET_BIT (cmd.window.avision.bitset1, 6); 
  
  /* set speed */
  cmd.window.avision.bitset1 |= s->val[OPT_SPEED].w & 0x07; /* only 3 bit */
  
  /* ADF scan? */
  DBG (3, "set_window: source mode %d source mode dim %d\n",
       s->source_mode, s->source_mode_dim);

  if (s->source_mode == AV_ADF ||
      s->source_mode == AV_ADF_REAR ||
      s->source_mode == AV_ADF_DUPLEX) {
    DBG (3, "set_window: filling ADF bits\n");
    SET_BIT (cmd.window.avision.bitset1, 7);

    /* normal, interlaced duplex scanners */
    if (dev->inquiry_duplex_interlaced) {
      DBG (3, "set_window: interlaced duplex type\n");
      if (s->source_mode == AV_ADF_REAR) {
        SET_BIT(cmd.window.avision.type.normal.bitset3, 3); /* 0x08 */
      }
      if (s->source_mode == AV_ADF_DUPLEX) {
        SET_BIT(cmd.window.avision.type.normal.bitset3, 4); /* 0x10 */
      }
    }
    else if (s->source_mode == AV_ADF_DUPLEX) /* HP 2-pass duplex */
    {
      DBG (3, "set_window: non-interlaced duplex type (HP)\n");
      SET_BIT(cmd.window.avision.type.normal.bitset3, 0); /* DPLX 0x01 */
      if (s->val[OPT_ADF_FLIP].w)
        SET_BIT(cmd.window.avision.type.normal.bitset3, 1); /* FLIP 0x02 */
      SET_BIT(cmd.window.avision.type.normal.bitset3, 2); /* MIRR 0x04 */
    }
  }
  
  if (s->val[OPT_PAPERLEN].w != SANE_FALSE) {
     set_double (cmd.window.descriptor.paper_length, (int)((double)30.0*1200));
  }

  if ( !(dev->hw->feature_type & AV_FUJITSU) )
    { 
      /* quality scan option switch */
      if (s->val[OPT_QSCAN].w == SANE_TRUE) {
	SET_BIT (cmd.window.avision.type.normal.bitset2, 4);
      }
  
      /* quality calibration option switch (inverted! if set == speed) */
      if (s->val[OPT_QCALIB].w == SANE_FALSE) {
	SET_BIT (cmd.window.avision.type.normal.bitset2, 3);
      }
      
      /* transparency option switch */
      if (s->source_mode_dim == AV_TRANSPARENT_DIM) {
	SET_BIT (cmd.window.avision.type.normal.bitset2, 7);
      }
      
      if (dev->scanner_type == AV_FILM) {
	/* TODO: wire to IR exposure option? */
	cmd.window.avision.type.normal.ir_exposure_time = 100;
	set_double (cmd.window.avision.type.normal.r_exposure_time, s->val[OPT_EXPOSURE].w);
	set_double (cmd.window.avision.type.normal.g_exposure_time, s->val[OPT_EXPOSURE].w);
	set_double (cmd.window.avision.type.normal.b_exposure_time, s->val[OPT_EXPOSURE].w);
	
	if (s->val[OPT_IR].w)
	  cmd.window.avision.type.normal.bitset3 |= (1 << 0);
	
	if (s->val[OPT_MULTISAMPLE].w)
	  cmd.window.avision.type.normal.bitset3 |= (1 << 1);
      }
    }
  
  /* fixed values */
  cmd.window.descriptor.padding_and_bitset = 3;
  cmd.window.descriptor.vendor_specific = 0xFF;
  cmd.window.descriptor.paralen = paralen; /* R² was: 9, later 14 */

  /* This is normally unsupported by Avision scanners, and we do this
     via the gamma table - which works for all devices ... */
  cmd.window.descriptor.threshold = 128;
  cmd.window.descriptor.brightness = 128; 
  cmd.window.descriptor.contrast = 128;
  cmd.window.avision.highlight = 0xFF;
  cmd.window.avision.shadow = 0x00;

  /* mode dependant settings */
  switch (s->c_mode)
    {
    case AV_THRESHOLDED:
      cmd.window.descriptor.bpc = 1;
      cmd.window.descriptor.image_comp = 0;
      break;
    
    case AV_DITHERED:
      cmd.window.descriptor.bpc = 1;
      cmd.window.descriptor.image_comp = 1;
      break;
      
    case AV_GRAYSCALE:
      cmd.window.descriptor.bpc = 8;
      cmd.window.descriptor.image_comp = 2;
      break;
      
    case AV_GRAYSCALE12:
      cmd.window.descriptor.bpc = 12;
      cmd.window.descriptor.image_comp = 2;
      break;

    case AV_GRAYSCALE16:
      cmd.window.descriptor.bpc = 16;
      cmd.window.descriptor.image_comp = 2;
      break;
      
    case AV_TRUECOLOR:
      cmd.window.descriptor.bpc = 8;
      cmd.window.descriptor.image_comp = 5;
      break;

    case AV_TRUECOLOR12:
      cmd.window.descriptor.bpc = 12;
      cmd.window.descriptor.image_comp = 5;
      break;

    case AV_TRUECOLOR16:
      cmd.window.descriptor.bpc = 16;
      cmd.window.descriptor.image_comp = 5;
      break;
      
    default:
      DBG (1, "Invalid mode. %d\n", s->c_mode);
      return SANE_STATUS_INVAL;
    }

  if (color_mode_is_color (s->c_mode)) {
    cmd.window.avision.bitset1 |= AVISION_FILTER_RGB;
  }
  else {
    if (dev->hw->feature_type & AV_FASTER_WITH_FILTER)
      cmd.window.avision.bitset1 |= AVISION_FILTER_GREEN;
    else if (dev->hw->feature_type & AV_USE_GRAY_FILTER)
      cmd.window.avision.bitset1 |= AVISION_FILTER_GRAY;
    else
      cmd.window.avision.bitset1 |= AVISION_FILTER_NONE;
  }
  
  debug_print_window_descriptor (5, "set_window", &(cmd.window));
  
  DBG (3, "set_window: sending command. Bytes: %d\n", transferlen);
  status = avision_cmd (&s->av_con, &cmd, sizeof (cmd.cmd),
			&(cmd.window), transferlen, 0, 0);
  
  return status;
}

static SANE_Status
get_background_raster (Avision_Scanner* s)
{
  const int debug = 0;

  Avision_Device* dev = s->hw;
  SANE_Status status;
  
  struct command_read rcmd;
  size_t size;
  int bytes_per_line, i;
  const int bpp = color_mode_is_color (s->c_mode) ? 3 : 1;
  const int lines = s->val[OPT_BACKGROUND].w * (s->avdimen.interlaced_duplex ? 2 : 1);
  
  uint8_t* background = NULL;

  DBG (1, "get_background_raster:\n");

  if (lines == 0) {
    DBG (1, "get_background_raster: no background requested\n");
    return SANE_STATUS_GOOD;
  }
  
  /* full width, always :-(, duplex *2 for front and rear */
  bytes_per_line = dev->inquiry_background_raster_pixel *
    s->avdimen.hw_xres / dev->inquiry_optical_res;
  bytes_per_line *= bpp;
  
  DBG (3, "get_background_raster: native raster pixels: %d, raster bytes_per_line: %d\n",
       dev->inquiry_background_raster_pixel, bytes_per_line);
  
  /* according to spec only 8-bit gray or color, TODO: test for bi-level scans */
  size = bytes_per_line * lines;

  DBG (3, "get_background_raster: buffer size: %ld\n", (long)size);
  
  background = s->background_raster = realloc (s->background_raster, size);
  if (!background)
    return SANE_STATUS_NO_MEM;

  memset (&rcmd, 0, sizeof (rcmd));
  rcmd.opc = AVISION_SCSI_READ;
  rcmd.datatypecode = 0x9b; /* get background raster */
  set_double (rcmd.datatypequal, s->hw->data_dq);
  
  /* Ok, well - this part is very messy. The AV122 and DM152 appear to
     contain differently buggy ASICs. The only combination I found to
     at least get a correct front raster out of them is to read it
     line by line and then every second line appears to be valid front
     data, ... */
  
  /* read the raster data */
  for (i = 0; i < lines;)
    {
      uint8_t* dst_raster = background + bytes_per_line * i;
      /* read stripe by stripe, or all in one chunk */
      size_t this_read, read_size;
      int this_lines;
      
      if (dev->hw->feature_type & AV_2ND_LINE_INTERLACED) {
	if (dev->hw->feature_type & AV_BACKGROUND_QUIRK)
	  this_lines = 1;
	else
	  this_lines = lines;
      }
      else {
	this_lines = s->val[OPT_BACKGROUND].w;
      }
      this_read = bytes_per_line * this_lines;
      
      DBG (3, "get_background_raster: line: %d, lines: %d, %lu bytes\n",
	   i, this_lines, (u_long) this_read);

      set_triple (rcmd.transferlen, this_read);
      
      read_size = this_read;
      status = avision_cmd (&s->av_con, &rcmd, sizeof (rcmd), 0, 0, dst_raster, &read_size);
      if (status != SANE_STATUS_GOOD || read_size != this_read) {
	DBG (1, "get_background_raster: read raster failed (%s)\n",
	     sane_strstatus (status) );
	return status;
      }
      
      i += this_lines;
    }
  
  /* dump raw result while debugging */
  if (debug)
    {
      FILE* f = NULL;
      f = fopen ("background-raw.pnm", "w");
      
      write_pnm_header (f, (color_mode_is_color (s->c_mode) ? AV_TRUECOLOR : AV_GRAYSCALE), 8,
			bytes_per_line / bpp, lines);
      
      fwrite (background, 1, bytes_per_line * lines, f);
      fclose (f);
    }

  /* line-pack - move to unified processing flow, later */
  if (dev->inquiry_needs_line_pack)
    { 
      /* TODO: add 16bit per sample code? */
      int l, p;
      
      uint8_t* tmp_data = malloc (bytes_per_line);
      for (l = 0; l < lines; ++l)
	{
	  uint8_t* out_data = tmp_data;
	  uint8_t* r_ptr = background + (bytes_per_line * l);
	  uint8_t* g_ptr = r_ptr + bytes_per_line / bpp;
	  uint8_t* b_ptr = g_ptr + bytes_per_line / bpp;
	  
	  for (p = 0; p < bytes_per_line;) {
	    out_data [p++] = *(r_ptr++);
	    out_data [p++] = *(g_ptr++);
	    out_data [p++] = *(b_ptr++);
	  }
	  
	  memcpy (background + (bytes_per_line * l), tmp_data, bytes_per_line);
	}
      
      free (tmp_data);
    } /* end line pack */
  
  /* deinterlace? */
  if (s->avdimen.interlaced_duplex && dev->hw->feature_type & AV_2ND_LINE_INTERLACED)
    {
      uint8_t* deinterlaced = malloc (size * 2);
      if (!deinterlaced)
	return SANE_STATUS_NO_MEM;
      
      for (i = 0; i < lines; ++i)
	{
	  int dst_i = i / 2 + (i % 2) * (lines / 2);
	  uint8_t* dst_raster; /* just no C99 in SANE :-( */
	  uint8_t* src_raster;
	  
	  /* for the quirky devices and some resolutions the interlacing differs */
	  if (dev->hw->feature_type & AV_BACKGROUND_QUIRK && s->avdimen.hw_xres >= 150)
	    dst_i = i / 2 + ((i+1) % 2) * (lines / 2);
	  
	  dst_raster = deinterlaced + bytes_per_line * dst_i;
	  src_raster = background + bytes_per_line * i;
	  
	  DBG(3, "get_background_raster: deinterlaced %d -> %d\n", i, dst_i);
	  memcpy(dst_raster, src_raster, bytes_per_line);
	}
      
      free (background);
      background = s->background_raster = deinterlaced;
    }
  
  /* dump raw result while debugging */
  for (i = 0; debug && i < (s->avdimen.interlaced_duplex ? 2 : 1); ++i)
    {
    FILE* f = NULL;
    uint8_t* raster = background;
    if (i == 0) {
      f = fopen ("background.pnm", "w");
    }
    else {
      f = fopen ("background-rear.pnm", "w");
      raster += bytes_per_line * s->val[OPT_BACKGROUND].w;
    }
    
    write_pnm_header (f, (color_mode_is_color (s->c_mode) ? AV_TRUECOLOR : AV_GRAYSCALE), 8,
		      bytes_per_line / bpp, s->val[OPT_BACKGROUND].w);
	
    fwrite (raster, 1, bytes_per_line * s->val[OPT_BACKGROUND].w, f);
    fclose (f);
  }
  
  /* crop from full-width scanlines to scan window */
  {
    uint8_t *dst_ptr, *src_ptr;
    dst_ptr = background;
    src_ptr = background + s->avdimen.tlx * bpp;
    for (i = 0; i < lines; ++i)
      {
	memmove (dst_ptr, src_ptr, s->avdimen.hw_bytes_per_line);
	dst_ptr += s->avdimen.hw_bytes_per_line;
	src_ptr += bytes_per_line;
      }
  }
  
  /* soft-scale - move to unified processing flow, later */
  if (s->avdimen.hw_xres != s->avdimen.xres)
    {
      const uint8_t* out_data = background;
      uint8_t* dst = background;
      
      int l;
      for (l = 0; l < lines; ++l)
	{
	  const int hwbpl = s->avdimen.hw_bytes_per_line;
	  const int sy = l;
	  
	  int x;
	  for (x = 0; x < s->params.pixels_per_line; ++x)
	    {
	      const double bx = (-1.0 + s->avdimen.hw_pixels_per_line) * x / s->params.pixels_per_line;
	      const int sx = (int)floor(bx);
	      const int xdist = (int) ((bx - sx) * 256);
	      const int sxx = sx + 1;
	      
	      switch (bpp) {
	      case 1:
		{
		  uint8_t v = 
		    ( out_data [sy*hwbpl  + sx ] * (256-xdist) +
		      out_data [sy*hwbpl  + sxx] * xdist
		    ) / (256);
		  *dst++ = v;
		}
		break;
		
	      case 3:
		{
		  int c;
		  for (c = 0; c < 3; ++c)
		    {
		      uint8_t v = 
			( out_data [sy*hwbpl  + sx*3  + c] * (256-xdist) +
			  out_data [sy*hwbpl  + sxx*3 + c] * xdist
			  ) / (256);
		      *dst++ = v;
		    }
		}
		break;
	      }
	    }
	}
    }
  
  /* dump final result while debugging */
  if (debug) {
    for (i = 0; i < (s->avdimen.interlaced_duplex ? 2 : 1); ++i)
      {
	FILE* f = NULL;
	uint8_t* raster = background;
	if (i == 0) {
	  f = fopen ("background-final.pnm", "w");
	}
	else {
	  f = fopen ("background-final-rear.pnm", "w");
	  raster += s->params.bytes_per_line * s->val[OPT_BACKGROUND].w;
	}
	
	write_pnm_header (f, (color_mode_is_color (s->c_mode) ? AV_TRUECOLOR : AV_GRAYSCALE), 8,
			  s->params.bytes_per_line / bpp, s->val[OPT_BACKGROUND].w);
	
	fwrite (raster, 1, s->params.bytes_per_line * s->val[OPT_BACKGROUND].w, f);
	fclose (f);
      }
  }
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
reserve_unit (Avision_Scanner* s)
{
  char cmd[] =
    {AVISION_SCSI_RESERVE_UNIT, 0, 0, 0, 0, 0};
  SANE_Status status;
  
  DBG (1, "reserve_unit:\n");
  
  status = avision_cmd (&s->av_con, cmd, sizeof (cmd), 0, 0, 0, 0);
  return status;
}

static SANE_Status
release_unit (Avision_Scanner* s, int type)
{
  char cmd[] =
    {AVISION_SCSI_RELEASE_UNIT, 0, 0, 0, 0, 0};
  SANE_Status status;
  
  DBG (1, "release unit: type: %d\n", type);
  cmd[5] = type; /* latest scanners also allow 1: release paper and 2: end job */
  status = avision_cmd (&s->av_con, cmd, sizeof (cmd), 0, 0, 0, 0);
  return status;
}

/* Check if a sheet is present. */
static SANE_Status
media_check (Avision_Scanner* s)
{
  char cmd[] = {AVISION_SCSI_MEDIA_CHECK, 0, 0, 0, 1, 0}; /* 1, 4 */
  SANE_Status status;
  uint8_t result[1]; /* 4 */
  size_t size = sizeof(result);
  
  status = avision_cmd (&s->av_con, cmd, sizeof (cmd),
			0, 0, result, &size);
  
  debug_print_raw (5, "media_check: result\n", result, size);
  
  if (status == SANE_STATUS_GOOD) {
    if (!(result[0] & 0x1))
      status = SANE_STATUS_NO_DOCS;
  }
  
  return status;
}

#if 0 /* unused */
static SANE_Status
flush_media (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  SANE_Status status;
  
  if (s->source_mode_dim == AV_ADF_DIM && dev->inquiry_batch_scan)
    {
      DBG (1, "flush_media: flushing pages out of batch scanner\n");
      do {
	status = media_check (s);
	if (status == SANE_STATUS_GOOD) {
	  SANE_Status status2 = reserve_unit (s);
	  DBG (1, "flush_media: reserve status: %d\n", status2);
	  status2 = release_unit (s, 0);
	  DBG (1, "flush_media: release status: %d\n", status2);
	}
      } while (status == SANE_STATUS_GOOD);
    } 
  return SANE_STATUS_GOOD;
}
#endif /* 0 - unused */

static SANE_Status
object_position (Avision_Scanner* s, uint8_t position)
{
  SANE_Status status;
  
  uint8_t cmd [10];
  
  memset (cmd, 0, sizeof (cmd));
  cmd[0] = AVISION_SCSI_OBJECT_POSITION;
  cmd[1] = position;
  
  DBG (1, "object_position: %d\n", position);
  
  status = avision_cmd (&s->av_con, cmd, sizeof(cmd), 0, 0, 0, 0);
  return status;
}

static SANE_Status
start_scan (Avision_Scanner* s)
{
  struct command_scan cmd;
  
  size_t size = sizeof (cmd);
  
  DBG (3, "start_scan:\n");
  
  memset (&cmd, 0, sizeof (cmd));
  cmd.opc = AVISION_SCSI_SCAN;
  cmd.transferlen = 1;

  /* AV610C2 in ADF preview mode does not detect the page end (...) */
  if (s->val[OPT_PREVIEW].w == SANE_TRUE && s->hw->inquiry_asic_type != AV_ASIC_C7) {
    SET_BIT(cmd.bitset1,6);
  }

  if (s->val[OPT_QSCAN].w == SANE_TRUE) {
    SET_BIT(cmd.bitset1,7);
  }
  
  DBG (3, "start_scan: sending command. Bytes: %lu\n", (u_long) size);
  return avision_cmd (&s->av_con, &cmd, size, 0, 0, 0, 0);
}

static SANE_Status
do_eof (Avision_Scanner *s)
{
  int exit_status;
  
  DBG (3, "do_eof:\n");

  /* we do not scan anymore */
  s->prepared = s->scanning = SANE_FALSE;
  
  /* we can now mark the rear data as valid */
  if (s->avdimen.interlaced_duplex ||
      (s->hw->hw->feature_type & AV_ADF_FLIPPING_DUPLEX && s->source_mode == AV_ADF_DUPLEX)) {
    DBG (3, "do_eof: toggling duplex rear data valid\n");
    s->duplex_rear_valid = !s->duplex_rear_valid;
    DBG (3, "do_eof: duplex rear data valid: %x\n",
	 s->duplex_rear_valid);
  }
  
  if (s->read_fds >= 0) {
    close (s->read_fds);
    s->read_fds = -1;
  }
  
  /* join our processes - without a wait() you will produce zombies
     (defunct children) */
  sanei_thread_waitpid (s->reader_pid, &exit_status);
  s->reader_pid = -1;

  DBG (3, "do_eof: returning %d\n", exit_status);
  return (SANE_Status)exit_status;
}

static SANE_Status
do_cancel (Avision_Scanner* s)
{
  DBG (3, "do_cancel:\n");
  
  s->prepared = s->scanning = SANE_FALSE;
  s->duplex_rear_valid = SANE_FALSE;
  s->page = 0;
  
  if (s->reader_pid != -1) {
    int exit_status;
    
    /* ensure child knows it's time to stop: */
    sanei_thread_kill (s->reader_pid);
    sanei_thread_waitpid (s->reader_pid, &exit_status);
    s->reader_pid = -1;
  }

  return SANE_STATUS_CANCELLED;
}

static SANE_Status
read_data (Avision_Scanner* s, SANE_Byte* buf, size_t* count)
{
  struct command_read rcmd;
  SANE_Status status;

  DBG (9, "read_data: %lu\n", (u_long) *count);
  
  memset (&rcmd, 0, sizeof (rcmd));
  
  rcmd.opc = AVISION_SCSI_READ;
  rcmd.datatypecode = 0x00; /* read image data */
  set_double (rcmd.datatypequal, s->hw->data_dq);
  set_triple (rcmd.transferlen, *count);
  
  status = avision_cmd (&s->av_con, &rcmd, sizeof (rcmd), 0, 0, buf, count);
  
  return status;
}

static SANE_Status
init_options (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  int i;
  
  DBG (3, "init_options:\n");
  
  memset (s->opt, 0, sizeof (s->opt));
  memset (s->val, 0, sizeof (s->val));

  for (i = 0; i < NUM_OPTIONS; ++ i) {
    s->opt[i].size = sizeof (SANE_Word);
    s->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }
  
  /* Init the SANE option from the scanner inquiry data */
  
  switch (dev->inquiry_asic_type) {
    case AV_ASIC_C2:
      dev->dpi_range.min = 100;
      break;
    case AV_ASIC_C5:
      dev->dpi_range.min = 80;
      break;
    case AV_ASIC_C6: /* TODO: AV610 in ADF mode does not scan less than 180 or so */
      dev->dpi_range.min = 50;
      break;
    case AV_ASIC_C7: /* AV610C2 empirically tested out */
      dev->dpi_range.min = 75;
      break;
    default:
      dev->dpi_range.min = 50;
  }
  DBG (1, "init_options: dpi_range.min set to %d\n", dev->dpi_range.min);

  dev->dpi_range.quant = 1; /* any, including 72, 144, etc. */
  dev->dpi_range.max = dev->inquiry_max_res;
  
  dev->speed_range.min = (SANE_Int)0;
  dev->speed_range.max = (SANE_Int)4;
  dev->speed_range.quant = (SANE_Int)1;
  
  s->opt[OPT_NUM_OPTS].name = "";
  s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].desc = "";
  s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  s->opt[OPT_NUM_OPTS].size = sizeof(SANE_TYPE_INT);
  s->val[OPT_NUM_OPTS].w = NUM_OPTIONS;
  
  /* "Mode" group: */
  s->opt[OPT_MODE_GROUP].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_MODE_GROUP].desc = ""; /* for groups only title and type are valid */
  s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MODE_GROUP].cap = 0;
  s->opt[OPT_MODE_GROUP].size = 0;
  s->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;
  
  /* color mode */
  s->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  s->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  s->opt[OPT_MODE].type = SANE_TYPE_STRING;
  s->opt[OPT_MODE].size = max_string_size (dev->color_list);
  s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_MODE].constraint.string_list = dev->color_list;
  s->val[OPT_MODE].s = strdup (dev->color_list[dev->color_list_default]);
  s->c_mode = match_color_mode (dev, s->val[OPT_MODE].s);
  
  /* source mode */
  s->opt[OPT_SOURCE].name = SANE_NAME_SCAN_SOURCE;
  s->opt[OPT_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
  s->opt[OPT_SOURCE].desc = SANE_DESC_SCAN_SOURCE;
  s->opt[OPT_SOURCE].type = SANE_TYPE_STRING;
  s->opt[OPT_SOURCE].size = max_string_size(dev->source_list);
  s->opt[OPT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_SOURCE].constraint.string_list = &dev->source_list[0];
  s->val[OPT_SOURCE].s = strdup(dev->source_list[0]);
  s->source_mode = match_source_mode (dev, s->val[OPT_SOURCE].s);
  s->source_mode_dim = match_source_mode_dim (s->source_mode);

  dev->x_range.max = SANE_FIX ( (int)dev->inquiry_x_ranges[s->source_mode_dim]);
  dev->x_range.quant = 0;
  dev->y_range.max = SANE_FIX ( (int)dev->inquiry_y_ranges[s->source_mode_dim]);
  dev->y_range.quant = 0;

  /* resolution */
  s->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_RESOLUTION].constraint.range = &dev->dpi_range;
  s->val[OPT_RESOLUTION].w = OPT_RESOLUTION_DEFAULT;

  /* preview */
  s->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  s->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  s->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  s->opt[OPT_PREVIEW].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  s->val[OPT_PREVIEW].w = 0;

  /* speed option */
  s->opt[OPT_SPEED].name  = SANE_NAME_SCAN_SPEED;
  s->opt[OPT_SPEED].title = SANE_TITLE_SCAN_SPEED;
  s->opt[OPT_SPEED].desc  = SANE_DESC_SCAN_SPEED;
  s->opt[OPT_SPEED].type  = SANE_TYPE_INT;
  s->opt[OPT_SPEED].constraint_type  = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_SPEED].constraint.range = &dev->speed_range; 
  s->val[OPT_SPEED].w = 0;
  if (dev->scanner_type == AV_SHEETFEED)
    s->opt[OPT_SPEED].cap |= SANE_CAP_INACTIVE;

  /* "Geometry" group: */
  s->opt[OPT_GEOMETRY_GROUP].title = "Geometry";
  s->opt[OPT_GEOMETRY_GROUP].desc = ""; /* for groups only title and type are valid */
  s->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_GEOMETRY_GROUP].size = 0;
  s->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* top-left x */
  s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  s->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_X].constraint.range = &dev->x_range;
  s->val[OPT_TL_X].w = 0;

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &dev->y_range;
  s->val[OPT_TL_Y].w = 0;

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &dev->x_range;
  s->val[OPT_BR_X].w = dev->x_range.max;

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &dev->y_range;
  s->val[OPT_BR_Y].w = dev->y_range.max;
  
  /* overscan top */
  s->opt[OPT_OVERSCAN_TOP].name = "overscan-top";
  s->opt[OPT_OVERSCAN_TOP].title = "Overscan top";
  s->opt[OPT_OVERSCAN_TOP].desc = "The top overscan controls the additional area to scan before the paper is detected.";
  s->opt[OPT_OVERSCAN_TOP].type = SANE_TYPE_FIXED;
  s->opt[OPT_OVERSCAN_TOP].unit = SANE_UNIT_MM;
  s->opt[OPT_OVERSCAN_TOP].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_OVERSCAN_TOP].constraint.range = &overscan_range;
  s->val[OPT_OVERSCAN_TOP].w = SANE_FIX(0);
  
  /* overscan bottom */
  s->opt[OPT_OVERSCAN_BOTTOM].name = "overscan-bottom";
  s->opt[OPT_OVERSCAN_BOTTOM].title = "Overscan bottom";
  s->opt[OPT_OVERSCAN_BOTTOM].desc = "The bottom overscan controls the additional area to scan after the paper end is detected.";
  s->opt[OPT_OVERSCAN_BOTTOM].type = SANE_TYPE_FIXED;
  s->opt[OPT_OVERSCAN_BOTTOM].unit = SANE_UNIT_MM;
  s->opt[OPT_OVERSCAN_BOTTOM].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_OVERSCAN_BOTTOM].constraint.range = &overscan_range;
  s->val[OPT_OVERSCAN_BOTTOM].w = SANE_FIX(0);
  
  if (!dev->inquiry_tune_scan_length)
    s->opt[OPT_OVERSCAN_TOP].cap |= SANE_CAP_INACTIVE;
  if (!dev->inquiry_tune_scan_length)
    s->opt[OPT_OVERSCAN_BOTTOM].cap |= SANE_CAP_INACTIVE;
  
  /* background raster */
  s->opt[OPT_BACKGROUND].name = "background-lines";
  s->opt[OPT_BACKGROUND].title = "Background raster lines";
  s->opt[OPT_BACKGROUND].desc = "The background raster controls the additional background lines to scan before the paper is feed through the scanner.";
  s->opt[OPT_BACKGROUND].type = SANE_TYPE_INT;
  s->opt[OPT_BACKGROUND].unit = SANE_UNIT_PIXEL;
  s->opt[OPT_BACKGROUND].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BACKGROUND].constraint.range = &background_range;
  s->val[OPT_BACKGROUND].w = 0;
  
  if (!dev->inquiry_background_raster) {
    s->opt[OPT_BACKGROUND].cap |= SANE_CAP_INACTIVE;
  }
  
  /* "Enhancement" group: */
  s->opt[OPT_ENHANCEMENT_GROUP].title = "Enhancement";
  s->opt[OPT_ENHANCEMENT_GROUP].desc = ""; /* for groups only title and type are valid */
  s->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ENHANCEMENT_GROUP].cap = 0;
  s->opt[OPT_ENHANCEMENT_GROUP].size = 0;
  s->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* brightness */
  s->opt[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].type = SANE_TYPE_FIXED;
  if (disable_gamma_table)
    s->opt[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_BRIGHTNESS].unit = SANE_UNIT_PERCENT;
  s->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BRIGHTNESS].constraint.range = &percentage_range;
  s->val[OPT_BRIGHTNESS].w = SANE_FIX(0);

  /* contrast */
  s->opt[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  s->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  s->opt[OPT_CONTRAST].desc = SANE_DESC_CONTRAST;
  s->opt[OPT_CONTRAST].type = SANE_TYPE_FIXED;
  if (disable_gamma_table)
    s->opt[OPT_CONTRAST].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_CONTRAST].unit = SANE_UNIT_PERCENT;
  s->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CONTRAST].constraint.range = &percentage_range;
  s->val[OPT_CONTRAST].w = SANE_FIX(0);

  /* Quality Scan */
  s->opt[OPT_QSCAN].name   = "quality-scan";
  s->opt[OPT_QSCAN].title  = "Quality scan";
  s->opt[OPT_QSCAN].desc   = "Turn on quality scanning (slower but better).";
  s->opt[OPT_QSCAN].type   = SANE_TYPE_BOOL;
  s->opt[OPT_QSCAN].unit   = SANE_UNIT_NONE;
  s->val[OPT_QSCAN].w      = SANE_TRUE;

  /* Quality Calibration */
  s->opt[OPT_QCALIB].name  = SANE_NAME_QUALITY_CAL;
  s->opt[OPT_QCALIB].title = SANE_TITLE_QUALITY_CAL;
  s->opt[OPT_QCALIB].desc  = SANE_DESC_QUALITY_CAL;
  s->opt[OPT_QCALIB].type  = SANE_TYPE_BOOL;
  s->opt[OPT_QCALIB].unit  = SANE_UNIT_NONE;
  s->val[OPT_QCALIB].w     = SANE_TRUE;

  /* gray scale gamma vector */
  s->opt[OPT_GAMMA_VECTOR].name = SANE_NAME_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR].title = SANE_TITLE_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR].desc = SANE_DESC_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GAMMA_VECTOR].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR].size = 256 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR].wa = &s->gamma_table[0][0];

  /* red gamma vector */
  s->opt[OPT_GAMMA_VECTOR_R].name = SANE_NAME_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].title = SANE_TITLE_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].desc = SANE_DESC_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GAMMA_VECTOR_R].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_R].size = 256 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR_R].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_R].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR_R].wa = &s->gamma_table[1][0];

  /* green gamma vector */
  s->opt[OPT_GAMMA_VECTOR_G].name = SANE_NAME_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].title = SANE_TITLE_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].desc = SANE_DESC_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GAMMA_VECTOR_G].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_G].size = 256 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR_G].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_G].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR_G].wa = &s->gamma_table[2][0];

  /* blue gamma vector */
  s->opt[OPT_GAMMA_VECTOR_B].name = SANE_NAME_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].title = SANE_TITLE_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].desc = SANE_DESC_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GAMMA_VECTOR_B].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_B].size = 256 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR_B].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_B].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR_B].wa = &s->gamma_table[3][0];
 
  if (!disable_gamma_table)
  {
    if (color_mode_is_color (s->c_mode)) {
      s->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
      s->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
      s->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
    }
    else {
      s->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
    }
  }
  
  /* exposure */
  s->opt[OPT_EXPOSURE].name = "exposure";
  s->opt[OPT_EXPOSURE].title = "Exposure";
  s->opt[OPT_EXPOSURE].desc = "Manual exposure adjustment.";
  s->opt[OPT_EXPOSURE].type = SANE_TYPE_INT;
  s->opt[OPT_EXPOSURE].unit = SANE_UNIT_PERCENT;
  s->opt[OPT_EXPOSURE].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_EXPOSURE].constraint.range = &exposure_range;
  s->val[OPT_EXPOSURE].w = 100;
  
  if (!dev->inquiry_exposure_control) {
    s->opt[OPT_EXPOSURE].cap |= SANE_CAP_INACTIVE;
  }

  /* Multi sample */
  s->opt[OPT_MULTISAMPLE].name  = "multi-sample";
  s->opt[OPT_MULTISAMPLE].title = "Multi-sample";
  s->opt[OPT_MULTISAMPLE].desc  = "Enable multi-sample scan mode.";
  s->opt[OPT_MULTISAMPLE].type  = SANE_TYPE_BOOL;
  s->opt[OPT_MULTISAMPLE].unit  = SANE_UNIT_NONE;
  s->val[OPT_MULTISAMPLE].w     = SANE_FALSE;
  
  /* TODO: No idea how to detect, assume exposure control devices are
     new enough to support this, for now. -ReneR */
  if (!dev->inquiry_exposure_control) {
    s->opt[OPT_MULTISAMPLE].cap |= SANE_CAP_INACTIVE;
  }

  /* Infra-red */
  s->opt[OPT_IR].name  = "infra-red";
  s->opt[OPT_IR].title = "Infra-red";
  s->opt[OPT_IR].desc  = "Enable infra-red scan mode.";
  s->opt[OPT_IR].type  = SANE_TYPE_BOOL;
  s->opt[OPT_IR].unit  = SANE_UNIT_NONE;
  s->val[OPT_IR].w     = SANE_FALSE;
  
  /* TODO: No idea how to detect, assume exposure control devices are
     new enough to support this, for now. -ReneR */
  if (!dev->inquiry_exposure_control) {
    s->opt[OPT_IR].cap |= SANE_CAP_INACTIVE;
  }
  
  /* "MISC" group: */
  s->opt[OPT_MISC_GROUP].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_MISC_GROUP].desc = ""; /* for groups only title and type are valid */
  s->opt[OPT_MISC_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MISC_GROUP].cap = 0;
  s->opt[OPT_MISC_GROUP].size = 0;
  s->opt[OPT_MISC_GROUP].constraint_type = SANE_CONSTRAINT_NONE;
  
  /* film holder control */
  if (dev->scanner_type != AV_FILM)
    s->opt[OPT_FRAME].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_FRAME].name = SANE_NAME_FRAME;
  s->opt[OPT_FRAME].title = SANE_TITLE_FRAME;
  s->opt[OPT_FRAME].desc = SANE_DESC_FRAME;
  s->opt[OPT_FRAME].type = SANE_TYPE_INT;
  s->opt[OPT_FRAME].unit = SANE_UNIT_NONE;
  s->opt[OPT_FRAME].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_FRAME].constraint.range = &dev->frame_range;
  s->val[OPT_FRAME].w = dev->current_frame;

  /* power save time */
  if (!dev->inquiry_power_save_time)
    s->opt[OPT_POWER_SAVE_TIME].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_POWER_SAVE_TIME].name = "power-save-time";
  s->opt[OPT_POWER_SAVE_TIME].title = "Power save timer control";
  s->opt[OPT_POWER_SAVE_TIME].desc = "Allows control of the scanner's power save timer, dimming or turning off the light.";
  s->opt[OPT_POWER_SAVE_TIME].type = SANE_TYPE_INT;
  s->opt[OPT_POWER_SAVE_TIME].unit = SANE_UNIT_NONE;
  s->opt[OPT_POWER_SAVE_TIME].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_POWER_SAVE_TIME].w = 0;

  /* message, like options set on the scanner, LED no. & co */
  s->opt[OPT_MESSAGE].name = "message";
  s->opt[OPT_MESSAGE].title = "message text from the scanner";
  s->opt[OPT_MESSAGE].desc = "This text contains device specific options controlled by the user on the scanner hardware.";
  s->opt[OPT_MESSAGE].type = SANE_TYPE_STRING;
  s->opt[OPT_MESSAGE].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
  s->opt[OPT_MESSAGE].size = 129;
  s->opt[OPT_MESSAGE].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_MESSAGE].s = malloc(s->opt[OPT_MESSAGE].size);
  s->val[OPT_MESSAGE].s[0] = 0;
  
  /* NVRAM */
  s->opt[OPT_NVRAM].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
  if (!dev->inquiry_nvram_read)
    s->opt[OPT_NVRAM].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_NVRAM].name = "nvram-values";
  s->opt[OPT_NVRAM].title = "Obtain NVRAM values";
  s->opt[OPT_NVRAM].desc = "Allows access obtaining the scanner's NVRAM values as pretty printed text.";
  s->opt[OPT_NVRAM].type = SANE_TYPE_STRING;
  s->opt[OPT_NVRAM].unit = SANE_UNIT_NONE;
  s->opt[OPT_NVRAM].size = 1024;
  s->opt[OPT_NVRAM].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_NVRAM].s = malloc(s->opt[OPT_NVRAM].size);
  s->val[OPT_NVRAM].s[0] = 0;
  
  /* paper_length */
  s->opt[OPT_PAPERLEN].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
  if (!dev->inquiry_paper_length)
    s->opt[OPT_PAPERLEN].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_PAPERLEN].name  = "paper-length";
  s->opt[OPT_PAPERLEN].title = "Use paper length";
  s->opt[OPT_PAPERLEN].desc  = "Newer scanners can utilize this paper length to detect double feeds.  However some others (DM152) can get confused during media flush if it is set.";
  s->opt[OPT_PAPERLEN].type  = SANE_TYPE_BOOL;
  s->opt[OPT_PAPERLEN].unit  = SANE_UNIT_NONE;
  s->opt[OPT_PAPERLEN].size = sizeof(SANE_Word);
  s->opt[OPT_PAPERLEN].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_PAPERLEN].w     = SANE_FALSE;
  
  /* ADF page flipping */
  s->opt[OPT_ADF_FLIP].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_AUTOMATIC | SANE_CAP_ADVANCED;
  if (!(s->hw->hw->feature_type & AV_ADF_FLIPPING_DUPLEX && s->source_mode == AV_ADF_DUPLEX))
    s->opt[OPT_ADF_FLIP].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_ADF_FLIP].name = "flip-page";
  s->opt[OPT_ADF_FLIP].title = "Flip document after duplex scanning";
  s->opt[OPT_ADF_FLIP].desc = "Tells page-flipping document scanners to flip the paper back to its original orientation before dropping it in the output tray.  Turning this off might make scanning a little faster if you don't care about manually flipping the pages afterwards.";
  s->opt[OPT_ADF_FLIP].type = SANE_TYPE_BOOL;
  s->opt[OPT_ADF_FLIP].unit = SANE_UNIT_NONE;
  s->opt[OPT_ADF_FLIP].size = sizeof(SANE_Word);
  s->opt[OPT_ADF_FLIP].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_ADF_FLIP].w = SANE_TRUE;

  return SANE_STATUS_GOOD;
}

/* This function is executed as a child process. The reason this is
   executed as a subprocess is because some (most?) generic SCSI
   interfaces block a SCSI request until it has completed. With a
   subprocess, we can let it block waiting for the request to finish
   while the main process can go about to do more important things
   (such as recognizing when the user presses a cancel button).

   WARNING: Since this is executed as a subprocess, it's NOT possible
   to update any of the variables in the main process (in particular
   the scanner state cannot be updated).  */

static int
reader_process (void *data)
{
  struct Avision_Scanner *s = (struct Avision_Scanner *) data;
  int fd = s->write_fds;

  Avision_Device* dev = s->hw;
  
  SANE_Status status;
  SANE_Status exit_status = SANE_STATUS_GOOD;
  sigset_t sigterm_set;
  sigset_t ignore_set;
  struct SIGACTION act;
  
  FILE* fp;
  FILE* rear_fp = 0; /* used to store the deinterlaced rear data */
  FILE* raw_fp = 0; /* used to write the RAW image data for debugging */
  
  /* the complex params */
  unsigned int lines_per_stripe;
  unsigned int lines_per_output;
  unsigned int max_bytes_per_read;
  
  SANE_Bool gray_mode;
  
  /* the simple params for the data reader */
  int hw_line = 0;
  int line = 0;

  unsigned int stripe_size;
  unsigned int stripe_fill;
  unsigned int out_size;
  
  size_t total_size;
  size_t processed_bytes;
  
  enum {
    NONE,   /* do not de-interlace at all */
    STRIPE, /* every 2nd stripe */
    HALF,   /* the 2nd half */
    LINE    /* every 2nd line */
  } deinterlace = NONE;
  
  /* the fat strip we currently puzzle together to perform software-colorpack
     and more */
  uint8_t* stripe_data;
  /* the corrected output data */
  uint8_t* out_data;
  /* interpolation output data, one line */
  uint8_t* ip_history = 0;
  uint8_t* ip_data = 0;
  
  DBG (3, "reader_process:\n");
  
  if (sanei_thread_is_forked())
    close (s->read_fds);
  
  sigfillset (&ignore_set);
  sigdelset (&ignore_set, SIGTERM);
#if defined (__APPLE__) && defined (__MACH__)
  sigdelset (&ignore_set, SIGUSR2);
#endif
  sigprocmask (SIG_SETMASK, &ignore_set, 0);
  
  memset (&act, 0, sizeof (act));
  sigaction (SIGTERM, &act, 0);
  
  sigemptyset (&sigterm_set);
  sigaddset (&sigterm_set, SIGTERM);
  
  gray_mode = color_mode_is_shaded (s->c_mode);

  if (s->avdimen.interlaced_duplex) {
    deinterlace = STRIPE;
    
    if ( (dev->hw->feature_type & AV_NON_INTERLACED_DUPLEX_300) &&
	 (s->avdimen.hw_xres <= 300 && s->avdimen.hw_yres <= 300) )
      deinterlace = HALF;
    if (dev->hw->feature_type & AV_2ND_LINE_INTERLACED)
      deinterlace = LINE;
    
    if (dev->scanner_type == AV_FILM)
      deinterlace = LINE;
  }
  
  fp = fdopen (fd, "w");
  if (!fp)
    return SANE_STATUS_NO_MEM;
  
  /* start scan ? */
  if ((deinterlace == NONE && !((dev->hw->feature_type & AV_ADF_FLIPPING_DUPLEX) && s->source_mode == AV_ADF_DUPLEX && s->duplex_rear_valid)) ||
      (deinterlace != NONE && !s->duplex_rear_valid))
    {
      /* reserve unit - in the past we did this in open - but the
	 windows driver does reserves for each scan and some ADF
	 devices need a release for each sheet anyway ... */
      status = reserve_unit (s);
      if (status != SANE_STATUS_GOOD) {
	DBG (1, "reader_process: reserve_unit failed: %s\n",
	     sane_strstatus (status));
	return status;
      }
  
      if (dev->hw->feature_type & AV_NO_START_SCAN) {
	DBG (1, "reader_process: start_scan skipped due to device-list!\n");
      }
      else {
	status = start_scan (s);
	if (status != SANE_STATUS_GOOD) {
	  DBG (1, "reader_process: start_scan failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}
      }
      
      if (dev->hw->feature_type & AV_ACCEL_TABLE)
     /*  (s->hw->inquiry_asic_type == AV_ASIC_C6) */ {
	status = send_acceleration_table (s);
	if (status != SANE_STATUS_GOOD) {
	  DBG (1, "reader_process: send_acceleration_table failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}
      }
    }
  
  /* setup file i/o for deinterlacing scans or if we are the back page with a flipping duplexer */
  if (deinterlace != NONE ||
     (dev->hw->feature_type & AV_ADF_FLIPPING_DUPLEX && s->source_mode == AV_ADF_DUPLEX && !(s->page % 2)))
    {
      if (!s->duplex_rear_valid) { /* create new file for writing */
	DBG (3, "reader_process: opening duplex rear file for writing.\n");
	rear_fp = fopen (s->duplex_rear_fname, "w");
	if (! rear_fp) {
	  fclose (fp);
	  return SANE_STATUS_NO_MEM;
	}
      }
      else { /* open saved rear data */
	DBG (3, "reader_process: opening duplex rear file for reading.\n");
	rear_fp = fopen (s->duplex_rear_fname, "r");
	if (! rear_fp) {
	  fclose (fp);
	  return SANE_STATUS_IO_ERROR;
	}
      }
    }
  
  /* it takes quite a few lines to saturate the (USB) bus */
  lines_per_stripe = dev->read_stripe_size;
  if (s->avdimen.line_difference)
    lines_per_stripe += 2 * s->avdimen.line_difference;
  
  stripe_size = s->avdimen.hw_bytes_per_line * lines_per_stripe;
  lines_per_output = lines_per_stripe - 2 * s->avdimen.line_difference;
  
  if (s->av_con.connection_type == AV_SCSI)
    /* maybe better not /2 ... */
    max_bytes_per_read = dev->scsi_buffer_size / 2;
  else 
    /* vast buffer size to saturate the bus */
    max_bytes_per_read = 0x100000;
  
  out_size = s->avdimen.hw_bytes_per_line * lines_per_output;
  
  DBG (3, "dev->scsi_buffer_size / 2: %d\n",
       dev->scsi_buffer_size / 2);
  
  DBG (3, "bytes_per_line: %d, pixels_per_line: %d\n",
       s->avdimen.hw_bytes_per_line, s->avdimen.hw_pixels_per_line);
  
  DBG (3, "lines_per_stripe: %d, lines_per_output: %d\n",
       lines_per_stripe, lines_per_output);
  
  DBG (3, "max_bytes_per_read: %d, stripe_size: %d, out_size: %d\n",
       max_bytes_per_read, stripe_size, out_size);
  
  stripe_data = malloc (stripe_size);
  
  /* for software scaling we need an additional interpolation line buffer */
  if (s->avdimen.hw_xres != s->avdimen.xres ||
      s->avdimen.hw_yres != s->avdimen.yres)
  {
    /* layout out_data so that the interpolation history is exactly in front */
    ip_history = malloc (s->avdimen.hw_bytes_per_line + out_size);
    out_data = ip_history + s->avdimen.hw_bytes_per_line;
 
    ip_data = malloc (s->params.bytes_per_line);
  }
  else {
    out_data = malloc (out_size);
  }
  
  /* calculate params for the reading loop */
  total_size = s->avdimen.hw_bytes_per_line * (s->avdimen.hw_lines +
					       2 * s->avdimen.line_difference +
					       s->avdimen.rear_offset);
  if (deinterlace != NONE && !s->duplex_rear_valid)
    total_size *= 2;
  DBG (3, "reader_process: total_size: %lu\n", (u_long) total_size);
  
  /* write a RAW PNM file for debugging -ReneR */
  if (0 /* DEBUG */ &&
      (deinterlace == NONE || (deinterlace != NONE && !s->duplex_rear_valid)) )
    {
      raw_fp = fopen ("/tmp/sane-avision.raw", "w");
      write_pnm_header (fp, s->c_mode, s->params.depth,
			s->avdimen.hw_pixels_per_line, total_size / s->avdimen.hw_bytes_per_line);
    }
  
  processed_bytes = 0;
  stripe_fill = 0;
  
  /* First, dump background raster, bypassing all the other processing. */
  if (dev->inquiry_background_raster && s->val[OPT_BACKGROUND].w)
    {
      uint8_t* background = s->background_raster;
      if (s->duplex_rear_valid)
	background += s->params.bytes_per_line * s->val[OPT_BACKGROUND].w;
      
      DBG (5, "reader_process: dumping background raster\n");
      fwrite (background, s->params.bytes_per_line, s->val[OPT_BACKGROUND].w, fp);
    }
  
  /* Data read; loop until all data has been processed.  Might exit
     before all lines are transferred for ADF paper end. */
  while (exit_status == SANE_STATUS_GOOD && processed_bytes < total_size)
    {
      unsigned int useful_bytes;
      
      DBG (5, "reader_process: stripe filled: %d\n", stripe_fill);
      
      /* fill the stripe buffer with real data */
      while (!s->duplex_rear_valid &&
	     processed_bytes < total_size && stripe_fill < stripe_size &&
             exit_status  == SANE_STATUS_GOOD)
	{
	  size_t this_read = stripe_size - stripe_fill;
	  
	  /* Limit reads to max_bytes_per_read and global data
	     boundaries. Rounded to the next lower multiple of
	     byte_per_lines, otherwise some scanners freeze. */
	  if (this_read > max_bytes_per_read)
	    this_read = (max_bytes_per_read -
			 max_bytes_per_read % s->avdimen.hw_bytes_per_line);

	  if (processed_bytes + this_read > total_size)
	    this_read = total_size - processed_bytes;

          read_constrains(s, this_read);

	  DBG (5, "reader_process: processed_bytes: %lu, total_size: %lu\n",
	       (u_long) processed_bytes, (u_long) total_size);
	  DBG (5, "reader_process: this_read: %lu\n", (u_long) this_read);

	  sigprocmask (SIG_BLOCK, &sigterm_set, 0);
	  status = read_data (s, stripe_data + stripe_fill, &this_read);
	  sigprocmask (SIG_UNBLOCK, &sigterm_set, 0);
	  
	  /* only EOF on the second stripe, as otherwise the rear page
	     is shorter */
	  if (status == SANE_STATUS_EOF && deinterlace == STRIPE) {
	    if (dev->inquiry_asic_type > AV_ASIC_C7 && dev->inquiry_asic_type < AV_ASIC_OA980) {
	      this_read = 0;
	    } else {
	      static int already_eof = 0;
	      if (!already_eof) {
		DBG (5, "reader_process: first EOF on stripe interlace: hiding.\n");
		status = SANE_STATUS_GOOD;
		already_eof = 1;
	      }
	    }
	  }
	  
	  /* write RAW data to file for debugging */
	  if (raw_fp && this_read > 0)
	    fwrite (stripe_data + stripe_fill, this_read, 1, raw_fp);
	  
	  if (status == SANE_STATUS_EOF || this_read == 0) {
	    DBG (1, "reader_process: read_data failed due to EOF\n");
	    exit_status = SANE_STATUS_EOF;
	  }
	  
	  if (status != SANE_STATUS_GOOD) {
	    DBG (1, "reader_process: read_data failed with status: %d\n",
		 status);
	    exit_status = status;
	  }
          
	  stripe_fill += this_read;
	  processed_bytes += this_read;
	}
      
      /* fill the stripe buffer with stored, virtual data */
      if (s->duplex_rear_valid)
	{
	  size_t this_read = stripe_size - stripe_fill;
	  size_t got;
	  
	  /* limit reads to max_read and global data boundaries */
	  if (this_read > max_bytes_per_read)
	    this_read = max_bytes_per_read;
	  
	  if (processed_bytes + this_read > total_size)
	    this_read = total_size - processed_bytes;
	  
	  DBG (5, "reader_process: virtual processed_bytes: %lu, total_size: %lu\n",
	       (u_long) processed_bytes, (u_long) total_size);
	  DBG (5, "reader_process: virtual this_read: %lu\n", (u_long) this_read);
	  
	  got = fread (stripe_data + stripe_fill, 1, this_read, rear_fp);
	  stripe_fill += got;
	  processed_bytes += got;
	  if (got != this_read)
	    exit_status = SANE_STATUS_EOF;
	}
      
      DBG (5, "reader_process: stripe filled: %d\n", stripe_fill);
      
      useful_bytes = stripe_fill;

      if (color_mode_is_color (s->c_mode))
	useful_bytes -= 2 * s->avdimen.line_difference * s->avdimen.hw_bytes_per_line;
      
      DBG (3, "reader_process: useful_bytes %i\n", useful_bytes);
      
      /* Deinterlace, save the rear stripes. For some scanners (AV220)
	 that is every 2nd stripe, the 2nd half of the transferred
	 data ((AV83xx), or every 2nd line (AV122)). */
      if (deinterlace != NONE && !s->duplex_rear_valid)
	{
	  /* for all lines we have in the buffer: */
	  unsigned int absline = (processed_bytes - stripe_fill) / s->avdimen.hw_bytes_per_line;
	  unsigned int abslines = absline + useful_bytes / s->avdimen.hw_bytes_per_line;
	  uint8_t* ptr = stripe_data;
	  for ( ; absline < abslines; ++absline)
	    {
	      DBG (9, "reader_process: deinterlacing line %d\n", absline);
	      /* interlaced? save the back data to the rear buffer */
	      if ( (deinterlace == STRIPE && absline % (lines_per_stripe*2) >= lines_per_stripe) ||
		   (deinterlace == HALF   && absline >= total_size / s->avdimen.hw_bytes_per_line / 2) ||
		   (deinterlace == LINE   && absline & 0x1) ) /* last bit equals % 2 */
		{
		  DBG (9, "reader_process: saving rear line %d to temporary file.\n", absline);
		  fwrite (ptr, s->avdimen.hw_bytes_per_line, 1, rear_fp);
		  if (deinterlace == LINE)
		    memmove (ptr, ptr+s->avdimen.hw_bytes_per_line,
			     stripe_data + stripe_fill - ptr - s->avdimen.hw_bytes_per_line);
		  else
		    ptr += s->avdimen.hw_bytes_per_line;
		  useful_bytes -= s->avdimen.hw_bytes_per_line;
		  stripe_fill -= s->avdimen.hw_bytes_per_line;
		}
	      else
		ptr += s->avdimen.hw_bytes_per_line;
 	    }
	  DBG (9, "reader_process: after deinterlacing: useful_bytes: %d, stripe_fill: %d\n",
	       useful_bytes, stripe_fill);
	}
      if (dev->hw->feature_type & AV_ADF_FLIPPING_DUPLEX && s->source_mode == AV_ADF_DUPLEX && !(s->page % 2) && !s->duplex_rear_valid) {
        /* Here we flip the image by writing the lines from the end of the file to the beginning. */
	unsigned int absline = (processed_bytes - stripe_fill) / s->avdimen.hw_bytes_per_line;
	unsigned int abslines = absline + useful_bytes / s->avdimen.hw_bytes_per_line;
	uint8_t* ptr = stripe_data;
	for ( ; absline < abslines; ++absline) {
          fseek (rear_fp, ((0 - s->params.lines) - absline - 2) * s->avdimen.hw_bytes_per_line, SEEK_SET);
          fwrite (ptr, s->avdimen.hw_bytes_per_line, 1, rear_fp);
          useful_bytes -= s->avdimen.hw_bytes_per_line;
          stripe_fill -= s->avdimen.hw_bytes_per_line;
          ptr += s->avdimen.hw_bytes_per_line;
        }
	DBG (9, "reader_process: after page flip: useful_bytes: %d, stripe_fill: %d\n",
	       useful_bytes, stripe_fill);
      } else {
      
      /*
       * Perform needed data conversions (packing, ...) and/or copy the
       * image data.
       */
      
      if (s->c_mode != AV_TRUECOLOR && s->c_mode != AV_TRUECOLOR16)
	/* simple copy */
	{
	  memcpy (out_data, stripe_data, useful_bytes);
	}
      else /* AV_TRUECOLOR* */
	{
	  /* WARNING: DO NOT MODIFY MY (HOPEFULLY WELL) OPTIMIZED
	     ALGORITHMS BELOW, WITHOUT UNDERSTANDING THEM FULLY ! */
	  if (s->avdimen.line_difference > 0) /* color-pack */
	    {
	      /* TODO: add 16bit per sample code? */
	      unsigned int i;
	      int c_offset = s->avdimen.line_difference * s->avdimen.hw_bytes_per_line;
	      
	      uint8_t* r_ptr = stripe_data;
	      uint8_t* g_ptr = stripe_data + c_offset + 1;
	      uint8_t* b_ptr = stripe_data + 2 * c_offset + 2;
	      
	      for (i = 0; i < useful_bytes;) {
		out_data [i++] = *r_ptr; r_ptr += 3;
		out_data [i++] = *g_ptr; g_ptr += 3;
		out_data [i++] = *b_ptr; b_ptr += 3;
	      }
	    } /* end color pack */
	  else if (dev->inquiry_needs_line_pack) /* line-pack */
	    { 
	      /* TODO: add 16bit per sample code? */
	      int i = 0, l, p;
	      const int lines = useful_bytes / s->avdimen.hw_bytes_per_line;
	      
	      for (l = 0; l < lines; ++l)
		{
		  uint8_t* r_ptr = stripe_data + (s->avdimen.hw_bytes_per_line * l);
		  uint8_t* g_ptr = r_ptr + s->avdimen.hw_pixels_per_line;
		  uint8_t* b_ptr = g_ptr + s->avdimen.hw_pixels_per_line;
		  
		  for (p = 0; p < s->avdimen.hw_pixels_per_line; ++p) {
		    out_data [i++] = *(r_ptr++);
		    out_data [i++] = *(g_ptr++);
		    out_data [i++] = *(b_ptr++);
		  }
		}
	    } /* end line pack */
	  else /* else no packing was required -> simple copy */
	    { 
	      memcpy (out_data, stripe_data, useful_bytes);
	    }
	} /* end if AV_TRUECOLOR* */
      
      /* FURTHER POST-PROCESSING ON THE FINAL OUTPUT DATA */
      
      /* maybe mirroring in ADF mode */
      if (s->source_mode_dim == AV_ADF_DIM && dev->inquiry_adf_need_mirror)
        {
	  if ( (s->c_mode != AV_TRUECOLOR) ||
	       (s->c_mode == AV_TRUECOLOR && dev->inquiry_adf_bgr_order) )
	  {
	    /* Mirroring with bgr -> rgb conversion: Just mirror the
	     * whole line */

	    int l;
	    int lines = useful_bytes / s->avdimen.hw_bytes_per_line;

	    for (l = 0; l < lines; ++l)
	      {
		uint8_t* begin_ptr = out_data + (l * s->avdimen.hw_bytes_per_line);
		uint8_t* end_ptr = begin_ptr + s->avdimen.hw_bytes_per_line;
		
		while (begin_ptr < end_ptr) {
		  uint8_t tmp;
		  tmp = *begin_ptr;
		  *begin_ptr++ = *end_ptr;
		  *end_ptr-- = tmp;
		}
	      }
	  }
	else /* non trivial mirroring */
	  {
	    /* Non-trivial Mirroring with element swapping */
	    
	    int l;
	    int lines = useful_bytes / s->avdimen.hw_bytes_per_line;

	    for (l = 0; l < lines; ++l)
	      {
		uint8_t* begin_ptr = out_data + (l * s->avdimen.hw_bytes_per_line);
		uint8_t* end_ptr = begin_ptr + s->avdimen.hw_bytes_per_line - 3;
		
		while (begin_ptr < end_ptr) {
		  uint8_t tmp;
		  
		  /* R */
		  tmp = *begin_ptr;
		  *begin_ptr++ = *end_ptr;
		  *end_ptr++ = tmp;
		  
		  /* G */
		  tmp = *begin_ptr;
		  *begin_ptr++ = *end_ptr;
		  *end_ptr++ = tmp;
		  
		  /* B */
		  tmp = *begin_ptr;
		  *begin_ptr++ = *end_ptr;
		  *end_ptr = tmp;
		  
		  end_ptr -= 5;
		}
	      }
	  }
      } /* end if mirroring needed */

      /* byte swapping and software calibration 16bit mode */
      if (s->c_mode == AV_GRAYSCALE12 ||
          s->c_mode == AV_GRAYSCALE16 ||
          s->c_mode == AV_TRUECOLOR12 ||
          s->c_mode == AV_TRUECOLOR16) {
	
	int l;
	int lines = useful_bytes / s->avdimen.hw_bytes_per_line;

	uint8_t* dark_avg_data = s->dark_avg_data;
	uint8_t* white_avg_data = s->white_avg_data;
	
	uint8_t* begin_ptr = out_data;
	uint8_t* end_ptr = begin_ptr + s->avdimen.hw_bytes_per_line;
	uint8_t* line_ptr;

        double scale = 1.0;
        if (s->c_mode == AV_GRAYSCALE12 || s->c_mode == AV_TRUECOLOR12)
          scale = (double) (1<<4);
	
	while (begin_ptr < end_ptr) {
	  uint16_t dark_avg = 0;
	  uint16_t white_avg = WHITE_MAP_RANGE;
	  
	  if (dark_avg_data)
	    dark_avg = get_double_le (dark_avg_data);
	  if (white_avg_data)
	    white_avg = get_double_le (white_avg_data);
	  
	  line_ptr = begin_ptr;
	  for (l = 0; l < lines; ++ l)
	    {
	      double v = (double) get_double_le (line_ptr) * scale;
	      uint16_t v2;
	      if (0)
		v = (v - dark_avg) * white_avg / WHITE_MAP_RANGE;
	      
	      v2 = v < 0xFFFF ? v : 0xFFFF;
	      
	      /* SANE Standard 3.2.1 "... bytes of each sample value are
		 transmitted in the machine's native byte order." */
	      *line_ptr = v2;
	      
	      line_ptr += s->avdimen.hw_bytes_per_line;
	    }
	  
	  begin_ptr += 2;
	  if (dark_avg_data)
	    dark_avg_data += 2;
	  if (white_avg_data)
	    white_avg_data += 2;
	}
      }
      
      /* SOFTWARE SCALING WITH INTERPOLATION (IF NECESSARY) */
      
      if (s->avdimen.hw_xres == s->avdimen.xres &&
	  s->avdimen.hw_yres == s->avdimen.yres) /* No scaling */
	{
	  int lines, _hw_line = hw_line;
	  uint8_t* src = out_data;
	  /* we support cropping at the beginning and end due to rear offset */
	  for (lines = useful_bytes / s->avdimen.hw_bytes_per_line;
	       lines > 0; --lines, ++_hw_line, src += s->avdimen.hw_bytes_per_line)
	  {
	    if (deinterlace != NONE) {
	      /* crop rear offset :-( */
	      if ( (!s->duplex_rear_valid && _hw_line >= s->avdimen.hw_lines) ||
		   (s->duplex_rear_valid && _hw_line < s->avdimen.rear_offset) )
	      {
	        DBG (7, "reader_process: skip due read offset line: %d\n", line);
	        continue;
	      }
	    }
	    fwrite (src, s->avdimen.hw_bytes_per_line, 1, fp);
	    ++line;
	  }
	}
      else /* Software scaling - watch out - this code bites back! */
	{
	  int x;
	  /* for convenience in the 16bit code path */
	  uint16_t* out_data16 = (uint16_t*) out_data;
	  
	  const int hw_line_end = hw_line + useful_bytes / s->avdimen.hw_bytes_per_line;
	  
	  /* on-the-fly bi-linear interpolation */
	  while (1) {
	    double by = (-1.0 + s->avdimen.hw_lines) * line / (s->avdimen.hw_lines * s->avdimen.xres / s->avdimen.hw_xres + s->val[OPT_BACKGROUND].w);
	    int sy = (int)floor(by);
	    int ydist = (int) ((by - sy) * 256);
	    int syy = sy + 1;
	    
	    const int hwbpl = s->avdimen.hw_bytes_per_line;
	    
	    uint8_t* dst = ip_data;
	    uint16_t* dst16 = (uint16_t*) ip_data;
	    unsigned int v; /* accumulator */
	    
	    /* Break out if we do not have the hw source line - yet,
	       or when we are past the end of wanted data (e.g. on the
	       front page due to rear_offset). Also take the read_offset
	       into account on the rear side */
	    if (deinterlace != NONE) {
	      if (!s->duplex_rear_valid && syy >= s->avdimen.hw_lines) {
		DBG (7, "reader_process: skip due past intended front page lines: %d\n", sy);
	        break;
	      }
	      else if (s->duplex_rear_valid) {
		/* the beginning is to be skipped, accessed thru offset */
		DBG (7, "reader_process: rear_offset adjusting source: %d\n", sy);
	        sy += s->avdimen.rear_offset;
		syy += s->avdimen.rear_offset;
	      }
	    }
	    
	    if (sy >= hw_line_end || syy >= hw_line_end) {
	      DBG (3, "reader_process: source line %d-%d not yet avail\n",
		   sy, syy);
	      break;
	    }
	    
	    /* convert to offset in current stripe */
	    sy -= hw_line;
	    syy -= hw_line;
	    
	    if (sy < -1) {
	      DBG (1, "reader_process: need more history: %d???\n", sy);
	      sy = -1;
	    }
	    
	    DBG (8, "reader_process: out line: %d <- from: %d-%d\n",
		 line, sy, syy);
	    
	    for (x = 0; x < s->params.pixels_per_line; ++x) {
	      const double bx = (-1.0 + s->avdimen.hw_pixels_per_line) * x / s->params.pixels_per_line;
	      const int sx = (int)floor(bx);
	      const int xdist = (int) ((bx - sx) * 256);
	      const int sxx = sx + 1;
	      
	      if (x == 0 || x == s->params.pixels_per_line - 1)
		 DBG (8, "reader_process: x: %d <- from: %d-%d\n",
		      x, sx, sxx);
	      
	      switch (s->c_mode) {
	      case AV_THRESHOLDED:
	      case AV_DITHERED:
		{
		  /* Repeating this over and over again is not fast, but
		     as a seldom used code-path we want it readable.
		     x/8 is the byte, and x%8 the bit position. */
		  v = 
		    ( ((out_data [sy*hwbpl  + sx/8 ] >> (7-sx%8 )) & 1) * (256-xdist) * (256-ydist) +
		      ((out_data [sy*hwbpl  + sxx/8] >> (7-sxx%8)) & 1) * xdist       * (256-ydist) +
		      ((out_data [syy*hwbpl + sx/8 ] >> (7-sx%8 )) & 1) * (256-xdist) * ydist +
		      ((out_data [syy*hwbpl + sxx/8] >> (7-sxx%8)) & 1) * xdist       * ydist
		      ) / (1 + 1 * 256);
		  
		  /* Shift and or the result together and eventually
		     jump to the next byte. */
		  *dst = (*dst << 1) | ((v>>7)&1);
		    if (x % 8 == 7)
		     ++dst;
		}
		break;
		
	      case AV_GRAYSCALE:
		{
		  v = 
		    ( out_data [sy*hwbpl  + sx ] * (256-xdist) * (256-ydist) +
		      out_data [sy*hwbpl  + sxx] * xdist       * (256-ydist) +
		      out_data [syy*hwbpl + sx ] * (256-xdist) * ydist +
		      out_data [syy*hwbpl + sxx] * xdist       * ydist
		      ) / (256 * 256);
		  *dst++ = v;
		}
		break;
		
	      case AV_GRAYSCALE12:
	      case AV_GRAYSCALE16:
		{
		  /* TODO: test! */		  
		  v = 
		    ( out_data16 [sy*hwbpl  + sx ] * (256-xdist) * (256-ydist) +
		      out_data16 [sy*hwbpl  + sxx] * xdist       * (256-ydist) +
		      out_data16 [syy*hwbpl + sx ] * (256-xdist) * ydist +
		      out_data16 [syy*hwbpl + sxx] * xdist       * ydist
		      ) / (256 * 256);
		  *dst16++ = v;
		}
		break;
		
	      case AV_TRUECOLOR:
		{
		  int c;
		  for (c = 0; c < 3; ++c)
		    {
		      v = 
			( out_data [sy*hwbpl  + sx*3  + c] * (256-xdist) * (256-ydist) +
			  out_data [sy*hwbpl  + sxx*3 + c] * xdist       * (256-ydist) +
			  out_data [syy*hwbpl + sx*3  + c] * (256-xdist) * ydist +
			  out_data [syy*hwbpl + sxx*3 + c] * xdist       * ydist
			  ) / (256 * 256);
		      *dst++ = v;
		    }
		}
		break;
		
	      case AV_TRUECOLOR12:
	      case AV_TRUECOLOR16:
		{
		  /* TODO: test! */
		  int c;
		  for (c = 0; c < 3; ++c)
		    {
		      v = 
			( out_data16 [sy*hwbpl  + sx*3  + c] * (256-xdist) * (256-ydist) +
			  out_data16 [sy*hwbpl  + sxx*3 + c] * xdist       * (256-ydist) +
			  out_data16 [syy*hwbpl + sx*3  + c] * (256-xdist) * ydist +
			  out_data16 [syy*hwbpl + sxx*3 + c] * xdist       * ydist
			  ) / (256 * 256);
		      *dst16++ = v;
		    }
		}
		break;
		
	      case AV_COLOR_MODE_LAST:
		; /* silence compiler warning */
	      }
	    }
	    fwrite (ip_data, s->params.bytes_per_line, 1, fp);
	    ++line;
	  }
	  /* copy one line of history for the next pass */
	  memcpy (ip_history,
	          out_data + useful_bytes - s->avdimen.hw_bytes_per_line,
		  s->avdimen.hw_bytes_per_line);
	}
      }
      
      /* save image date in stripe buffer for next next stripe */
      stripe_fill -= useful_bytes;
      if (stripe_fill > 0)
	memcpy (stripe_data, stripe_data + useful_bytes, stripe_fill);
      
      hw_line += useful_bytes / s->avdimen.hw_bytes_per_line;
      
      DBG (3, "reader_process: end of iteration\n");
    } /* end while not all lines or inf. mode */
  
  DBG (3, "reader_process: i/o loop finished\n");
  if (exit_status == SANE_STATUS_GOOD)
    exit_status = SANE_STATUS_EOF;
  
  if (raw_fp)
    fclose (raw_fp);
  
  /* maybe we need to fill in some white data */
  if (exit_status == SANE_STATUS_EOF && line < s->params.lines) {
    DBG (3, "reader_process: padding with white data\n");
    memset (out_data, gray_mode ? 0xff : 0x00, s->params.bytes_per_line);
    
    DBG (6, "reader_process: padding line %d - %d\n",
	 line, s->params.lines);
    while (line < s->params.lines) {
      fwrite (out_data, s->params.bytes_per_line, 1, fp);
      ++line;
    }
  }
  
  /* Eject film holder and/or release_unit - but only for
     non-duplex-rear / non-virtual scans. */
  if ((deinterlace != NONE && s->duplex_rear_valid) ||
     ((dev->hw->feature_type & AV_ADF_FLIPPING_DUPLEX) && s->source_mode == AV_ADF_DUPLEX && !(s->page % 2) && s->duplex_rear_valid))
    {
      DBG (1, "reader_process: virtual duplex scan - no device cleanup!\n");
    }
  else
    {
      /* poll the cancel button if the scanner is marked as having one */
      if (dev->hw->feature_type & AV_CANCEL_BUTTON) {
	if (get_button_status (s) == SANE_STATUS_CANCELLED)
	  exit_status =  SANE_STATUS_CANCELLED;
      }

      status = release_unit (s, 0);
      if (status != SANE_STATUS_GOOD)
	DBG (1, "reader_process: release_unit failed\n");
      
      if (dev->inquiry_new_protocol && dev->scanner_type == AV_FILM) {
	status = object_position (s, AVISION_SCSI_OP_GO_HOME);
	if (status != SANE_STATUS_GOOD)
	  DBG (1, "reader_process: object position go-home failed!\n");
      }
    }
  
  if ((dev->hw->feature_type & AV_ADF_FLIPPING_DUPLEX) && s->source_mode == AV_ADF_DUPLEX && s->page % 2) {
    /* front page of flipping duplex */
    if (exit_status == SANE_STATUS_EOF) {
      if (s->val[OPT_ADF_FLIP].w) {
        /* The page flip bit must be reset after every scan, but if the
         * user doesn't care, there's no reason to reset.
         */
        status = set_window (s);
        if (status != SANE_STATUS_GOOD) {
          DBG (1, "reader_process: set scan window command failed: %s\n",
          sane_strstatus (status));
          return status;
        }
      }
      /* we can set anything here without fear because the process will terminate soon and take our changes with it */
      s->page += 1;
      s->params.lines = -line;
      exit_status = reader_process (s);
    }
    /* TODO:
    * else {
    *   spit out the page if an error was encountered...
    *   assuming the error won't prevent it.
    * } */
  } else {
    fclose (fp);
  }
  if (rear_fp)
    fclose (rear_fp);
 
  if (ip_data) free (ip_data);
  if (ip_history)
    free (ip_history);
  else
    free (out_data); /* if we have ip_history out_data is included there */
  
  free (stripe_data);
  
  DBG (3, "reader_process: returning success\n");
  return exit_status;
}

/* SANE callback to attach a SCSI device */
static SANE_Status
attach_one_scsi (const char* dev)
{
  attach (dev, AV_SCSI, 0);
  return SANE_STATUS_GOOD;
}

/* SANE callback to attach a USB device */
static SANE_Status
attach_one_usb (const char* dev)
{
  attach (dev, AV_USB, 0);
  return SANE_STATUS_GOOD;
}
  
static SANE_Status
sane_reload_devices (void)
{
  FILE* fp;
  
  char line[PATH_MAX];
  const char* cp = 0;
  char* word;
  int linenumber = 0;
  int model_num = 0;  

  sanei_usb_init ();
  fp = sanei_config_open (AVISION_CONFIG_FILE);
  if (fp <= (FILE*)0)
    {
      DBG (1, "sane_reload_devices: No config file present!\n");
    }
  else
    {
      /* first parse the config file */
      while (sanei_config_read  (line, sizeof (line), fp))
	{
	  attaching_hw = 0;
	  
	  word = NULL;
	  ++ linenumber;
      
	  DBG (5, "sane_reload_devices: parsing config line \"%s\"\n",
	       line);
      
	  cp = sanei_config_get_string (line, &word);
	  
	  if (!word || cp == line) {
	    DBG (5, "sane_reload_devices: config file line %d: ignoring empty line\n",
		 linenumber);
	    if (word) {
	      free (word);
	      word = NULL;
	    }
	    continue;
	  }
	  
	  if (!word) {
	    DBG (1, "sane_reload_devices: config file line %d: could not be parsed\n",
		 linenumber);
	    continue;
	  }
	  
	  if (word[0] == '#') {
	    DBG (5, "sane_reload_devices: config file line %d: ignoring comment line\n",
		 linenumber);
	    free (word);
	    word = NULL;
	    continue;
	  }
                    
	  if (strcmp (word, "option") == 0)
	    {
	      free (word);
	      word = NULL;
	      cp = sanei_config_get_string (cp, &word);
	  
	      if (strcmp (word, "disable-gamma-table") == 0) {
		DBG (3, "sane_reload_devices: config file line %d: disable-gamma-table\n",
		     linenumber);
		disable_gamma_table = SANE_TRUE;
	      }
	      else if (strcmp (word, "disable-calibration") == 0) {
		DBG (3, "sane_reload_devices: config file line %d: disable-calibration\n",
		     linenumber);
		disable_calibration = SANE_TRUE;
	      }
	      else if (strcmp (word, "force-calibration") == 0) {
		DBG (3, "sane_reload_devices: config file line %d: force-calibration\n",
		     linenumber);
		force_calibration = SANE_TRUE;
	      }
	      else if (strcmp (word, "force-a4") == 0) {
		DBG (3, "sane_reload_devices: config file line %d: enabling force-a4\n",
		     linenumber);
		force_a4 = SANE_TRUE;
	      }
	      else if (strcmp (word, "force-a3") == 0) {
		DBG (3, "sane_reload_devices: config file line %d: enabling force-a3\n",
		     linenumber);
		force_a3 = SANE_TRUE;
	      }
	      else if (strcmp (word, "static-red-calib") == 0) {
		DBG (3, "sane_reload_devices: config file line %d: static red calibration\n",
		     linenumber);
		static_calib_list [0] = SANE_TRUE;
	      }
	      else if (strcmp (word, "static-green-calib") == 0) {
		DBG (3, "sane_reload_devices: config file line %d: static green calibration\n",
		    linenumber);
		static_calib_list [1] = SANE_TRUE;
	      }
	      else if (strcmp (word, "static-blue-calib") == 0) {
		DBG (3, "sane_reload_devices: config file line %d: static blue calibration\n",
		    linenumber);
		static_calib_list [2] = SANE_TRUE; 
	      }
	      else
		DBG (1, "sane_reload_devices: config file line %d: options unknown!\n",
		    linenumber);
	    }
	  else if (strcmp (word, "usb") == 0) {
	    DBG (2, "sane_reload_devices: config file line %d: trying to attach USB:`%s'\n",
		 linenumber, line);
	    /* try to attach USB device */
	    sanei_usb_attach_matching_devices (line, attach_one_usb);
	  }
	  else if (strcmp (word, "scsi") == 0) {
	    DBG (2, "sane_reload_devices: config file line %d: trying to attach SCSI: %s'\n",
		 linenumber, line);
	    
	    /* the last time I verified (2003-03-18) this function
	       only matches SCSI devices ... */
	    sanei_config_attach_matching_devices (line, attach_one_scsi);
	  }
	  else {
	    DBG (1, "sane_reload_devices: config file line %d: OBSOLETE !! use the scsi keyword!\n",
		linenumber);
	    DBG (1, "sane_reload_devices:   (see man sane-avision for details): trying to attach SCSI: %s'\n",
		line);
	  
	    /* the last time I verified (2003-03-18) this function
	       only matched SCSI devices ... */
	    sanei_config_attach_matching_devices (line, attach_one_scsi);
	  }
	  free (word);
	  word = NULL;
	} /* end while read */
      
      fclose (fp);
      
      if (word)
	free (word);
    } /* end if fp */
  
  /* search for all supported SCSI/USB devices */
  while (Avision_Device_List [model_num].scsi_mfg != NULL ||
         Avision_Device_List [model_num].real_mfg != NULL)
    {
      /* also potentially accessed from the attach_* callbacks */
      attaching_hw = &(Avision_Device_List [model_num]);
	if (attaching_hw->scsi_mfg != NULL)
	  sanei_scsi_find_devices (attaching_hw->scsi_mfg,
				               attaching_hw->scsi_model, NULL,
				               -1, -1, -1, -1,
				               attach_one_scsi);
      
	if (attaching_hw->usb_vendor != 0 && attaching_hw->usb_product != 0 )
	{
	  DBG (1, "sane_reload_devices: Trying to find USB device %.4x %.4x ...\n",
	       attaching_hw->usb_vendor,
	       attaching_hw->usb_product);
	  
	  /* TODO: check return value */
	  if (sanei_usb_find_devices (attaching_hw->usb_vendor,
				      attaching_hw->usb_product,
				      attach_one_usb) != SANE_STATUS_GOOD) {
	    DBG (1, "sane_reload_devices: error during USB device detection!\n");
	  }
	}
      ++ model_num;
    } /* end for all devices in supported list */
  
  attaching_hw = 0;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_init (SANE_Int* version_code, SANE_Auth_Callback authorize)
{
  authorize = authorize; /* silence gcc */
  
  DBG_INIT();

#ifdef AVISION_STATIC_DEBUG_LEVEL
  DBG_LEVEL = AVISION_STATIC_DEBUG_LEVEL;
#endif
  
  DBG (3, "sane_init:(Version: %i.%i Build: %i)\n",
       SANE_CURRENT_MAJOR, V_MINOR, BACKEND_BUILD);
  
  /* must come first */
  sanei_thread_init ();

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, BACKEND_BUILD);

  sane_reload_devices ();

  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  Avision_Device* dev;
  Avision_Device* next;

  DBG (3, "sane_exit:\n");

  for (dev = first_dev; dev; dev = next) {
    next = dev->next;
    /* no warning for stripping const - C lacks a const_cast<> */
    free ((void*)(size_t) dev->sane.name);

    free (dev);
  }
  first_dev = NULL;

  free(devlist);
  devlist = NULL;
}

SANE_Status
sane_get_devices (const SANE_Device*** device_list, SANE_Bool local_only)
{
  Avision_Device* dev;
  int i;

  local_only = local_only; /* silence gcc */

  DBG (3, "sane_get_devices:\n");

  sane_reload_devices ();

  if (devlist)
    free (devlist);

  devlist = malloc ((num_devices + 1) * sizeof (devlist[0]));
  if (!devlist)
    return SANE_STATUS_NO_MEM;

  i = 0;
  for (dev = first_dev; i < num_devices; dev = dev->next)
    devlist[i++] = &dev->sane;
  devlist[i++] = 0;

  *device_list = devlist;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle *handle)
{
  Avision_Device* dev;
  SANE_Status status;
  Avision_Scanner* s;
  int i, j;
  uint8_t inquiry_result[AVISION_INQUIRY_SIZE_V1];

  DBG (3, "sane_open:\n");
  
  if (devicename[0]) {
    for (dev = first_dev; dev; dev = dev->next)
      if (strcmp (dev->sane.name, devicename) == 0)
	break;

    if (dev) {
      status = attach (devicename, dev->connection.connection_type, &dev);
      if (status != SANE_STATUS_GOOD)
	return status;
    }
  } else {
    /* empty devicename -> use first device */
    dev = first_dev;
  }

  if (!dev)
    return SANE_STATUS_INVAL;
  
  s = malloc (sizeof (*s));
  if (!s)
    return SANE_STATUS_NO_MEM;
  
  /* initialize ... */
  /* the other states (scanning, ...) rely on this memset (0) */
  memset (s, 0, sizeof (*s));
  
  /* initialize connection state */
  s->av_con.connection_type = dev->connection.connection_type;
  s->av_con.usb_status = dev->connection.usb_status;
  s->av_con.scsi_fd = -1;
  s->av_con.usb_dn = -1;
  
  s->reader_pid = -1;
  s->read_fds = -1;

  s->hw = dev;
  
  /* We initialize the table to a gamma value of 2.22, since this is what
     papers about Colorimetry suggest.

	http://www.poynton.com/GammaFAQ.html

     Avision's driver defaults to 2.2 though. */
  {
    const double gamma = 2.22;
    const double one_over_gamma = 1. / gamma;
    
    for (i = 0; i < 4; ++ i)
      for (j = 0; j < 256; ++ j)
	s->gamma_table[i][j] = pow( (double) j / 255, one_over_gamma) * 255;
  }
  
  /* insert newly opened handle into list of open handles: */
  s->next = first_handle;
  first_handle = s;
  *handle = s;
  
  /* open the device */
  if (! avision_is_open (&s->av_con) ) {
#ifdef HAVE_SANEI_SCSI_OPEN_EXTENDED
    DBG (1, "sane_open: using open_extended\n");
    status = avision_open_extended (s->hw->sane.name, &s->av_con, sense_handler, 0,
				    &(dev->scsi_buffer_size));
#else
    status = avision_open (s->hw->sane.name, &s->av_con, sense_handler, 0);
#endif
    if (status != SANE_STATUS_GOOD) {
      DBG (1, "sane_open: open of %s failed: %s\n",
	   s->hw->sane.name, sane_strstatus (status));
      return status;
    }
    DBG (1, "sane_open: got %d scsi_max_request_size\n", dev->scsi_buffer_size);
  }

  /* first: re-awake the device with an inquiry, some devices are flunk while initializing
     the usb connection and like a inquiry to come first ... (AV610 et.al.) */
  status = inquiry (s->av_con, inquiry_result, sizeof(inquiry_result));
  if (status != SANE_STATUS_GOOD) {
	DBG (1, "sane_open: awakening inquiry failed: %s\n", sane_strstatus (status));
	return status;
  }

  status = wait_ready (&s->av_con, 1);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "sane_open: wait_ready() failed: %s\n", sane_strstatus (status));
    return status;
  }

  /* update settings based on additional accessory information */
  status = additional_probe (s);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "sane_open: additional probe failed: %s\n", sane_strstatus (status));
    return status;
  }
  
  /* initialize the options */
  init_options (s);
  
  if (dev->inquiry_duplex_interlaced || dev->scanner_type == AV_FILM ||
      dev->hw->feature_type & AV_ADF_FLIPPING_DUPLEX) {
    /* Might need at least *DOS (Windows flavour and OS/2) portability fix
       However, I was told Cygwin (et al.) takes care of it. */
    strncpy(s->duplex_rear_fname, "/tmp/avision-rear-XXXXXX", PATH_MAX);
    
    if (! mktemp(s->duplex_rear_fname) ) {
      DBG (1, "sane_open: failed to generate temporary fname for duplex scans\n");
      return SANE_STATUS_NO_MEM;
    }
    else {
      DBG (1, "sane_open: temporary fname for duplex scans: %s\n",
	   s->duplex_rear_fname);
    }
  }
  
  /* calibrate film scanners, as this must be done without the
     film holder and at the full resolution */
  if (dev->scanner_type == AV_FILM)
    {
      int default_res = s->val[OPT_RESOLUTION].w;
      s->val[OPT_RESOLUTION].w = dev->inquiry_optical_res;
      
      DBG (1, "sane_open: early calibration for film scanner.\n");
      
      compute_parameters (s);
      
      status = set_window (s);
      if (status != SANE_STATUS_GOOD) {
	DBG (1, "sane_open: set scan window command failed: %s\n",
	     sane_strstatus (status));
	return status;
      }
      
      if (!(dev->hw->feature_type & AV_NO_CALIB))
	{
	  status = normal_calibration (s);
	  if (status != SANE_STATUS_GOOD) {
	    DBG (1, "sane_open: perform calibration failed: %s\n",
		 sane_strstatus (status));
	    return status;
	  }
	}
      
      if (dev->scanner_type == AV_FILM) {
	status = object_position (s, AVISION_SCSI_OP_GO_HOME);
	if (status != SANE_STATUS_GOOD)
	  DBG (1, "reader_open: object position go-home failed!\n");
      }
      
      s->val[OPT_RESOLUTION].w = default_res;
    }
  
  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  Avision_Scanner* prev;
  Avision_Scanner* s = handle;
  int i;

  DBG (3, "sane_close:\n");
  
  /* close the device */
  if (avision_is_open (&s->av_con) ) {
    avision_close (&s->av_con);
  }
  
  /* remove handle from list of open handles: */
  prev = 0;
  for (s = first_handle; s; s = s->next) {
    if (s == handle)
      break;
    prev = s;
  }

  /* a handle we know about ? */
  if (!s) {
    DBG (1, "sane_close: invalid handle %p\n", handle);
    return;
  }

  if (s->scanning)
    do_cancel (handle);

  if (prev)
    prev->next = s->next;
  else
    first_handle = s->next;

  for (i = 1; i < NUM_OPTIONS; ++ i) {
    if (s->opt[i].type == SANE_TYPE_STRING && s->val[i].s) {
      free (s->val[i].s);
    }
  }
  
  if (s->white_avg_data)
    free (s->white_avg_data);
  if (s->dark_avg_data)
    free (s->dark_avg_data);
  
  if (s->background_raster)
    free (s->background_raster);
  
  if (*(s->duplex_rear_fname)) {
    unlink (s->duplex_rear_fname);
    *(s->duplex_rear_fname) = 0;
  }
  
  free (handle);
}

const SANE_Option_Descriptor*
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  Avision_Scanner* s = handle;
  
  DBG (3, "sane_get_option_descriptor: %d\n", option);

  if ((unsigned) option >= NUM_OPTIONS)
    return 0;
  return s->opt + option;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void* val, SANE_Int* info)
{
  Avision_Scanner* s = handle;
  Avision_Device* dev = s->hw;
  SANE_Status status;
  SANE_Word cap;
  
  DBG (3, "sane_control_option: option=%d, action=%d\n",
       (int)option, (int)action);

  DBG (5, "sane_control_option: option=%s, action=%s\n",
       s->opt[option].name,
       action == SANE_ACTION_GET_VALUE ? "GET" :
       (action == SANE_ACTION_SET_VALUE ? "SET" :
	(action == SANE_ACTION_SET_AUTO ? "AUTO" : "UNKNOWN") ) );

  if (info)
    *info = 0;

  if (s->scanning)
    return SANE_STATUS_DEVICE_BUSY;
  
  if (option >= NUM_OPTIONS)
    return SANE_STATUS_INVAL;

  cap = s->opt[option].cap;

  if (!SANE_OPTION_IS_ACTIVE (cap))
    return SANE_STATUS_INVAL;

  if (action == SANE_ACTION_GET_VALUE)
    {
      switch (option)
	{
	  /* word options: */
	case OPT_PREVIEW:
	  
	case OPT_RESOLUTION:
	case OPT_SPEED:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_OVERSCAN_TOP:
	case OPT_OVERSCAN_BOTTOM:
	case OPT_BACKGROUND:
	case OPT_NUM_OPTS:
	  
	case OPT_BRIGHTNESS:
	case OPT_CONTRAST:
	case OPT_EXPOSURE:
	case OPT_IR:
	case OPT_MULTISAMPLE:
	case OPT_QSCAN:
	case OPT_QCALIB:
	case OPT_PAPERLEN:
	case OPT_ADF_FLIP:
	  *(SANE_Word*) val = s->val[option].w;
	  return SANE_STATUS_GOOD;
	  
	  /* specially treated word options */
	  
	case OPT_FRAME:
	  status = get_frame_info (s);
	  *(SANE_Word*) val = s->val[option].w;
	  return status;
	  
	case OPT_POWER_SAVE_TIME:
	  get_power_save_time (s, &(s->val[option].w));
	  *(SANE_Word*) val = s->val[option].w;
	  return SANE_STATUS_GOOD;
	  
	  /* word-array options: */
	case OPT_GAMMA_VECTOR:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	  memcpy (val, s->val[option].wa, s->opt[option].size);
	  return SANE_STATUS_GOOD;
	  
	  /* string options: */
	case OPT_MODE:
	case OPT_SOURCE:
	  strcpy (val, s->val[option].s);
	  return SANE_STATUS_GOOD;

	  /* specially treated string options */
	case OPT_MESSAGE:
	  if (dev->inquiry_button_control || dev->inquiry_buttons)
	    status = get_button_status (s);
	  
	  strcpy (val, s->val[option].s);
	  s->val[option].s[0] = 0;
	  return SANE_STATUS_GOOD;

	case OPT_NVRAM:
	  get_and_parse_nvram (s, s->val[option].s, 1024);
	  
	  strcpy (val, s->val[option].s);
	  return SANE_STATUS_GOOD;
	  
	} /* end switch option */
    } /* end if GET_ACTION_GET_VALUE */
  else if (action == SANE_ACTION_SET_VALUE)
    {
      if (!SANE_OPTION_IS_SETTABLE (cap))
	return SANE_STATUS_INVAL;
      
      status = constrain_value (s, option, val, info);
      if (status != SANE_STATUS_GOOD)
	return status;
      
      switch (option)
	{
	  /* side-effect-free word options: */
	case OPT_SPEED:
	case OPT_PREVIEW:
	case OPT_BRIGHTNESS:
	case OPT_CONTRAST:
	case OPT_EXPOSURE:
	case OPT_IR:
	case OPT_MULTISAMPLE:
	case OPT_QSCAN:
	case OPT_QCALIB:
	case OPT_OVERSCAN_TOP:
	case OPT_OVERSCAN_BOTTOM:
	case OPT_BACKGROUND:
	case OPT_PAPERLEN:
	case OPT_ADF_FLIP:
	  s->val[option].w = *(SANE_Word*) val;
	  return SANE_STATUS_GOOD;
	  
	  /* side-effect-free word-array options: */
	case OPT_GAMMA_VECTOR:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	  memcpy (s->val[option].wa, val, s->opt[option].size);
	  return SANE_STATUS_GOOD;
		 
	  /* options with side-effects: */
	case OPT_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:

	  s->val[option].w = *(SANE_Word*) val;

	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;

	  return SANE_STATUS_GOOD;

	  /* string options with side-effects: */
	case OPT_SOURCE:
	  
	  if (s->val[option].s) {
	    free(s->val[option].s);
	  }
	  s->val[option].s = strdup(val);
	  s->source_mode = match_source_mode (dev, s->val[option].s);
	  s->source_mode_dim = match_source_mode_dim (s->source_mode);
	  
	  /* set side-effects */
	  dev->x_range.max =
	    SANE_FIX ( dev->inquiry_x_ranges[s->source_mode_dim]);
	  dev->y_range.max =
	    SANE_FIX ( dev->inquiry_y_ranges[s->source_mode_dim]);

          if (s->hw->hw->feature_type & AV_ADF_FLIPPING_DUPLEX && s->source_mode == AV_ADF_DUPLEX) {
            s->opt[OPT_ADF_FLIP].cap &= ~SANE_CAP_INACTIVE;
          } else {
            s->opt[OPT_ADF_FLIP].cap |= SANE_CAP_INACTIVE;
          }
	  
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  
	  return SANE_STATUS_GOOD;
	  
	case OPT_MODE:
	  {
	    if (s->val[option].s)
	      free (s->val[option].s);
	    
	    s->val[option].s = strdup (val);
	    s->c_mode = match_color_mode (dev, s->val[OPT_MODE].s);
	    
	    /* set to mode specific values */
	    
	    /* the gamma table related */
	    if (!disable_gamma_table)
	      {
		if (color_mode_is_color (s->c_mode) ) {
		  s->opt[OPT_GAMMA_VECTOR].cap   |= SANE_CAP_INACTIVE;
		  s->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
		}
		else /* gray or mono */
		  {
		    s->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
		    s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
		    s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
		    s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
		  }
	      }		
	    if (info)
	      *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	    return SANE_STATUS_GOOD;
	  }
	case OPT_FRAME:
	  {
	    SANE_Word frame = *((SANE_Word  *) val);
	    
	    status = set_frame (s, frame);
	    if (status == SANE_STATUS_GOOD) {
	      s->val[OPT_FRAME].w = frame;
	      dev->current_frame = frame;
	    }
	    return status;
	  }
	case OPT_POWER_SAVE_TIME:
	  {
	    SANE_Word time = *((SANE_Word  *) val);

	    status = set_power_save_time (s, time);
	    if (status == SANE_STATUS_GOOD)
	      s->val[OPT_POWER_SAVE_TIME].w = time;
	    return status;
	  }
	} /* end switch option */
    }
  else if (action == SANE_ACTION_SET_AUTO)
    {
      if (!SANE_OPTION_IS_SETTABLE (cap))
	return SANE_STATUS_INVAL;

      switch (option)
	{
	case OPT_ADF_FLIP:
	  s->val[option].w = SANE_TRUE;
	  return SANE_STATUS_GOOD;
	} /* end switch option */
    } /* end else SET_VALUE */
  return SANE_STATUS_INVAL;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters* params)
{
  Avision_Scanner* s = handle;
  
  DBG (3, "sane_get_parameters:\n");
  
  /* During an actual scan these parameters will have been computed in
     sane_start().  Otherwise, the values must be computed on demand.  The
     values cannot be changed during a scan to avoid inconsistency. */
  if (!s->scanning)
    {
      DBG (3, "sane_get_parameters: computing parameters\n");
      compute_parameters (s);
    }

  if (params) {
    *params = s->params;
    /* add background raster lines */
    params->lines += s->val[OPT_BACKGROUND].w;
  }
  
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  Avision_Scanner* s = handle;
  Avision_Device* dev = s->hw;
  
  SANE_Status status;
  int fds [2];
  DBG (1, "sane_start:\n");
  
  /* Make sure there is no scan running!!! */
  if (s->scanning)
    return SANE_STATUS_DEVICE_BUSY;
 
  /* Make sure we have a current parameter set. Some of the
     parameters will be overwritten below, but that's OK. */
  status = sane_get_parameters (s, &s->params);
  if (status != SANE_STATUS_GOOD) {
    return status;
  }
  
  /* for non ADF scans (e.g. scanimage --batch-prompt on a Flatbed
     scanner) make sure we do not assume it's an ADF scan and
     optimize something away*/
  if (!is_adf_scan (s))
    s->page = 0;

  if (s->page > 0 && s->duplex_rear_valid) {
    DBG (1, "sane_start: virtual duplex rear data valid.\n");
    goto start_scan_end;
  }
  
  /* Check for paper during ADF scans and for sheetfed scanners. */
  if (is_adf_scan (s)) {
    status = media_check (s);
    if (status != SANE_STATUS_GOOD) {
      DBG (1, "sane_start: media_check failed: %s\n",
	   sane_strstatus (status));
      return status;
    }
    else
      DBG (1, "sane_start: media_check ok\n");
  }
  
  /* Check the light early, to return to the GUI and notify the user. */
  if (s->prepared == SANE_FALSE) {
    if (dev->inquiry_light_control) {
      status = wait_4_light (s);
      if (status != SANE_STATUS_GOOD) {
        return status;
      }
    }
  }

  if (s->page > 0 && dev->inquiry_keeps_window) {
     DBG (1, "sane_start: Optimized set_window away.\n");
  }
  else
  {
    status = set_window (s);
    if (status != SANE_STATUS_GOOD) {
      DBG (1, "sane_start: set scan window command failed: %s\n",
	   sane_strstatus (status));
      goto stop_scanner_and_return;
    }
  }

#ifdef DEBUG_TEST
  /* debug window size test ... */
  if (dev->inquiry_new_protocol)
    {
      size_t size = 16;
      uint8_t result[16];
      
      DBG (5, "sane_start: reading scanner window size\n");
      
      status = simple_read (s, 0x80, 0, &size, result);
      
      if (status != SANE_STATUS_GOOD) {
	DBG (1, "sane_start: get pixel size command failed: %s\n",
	     sane_strstatus (status));
	goto stop_scanner_and_return;
      } 
      debug_print_raw (5, "sane_start: pixel_size:", result, size);
      DBG (5, "sane_start: x-pixels: %d, y-pixels: %d\n",
	   get_quad (&(result[0])), get_quad (&(result[4])));
    }
#endif
  
  /* no calibration for ADF pages */
  if (s->page > 0) {
    DBG (1, "sane_start: optimized calibration away.\n");
    goto calib_end;
  }
  
  /* check whether the user enforces calibration */
  if (force_calibration) {
    DBG (1, "sane_start: calibration enforced in config!\n");
    goto calib;
  }

  /* Only perform the calibration for newer scanners - it is not needed
     for my Avision AV 630 - and also does not even work ... */
  if (!dev->inquiry_new_protocol) {
    DBG (1, "sane_start: old protocol no calibration needed!\n");
    goto calib_end;
  }
  
  if (!dev->inquiry_needs_calibration) {
    DBG (1, "sane_start: due to inquiry no calibration needed!\n");
    goto calib_end;
  }
  
  /* calibration allowed for this scanner? */
  if (dev->hw->feature_type & AV_NO_CALIB) {
    DBG (1, "sane_start: calibration disabled in device list!!\n");
    goto calib_end;
  }

  /* Not for film scanners, ... */
  if (dev->scanner_type == AV_FILM) {
    DBG (1, "sane_start: no calibration for film scanner!\n");
    goto calib_end;
  }
  
  /* check whether calibration is disabled by the user */
  if (disable_calibration) {
    DBG (1, "sane_start: calibration disabled in config - skipped!\n");
    goto calib_end;
  }
  
  /* R² reminder: We must not skip the calibration for ADF scans, some
     scanner (HP 53xx/74xx ASIC series) rely on a calibration data
     read (and will hang otherwise) */

 calib:
  status = normal_calibration (s);
  
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "sane_start: perform calibration failed: %s\n",
	 sane_strstatus (status));
	goto stop_scanner_and_return;
  }
  
 calib_end:
  
  if (dev->inquiry_3x3_matrix && dev->inquiry_asic_type >= AV_ASIC_C6 &&
      s->page == 0)
  {
    status = send_3x3_matrix (s);
    if (status != SANE_STATUS_GOOD) {
      return status;
    }
  }
  
  /* check whether gamma-table is disabled by the user? */
  if (disable_gamma_table) {
    DBG (1, "sane_start: gamma-table disabled in config - skipped!\n");
    goto gamma_end;
  }
  
  if (dev->hw->feature_type & AV_NO_GAMMA) {
    DBG (1, "sane_start: gamma table skipped due to device-list!!\n");
    goto gamma_end;
  }

  if (s->page > 0 && dev->inquiry_keeps_gamma)
    DBG (1, "sane_start: Optimized send_gamma away.\n");
  else
  {
    status = send_gamma (s);
    if (status != SANE_STATUS_GOOD) {
      DBG (1, "sane_start: send gamma failed: %s\n",
	   sane_strstatus (status));
      goto stop_scanner_and_return;
    }
  }
  
 gamma_end:
  
  if (dev->inquiry_tune_scan_length && is_adf_scan (s)) {
    status = send_tune_scan_length (s);
    if (status != SANE_STATUS_GOOD) {
      DBG (1, "sane_start: tune_scan_length command failed: %s\n",
	   sane_strstatus (status));
      goto stop_scanner_and_return;
    }
  }
  
  /* if the device supports retrieving background raster data
     inquire the data no matter if the user/applications asks for
     it in order to use it for bottom padding */
  if (s->page == 0 && dev->inquiry_background_raster) {
    status = get_background_raster (s);
    if (status != SANE_STATUS_GOOD) {
      DBG (1, "sane_start: get background raster command failed: %s\n",
	   sane_strstatus (status));
      goto stop_scanner_and_return;
    }
  }
  
  /* check film holder */
  if (dev->scanner_type == AV_FILM && dev->holder_type == 0xff) {
    DBG (1, "sane_start: no film holder or APS cassette!\n");
    
    /* Normally "go_home" is executed from the reader process,
       but as it will not start we have to reset things here */
    if (dev->inquiry_new_protocol) {
      status = object_position (s, AVISION_SCSI_OP_GO_HOME);
      if (status != SANE_STATUS_GOOD)
	DBG (1, "sane_start: go home failed: %s\n",
	     sane_strstatus (status));
    }
    goto stop_scanner_and_return;
  }
  
 start_scan_end:
  
  s->scanning = SANE_TRUE;
  s->page += 1; /* processing next page */
  
  if (pipe (fds) < 0) {
    return SANE_STATUS_IO_ERROR;
  }
  
  s->read_fds = fds[0];
  s->write_fds = fds[1];

  /* create reader routine as new process or thread */
  DBG (3, "sane_start: starting thread\n");
  s->reader_pid = sanei_thread_begin (reader_process, (void *) s);
  
  if (sanei_thread_is_forked())
	close (s->write_fds);
 
  return SANE_STATUS_GOOD;
  
 stop_scanner_and_return:
  
  /* cancel the scan nicely */
  do_cancel (s);
  
  return status;
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte* buf, SANE_Int max_len, SANE_Int* len)
{
  Avision_Scanner* s = handle;
  ssize_t nread;
  *len = 0;

  DBG (8, "sane_read: max_len: %d\n", max_len);

  nread = read (s->read_fds, buf, max_len);
  if (nread > 0) {
    DBG (8, "sane_read: got %ld bytes\n", (long) nread);
  }
  else {
    DBG (3, "sane_read: got %ld bytes, err: %d %s\n", (long) nread, errno, strerror(errno));
  }

  if (!s->scanning)
    return SANE_STATUS_CANCELLED;
  
  if (nread < 0) {
    if (errno == EAGAIN) {
      return SANE_STATUS_GOOD;
    } else {
      do_cancel (s);
      return SANE_STATUS_IO_ERROR;
    }
  }
  
  *len = nread;
  
  /* if all data was passed through */
  if (nread == 0)
    return do_eof (s);

  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  Avision_Scanner* s = handle;
  DBG (3, "sane_cancel:\n");

  /* always do the housekeeping, e.g. flush batch scanner pages */
  do_cancel (s);
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  Avision_Scanner* s = handle;
  
  DBG (3, "sane_set_io_mode:\n");
  if (!s->scanning) {
    DBG (3, "sane_set_io_mode: not yet scanning\n");
    return SANE_STATUS_INVAL;
  }
  
  if (fcntl (s->read_fds, F_SETFL, non_blocking ? O_NONBLOCK : 0) < 0)
    return SANE_STATUS_IO_ERROR;
  
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int* fd)
{
  Avision_Scanner* s = handle;
  
  DBG (3, "sane_get_select_fd:\n");
  
  if (!s->scanning) {
    DBG (3, "sane_get_select_fd: not yet scanning\n");
    return SANE_STATUS_INVAL;
  }
  
  *fd = s->read_fds;
  return SANE_STATUS_GOOD;
}
