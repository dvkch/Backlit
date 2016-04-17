/* sane - Scanner Access Now Easy.

   pieusb_scancmd.h

   Copyright (C) 2012-2015 Jan Vleeshouwers, Michael Rickmann, Klaus Kaempf

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
   If you do not wish that, delete this exception notice.  */

#ifndef PIEUSB_SCANCMD_H
#define	PIEUSB_SCANCMD_H

#include "../include/sane/sane.h"

/* =========================================================================
 *
 * Data-structures used by scanner commands
 *
 * For SENSE descriptions, see SCSI-2 p158, table 67 (p469 ASC/Q alphabetically)
 * For the INQUIRY command, see SCSI-2 p141 table 45, 46, 47
 *
 * 2-byte short ints are represented by 4-byte SANE_Int types
 *
 * ========================================================================= */

struct Pieusb_Scanner_Properties {
    SANE_Byte deviceType; /* 0x06 = scanner */
    SANE_Byte additionalLength; /* including this byte: 0xb4 = 180, so total structure 184 bytes */
    SANE_Char vendor[9]; /* actually 8 bytes, not null-terminated ...PIE     ... */
    SANE_Char product[17]; /* actually 16 bytes, null-terminated ...SF Scanner... */
    SANE_Char productRevision[5]; /* actually 4 bytes, not null-terminated ...1.70... */
    /* 1st Vendor-specific block, 20 bytes, see pie_get_inquiry_values(), partially: */
    SANE_Int maxResolutionX; /* 7200 maximum scan resolution in x direction */
    SANE_Int maxResolutionY; /* 7200 maximum scan resolution in y direction */
    SANE_Int maxScanWidth; /* 10680 flatbed_max_scan_width (& calibration block size) */
    SANE_Int maxScanHeight; /* 6888 flatbed_max_scan_height */
    SANE_Byte filters; /* 0x9e = 10011110 ?-0-0-OnePassColor-B-G-R-N => additional infrared? */
    SANE_Byte colorDepths; /* 0x35 = 00110101 0-0-16-12-10-8-4-1 */
    SANE_Byte colorFormat; /* 0x07 = 00000111 0-0-0-0-0-Index-Line-Pixel */
    SANE_Byte imageFormat; /* 0x09 = 00001001 0-0-0-0-OKLine-BlkOne-Motorola-Intel */
    SANE_Byte scanCapability;
      /* 0x4b = 01001011 PowerSave-ExtCal-0-FastPreview-DisableCal-[CalSpeeds=3]
       * PowerSave: no
       * ExtCal: yes =>
       * FastPreview: no
       * DisableCal: yes => can calibration be disabled?
       * CalSpeeds: 3 => 1 line, 13 lines, 31 lines */
    SANE_Byte optionalDevices;
      /* 0x61 = 01100001 MultiPageLoad-?-?-0-0-TransModule1-TransModule-AutoDocFeeder => additional?
       * MultiPageLoad: no
       * ?: yes
       * ?: yes
       * TransModule1: no
       * TransModule: no
       * AutoDocFeeder: yes */
    SANE_Byte enhancements; /* 0x02 = no info in pie.c */
    SANE_Byte gammaBits; /* 0x0c = 00001100 = 12 ? used when downloading gamma table ... does not happen */
    SANE_Byte lastFilter; /* 0x00 = ? no info in pie.c, not used */
    SANE_Int previewScanResolution; /* 0x2c01 = 300 fast preview scan resolution */
    /* Reserved (56-95) */
    /* SANE_Byte div_56[40]; */
    /* 2nd vendor specific block (36 bytes at offset 96) */
    SANE_Char firmwareVersion[5]; /* actually 4 bytes, not null terminated "1.05" */
    SANE_Byte halftones; /* 0x08 = halftones (4 LSbits) = 00001000 ? */
    SANE_Byte minumumHighlight; /* 0x64 = 100 */
    SANE_Byte maximumShadow; /* 0x64 = 100 */
    SANE_Byte calibrationEquation; /* 0x01 ? see pie_perform_cal() */
    SANE_Int maximumExposure; /* 0xc409 = 2500 (units?) */
    SANE_Int minimumExposure; /* 0x6400 = 100 (units?) */
    SANE_Int x0; /* 0xd002 = 720 transparency top left x */
    SANE_Int y0; /* 0x8604 = 1158 transparency top left y */
    SANE_Int x1; /* 0xbc10 = 4284 transparency bottom right x */
    SANE_Int y1; /* 0xc015 = 5568 transparency bottom right y */
    SANE_Int model; /* 0x3000 => model number */
    /* SANE_Int div_118; 0x0000 meaning? */
    SANE_Char production[4]; /* null terminated */
    SANE_Char timestamp[20]; /* null terminated */
    SANE_Byte signature[40]; /* null terminated */
};

struct Pieusb_Sense {
    /* 14 bytes according to SCSI-2 p158, table 67 (p469 ASC/Q alphabetically) */
    SANE_Byte errorCode; /* 0x70 or 0x71 */
    SANE_Byte segment;
    SANE_Byte senseKey; /* sense key is actually this value & 0x0F - table 69 */
    SANE_Byte info[4];
    SANE_Byte addLength; /* should be 0x07 (remaining struct length including this byte) */
    SANE_Byte cmdInfo[4]; /* command specific information */
    SANE_Byte senseCode; /* abbreviated ASC - table 71 */
    SANE_Byte senseQualifier; /* abbreviated ASCQ - table 71 */
};

struct Pieusb_Scanner_State {
    SANE_Byte buttonPushed; /* 0x01 if pushed */
    SANE_Byte warmingUp; /* 0x01 if warming up, 0x00 if not */
    SANE_Byte scanning; /* bit 6 set if SCAN active, bit 7 motor direction inverted (not analysed in detail) */
};

struct Pieusb_Scan_Parameters {
    SANE_Int width; /* Number of pixels on a scan line */
    SANE_Int lines; /* Number of lines in the scan. Value depends on color format, see Pieusb_Mode. */
    SANE_Int bytes; /* Number of bytes on a scan line. Value depends on color format. */
    SANE_Byte filterOffset1; /* 0x08 in the logs, but may also be set to 0x16, they seem to be used in “line”-format only. */
    SANE_Byte filterOffset2; /* 0x08 in the logs, but may also be set to 0x16, they seem to be used in “line”-format only. */
    SANE_Int period; /* Seems to be unused */
    SANE_Int scsiTransferRate; /* Don't use, values cannot be trusted */
    SANE_Int availableLines; /* The number of currently available scanned lines. Value depends on color format. Returns a value >0 if PARAM is called while scanning is in progress */
    SANE_Byte motor; /* Motor direction in bit 0 */
};

struct Pieusb_Mode {
    /* SANE_Byte size; of remaining data, not useful */
    SANE_Int resolution; /* in dpi */
    SANE_Byte passes;
      /* 0x80 = One pass color; 0x90 = One pass RGBI;
       * bit 7 : one-pass-color bit (equivalent to RGB all set?)
       * bit 6 & 5: unused
       * bit 4 : Infrared
       * bit 3 : Blue
       * bit 2 : Green
       * bit 1 : Red
       * bit 0: Neutral (not supported, ignored) */
    SANE_Byte colorDepth;
      /* 0x04 = 8, 0x20 = 16 bit
       * bit 7 & 6 : 0 (unused)
       * bit 5 : 16 bit
       * bit 4 : 12 bit
       * bit 3 : 10 bit
       * bit 2 : 8 bit
       * bit 1 : 4 bit
       * bit 0 : 1 bit */
    SANE_Byte colorFormat;
      /* 0x04 = index, cf. INQUIRY
       * bit 7-3 : 0 (unused)
       * bit 2 : Index = scanned data are lines preceeded by a two-byte index, 'RR', 'GG', 'BB', or 'II'
       * bit 1 : Line =  scanned data are (probably) lines in RGBI order (needs testing)
       * bit 0 : Pixel = scanned data are always RGB-pixels, i.e. 3x2 bytes at depth = 16 bits, 3 bytes
       *                 at depth = 8 bits, and 3 packed bytes at depth = 1. This is also the case in
       *                 a single color or gray scale scan; in these cases only the first pixel contains
       *                 valid data. */
    SANE_Byte byteOrder; /* 0x01 = Intel; only bit 0 used */
    SANE_Bool sharpen; /* byte 9 bit 1 */
    SANE_Bool skipShadingAnalysis; /* byte 9 bit 3 */
    SANE_Bool fastInfrared; /* byte 9 bit 7 */
      /* bit 7 : “fast infrared” flag
       * bit 6,5,4 : 0 (unused)
       * bit 3 : “skip calibration” flag (skip collecting shading information)
       * bit 2 : 0 (unused)
       * bit 1 : “sharpen” flag (only effective with fastInfrared off, one-pass-color and no extra BADF-entries)
       * bit 0 : 0 (unused) */
    SANE_Byte halftonePattern; /* 0x00 = no halftone pattern */
    SANE_Byte lineThreshold; /* 0xFF = 100% */
};

struct Pieusb_Settings {
    SANE_Int saturationLevel[3];
      /* The average pixel values for the three colors Red, Green and Blue,
       * which are the result of optimizing the Timer 1 counts so that Red and
       * Blue values are least 90% of full scale (58981) and the Green value is
       * at least 80% (52428). These levels are only determined during warming up. */
    SANE_Int exposureTime[4];
      /* Optimized exposure times for Red, Green and Blue. The exposure times are
       * Timer 1 counts which define when Timer 1 interrupts. These values are
       * only determined at startup.
       * Exposure time for Infrared. The value is optimized and set at startup
       * with the other exposure times. Quite often, it is subsequently reset to
       * a default value (0x0B79). */
    SANE_Word offset[4];
      /* Optimized offsets for Red, Green and Blue. See above. These values are
       * also updated before outputting the CCD-mask.
       * Element 4 is offset for Infrared. */
    SANE_Word gain[4];
      /* Optimized gains for Red, Green and Blue. See the remark for
       * exposureTime above. Element 4 is gain for Infrared. */
    SANE_Byte light;
      /* Current light level. The stability of the light source is tested during
       * warming up. The check starts with a light value 7 or 6, and decrements
       * it when the light warms up. At a light value of 4, the scanner produces
       * stable scans (i.e. successive “white” scan values don't differ more
       * than 0x200). */
    SANE_Int minimumExposureTime;
      /* Fixed value: 0x0b79 (2937) */
    SANE_Byte extraEntries;
    SANE_Byte doubleTimes;
      /* Originally 20 unused bytes (uninitialized memory)
       * To complete the mapping to the Pieusb_Settings_Condensed struct,
       * the last two bytes are given an explicit meaning. */
    /* SANE_Int exposureTimeIR; */
    /* SANE_Byte offsetIR; */
    /* SANE_Byte gainIR; */
};

/* Not used, Pieusb_Settings contains the same fields (after a bit of juggling) */
struct Pieusb_Settings_Condensed {
    SANE_Int exposureTime[4]; /* => Pieusb_Settings.exposureTime */
    SANE_Byte offset[4]; /* => Pieusb_Settings.offset */
    SANE_Byte gain[4]; /* => Pieusb_Settings.gain */
    SANE_Byte light; /* => Pieusb_Settings.light */
    SANE_Byte extraEntries; /* => Pieusb_Settings.extraEntries */
    SANE_Byte doubleTimes; /* => Pieusb_Settings.doubleTimes */
};

struct Pieusb_Halftone_Pattern {
    SANE_Int code; /* 0x91 */
    /*TODO */
};

struct Pieusb_Scan_Frame {
    SANE_Int index; /* scan frame index (0-7) */
    SANE_Int x0; /* top left, is origin */
    SANE_Int y0;
    SANE_Int x1; /* bottom right */
    SANE_Int y1;
};

struct Pieusb_Exposure_Time_Color {
    SANE_Int filter; /* color mask 0x02, 0x04 or 0x08 for R, G, B */
    SANE_Int value; /* relative exposure time 0 - 100 */
};

struct Pieusb_Exposure_Time {
    SANE_Int code; /* 0x93 */
    SANE_Int size; /* number of bytes in rest of structure */
    struct Pieusb_Exposure_Time_Color color[3]; /* not all elements may actually be used */
};

struct Pieusb_Highlight_Shadow_Color {
    SANE_Int filter; /* color mask 0x02, 0x04 or 0x08 for R, G, B */
    SANE_Int value; /* 0-100 */
};

struct Pieusb_Highlight_Shadow {
    SANE_Int code; /* 0x94 */
    SANE_Int size; /* number of bytes in rest of structure */
    struct Pieusb_Highlight_Shadow_Color color[3];
};

struct Pieusb_Shading_Parameters_Info {
    SANE_Byte type; /* 0x00, 0x08, 0x10, 0x20; RGBI(?) */
    SANE_Byte sendBits; /* 0x10 = 16 */
    SANE_Byte recieveBits; /* 0x10 = 16 */
    SANE_Byte nLines; /* 0x2D = 45 */
    SANE_Int pixelsPerLine; /* 0x14dc = 5340 */
};

#define SHADING_PARAMETERS_INFO_COUNT 4
struct Pieusb_Shading_Parameters {
    SANE_Byte code; /* 0x95 */
    SANE_Int size; /* number of bytes in rest of structure (0x1c=28) */
    SANE_Byte calInfoCount; /* number of individual info structures (=0x04) */
    SANE_Byte calInfoSize; /* size of individual info structure (=0x06) */
    SANE_Int div_6; /* 0x0004, meaning not clear */
    struct Pieusb_Shading_Parameters_Info cal[SHADING_PARAMETERS_INFO_COUNT];
};

typedef enum {
  PIEUSB_STATUS_GOOD = 0,	/*  0 everything A-OK */
  PIEUSB_STATUS_UNSUPPORTED,	/*  1 operation is not supported */
  PIEUSB_STATUS_CANCELLED,	/*  2 operation was cancelled */
  PIEUSB_STATUS_DEVICE_BUSY,	/*  3 device is busy; try again later */
  PIEUSB_STATUS_INVAL,		/*  4 data is invalid (includes no dev at open) */
  PIEUSB_STATUS_EOF,		/*  5 no more data available (end-of-file) */
  PIEUSB_STATUS_JAMMED,		/*  6 document feeder jammed */
  PIEUSB_STATUS_NO_DOCS,	/*  7 document feeder out of documents */
  PIEUSB_STATUS_COVER_OPEN,	/*  8 scanner cover is open */
  PIEUSB_STATUS_IO_ERROR,	/*  9 error during device I/O */
  PIEUSB_STATUS_NO_MEM,		/* 10 out of memory */
  PIEUSB_STATUS_ACCESS_DENIED,	/* 11 access to resource has been denied */
  PIEUSB_STATUS_WARMING_UP,     /* 12 lamp not ready, please retry */
  PIEUSB_STATUS_HW_LOCKED,      /* 13 scanner mechanism locked for transport */
  PIEUSB_STATUS_MUST_CALIBRATE  /* 14 */
} PIEUSB_Status;

/* Structures used by the USB functions */

struct Pieusb_Command_Status {
    PIEUSB_Status pieusb_status;
    SANE_Byte senseKey; /* sense key: see Pieusb_Sense */
    SANE_Byte senseCode; /* sense code */
    SANE_Byte senseQualifier; /* sense code qualifier */
};

typedef struct Pieusb_Scanner_Properties Pieusb_Scanner_Properties;

typedef enum {
  SLIDE_NEXT = 0x04, SLIDE_PREV = 0x05, SLIDE_LAMP_ON = 0x10, SLIDE_RELOAD = 0x40
} slide_action;

void sanei_pieusb_cmd_slide(SANE_Int device_number, slide_action action, struct Pieusb_Command_Status *status);

/* Scanner commands */

void sanei_pieusb_cmd_test_unit_ready(SANE_Int device_number, struct Pieusb_Command_Status *status);

void sanei_pieusb_cmd_get_sense(SANE_Int device_number, struct Pieusb_Sense* sense, struct Pieusb_Command_Status *status, PIEUSB_Status *ret);

void sanei_pieusb_cmd_get_halftone_pattern(SANE_Int device_number, SANE_Int index, struct Pieusb_Halftone_Pattern* pattern, struct Pieusb_Command_Status *status);
void sanei_pieusb_cmd_set_halftone_pattern(SANE_Int device_number, SANE_Int index, struct Pieusb_Halftone_Pattern* pattern, struct Pieusb_Command_Status *status);

void sanei_pieusb_cmd_get_scan_frame(SANE_Int device_number, SANE_Int index, struct Pieusb_Scan_Frame* frame, struct Pieusb_Command_Status *status);
void sanei_pieusb_cmd_set_scan_frame(SANE_Int device_number, SANE_Int index, struct Pieusb_Scan_Frame* frame, struct Pieusb_Command_Status *status);

void sanei_pieusb_cmd_17(SANE_Int device_number, SANE_Int value, struct Pieusb_Command_Status *status);

void sanei_pieusb_cmd_get_exposure_time(SANE_Int device_number, SANE_Int colorbits, struct Pieusb_Exposure_Time* time, struct Pieusb_Command_Status *status);
void sanei_pieusb_cmd_set_exposure_time(SANE_Int device_number, struct Pieusb_Exposure_Time* time, struct Pieusb_Command_Status *status);

void sanei_pieusb_cmd_get_highlight_shadow(SANE_Int device_number, SANE_Int colorbits, struct Pieusb_Highlight_Shadow* hgltshdw, struct Pieusb_Command_Status *status);
void sanei_pieusb_cmd_set_highlight_shadow(SANE_Int device_number, struct Pieusb_Highlight_Shadow* hgltshdw, struct Pieusb_Command_Status *status);

void sanei_pieusb_cmd_get_shading_parms(SANE_Int device_number, struct Pieusb_Shading_Parameters_Info* shading, struct Pieusb_Command_Status *status);
void sanei_pieusb_cmd_get_scanned_lines(SANE_Int device_number, SANE_Byte* data, SANE_Int lines, SANE_Int size, struct Pieusb_Command_Status *status);

void sanei_pieusb_cmd_get_ccd_mask(SANE_Int device_number, SANE_Byte* mask, SANE_Int mask_size, struct Pieusb_Command_Status *status);
void sanei_pieusb_cmd_set_ccd_mask(SANE_Int device_number, SANE_Byte colorbits, SANE_Byte* mask, SANE_Int mask_size, struct Pieusb_Command_Status *status);

/*
void cmdPrepareHalftonePattern(SANE_Int device_number, SANE_Int index, struct Pieusb_Command_Status *status);
void cmdPrepareScanFrame(SANE_Int device_number, SANE_Int index, struct Pieusb_Command_Status *status);
void cmdPrepareRelativeExposureTime(SANE_Int device_number, SANE_Int colorbits, struct Pieusb_Command_Status *status);
void cmdPrepareHighlightShadow(SANE_Int device_number, SANE_Int colorbits, struct Pieusb_Command_Status *status);
void cmdPrepareShadingParameters(SANE_Int device_number, struct Pieusb_Command_Status *status);
*/

void sanei_pieusb_cmd_get_parameters(SANE_Int device_number, struct Pieusb_Scan_Parameters* parameters, struct Pieusb_Command_Status *status);

void sanei_pieusb_cmd_inquiry(SANE_Int device_number, struct Pieusb_Scanner_Properties* inq, SANE_Byte size, struct Pieusb_Command_Status *status);

void sanei_pieusb_cmd_get_mode(SANE_Int device_number, struct Pieusb_Mode* mode, struct Pieusb_Command_Status *status);
void sanei_pieusb_cmd_set_mode(SANE_Int device_number, struct Pieusb_Mode* mode, struct Pieusb_Command_Status *status);

void sanei_pieusb_cmd_start_scan(SANE_Int device_number, struct Pieusb_Command_Status *status);
void sanei_pieusb_cmd_stop_scan(SANE_Int device_number, struct Pieusb_Command_Status *status);

void sanei_pieusb_cmd_set_scan_head(SANE_Int device_number, SANE_Int mode, SANE_Int steps, struct Pieusb_Command_Status *status);

void sanei_pieusb_cmd_get_gain_offset(SANE_Int device_number, struct Pieusb_Settings* settings, struct Pieusb_Command_Status *status);

void sanei_pieusb_cmd_set_gain_offset(SANE_Int device_number, struct Pieusb_Settings* settings, struct Pieusb_Command_Status *status);

void sanei_pieusb_cmd_read_state(SANE_Int device_number, struct Pieusb_Scanner_State* state, struct Pieusb_Command_Status *status);

#endif	/* PIEUSB_SCANCMD_H */
