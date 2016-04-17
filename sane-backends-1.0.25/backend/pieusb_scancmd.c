/* sane - Scanner Access Now Easy.

   pieusb_scancmd.c

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

/* =========================================================================
 *
 * Pieusb scanner commands
 *
 * Each scanner command has its own function.
 * See the sort description preceeding each function.
 *
 * ========================================================================= */

#define DEBUG_DECLARE_ONLY

#include "pieusb.h"
#include "pieusb_scancmd.h"
#include "pieusb_usb.h"

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include <time.h> /* for time() */

static void _prep_scsi_cmd(SANE_Byte* command_bytes, SANE_Byte command, SANE_Word size);

/* =========================================================================
 *
 * Pieusb utility functions
 *
 * ========================================================================= */

/*
 * Get the unsigned char value in the array at given offset
 */
static SANE_Byte
_get_byte(SANE_Byte* array, SANE_Byte offset) {
    return *(array+offset);
}

/*
 * Set the array at given offset to the given unsigned char value
 */
static void
_set_byte(SANE_Byte val, SANE_Byte* array, SANE_Byte offset) {
    *(array+offset) = val;
}


/*
 * Get the unsigned short value in the array at given offset.
 * All data in structures is little-endian, so the LSB comes first
 * SANE_Int is 4 bytes, but that is not a problem.
 */
static SANE_Int
_get_short(SANE_Byte* array, SANE_Byte offset) {
    SANE_Int i = *(array+offset+1);
    i <<= 8;
    i += *(array+offset);
    return i;
}


/*
 * Put the bytes of a short int value into an unsigned char array
 * All data in structures is little-endian, so start with LSB
 */
static void
_set_short(SANE_Word val, SANE_Byte* array, SANE_Byte offset) {
    *(array+offset) = val & 0xFF;
    *(array+offset+1) = (val>>8) & 0xFF;
}


/*
 * Get the signed int value in the array at given offset.
 * All data in structures is little-endian, so the LSB comes first
 */
static SANE_Int
_get_int(SANE_Byte* array, SANE_Byte offset) {
    SANE_Int i = *(array+offset+3);
    i <<= 8;
    i += *(array+offset+2);
    i <<= 8;
    i += *(array+offset+1);
    i <<= 8;
    i += *(array+offset);
    return i;
}

#if 0 /* unused */
/*
 * Put the bytes of a signed int value into an unsigned char array
 * All data in structures is little-endian, so start with LSB
 */
static void
_set_int(SANE_Word val, SANE_Byte* array, SANE_Byte offset) {
    *(array+offset) = val & 0xFF;
    *(array+offset+1) = (val>>8) & 0xFF;
    *(array+offset+2) = (val>>16) & 0xFF;
    *(array+offset+3) = (val>>24) & 0xFF;
}
#endif

/*
 * Copy count unsigned char values from src to dst
 */
static void
_copy_bytes(SANE_Byte* dst, SANE_Byte* src, SANE_Byte count) {
    SANE_Byte k;
    for (k=0; k<count; k++) {
        *dst++ = *src++;
    }
}


/*
 * Get count unsigned short values in the array at given offset.
 * All data in structures is little-endian, so the MSB comes first.
 * SANE_Word is 4 bytes, but that is not a problem.
 */
static void
_get_shorts(SANE_Word* dst, SANE_Byte* src, SANE_Byte count) {
    SANE_Byte k;
    for (k=0; k<count; k++) {
        *dst = *(src+2*k+1);
        *dst <<= 8;
        *dst++ += *(src+2*k);
    }
}

/*
 * Copy an unsigned short array of given size
 * All data in structures is little-endian, so start with MSB of each short.
 * SANE_Word is 4 bytes, but that is not a problem. All MSB 2 bytes are ignored.
 */
static void
_set_shorts(SANE_Word* src, SANE_Byte* dst, SANE_Byte count) {
    SANE_Byte k;
    for (k=0; k<count; k++) {
        *(dst+2*k) = *src & 0xFF;
        *(dst+2*k+1) = ((*src++)>>8) & 0xFF;
    }
}


/**
 * Perform a TEST UNIT READY (SCSI command code 0x00)
 * Returns status->pieusb_status:
 * - PIEUSB_STATUS_GOOD if device is ready
 * - PIEUSB_STATUS_DEVICE_BUSY if device is still busy after timeout
 * - other SANE status code if TEST UNIT READY failed or if it returned
 *   CHECK CONDITION and REQUEST SENSE failed
 *
 * @param device_number Device number
 * @return Pieusb_Command_Status SANE_STATUS_GOOD if ready, SANE_STATUS_DEVICE_BUSY if not
 */
void
sanei_pieusb_cmd_test_unit_ready(SANE_Int device_number, struct Pieusb_Command_Status *status)
{
    SANE_Byte command[SCSI_COMMAND_LEN];

    DBG (DBG_info_scan, "sanei_pieusb_cmd_test_unit_ready()\n");

    _prep_scsi_cmd (command, SCSI_TEST_UNIT_READY, 0);

    status->pieusb_status = sanei_pieusb_command (device_number, command, NULL, 0);

    DBG (DBG_info_scan, "sanei_pieusb_cmd_test_unit_ready() return status = %s\n", sane_strstatus(status->pieusb_status));
}

/**
 * slide action
 * @param action  SLIDE_NEXT, SLIDE_PREV, SLIDE_LAMP_ON, SLIDE_RELOAD
 * @return Pieusb_Command_Status
 */

void
sanei_pieusb_cmd_slide(SANE_Int device_number, slide_action action, struct Pieusb_Command_Status *status)
{
    SANE_Byte command[SCSI_COMMAND_LEN];
#define SLIDE_DATA_SIZE 4
    SANE_Byte data[SLIDE_DATA_SIZE];

    DBG (DBG_info_scan, "sanei_pieusb_cmd_slide(0x%02x)\n", action);

    _prep_scsi_cmd(command, SCSI_SLIDE, SLIDE_DATA_SIZE);
    memset(data, '\0', SLIDE_DATA_SIZE);
    data[0] = action;
    data[1] = 0x01;

    status->pieusb_status = sanei_pieusb_command(device_number, command, data, SLIDE_DATA_SIZE);
#undef SLIDE_DATA_SIZE
}

/**
 * Perform a REQUEST SENSE (SCSI command code 0x03)
 * Returns status->pieusb_status:
 * - PIEUSB_STATUS_GOOD is the command executes OK
 * - other SANE status code if REQUEST SENSE fails
 * The sense fields in status are always 0. A REQUEST SENSE is not repeated if
 * the device returns PIEUSB_STATUS_DEVICE_BUSY.
 *
 * @param device_number Device number
 * @param sense Sense data
 * @param status Command result status
 * @see struc Pieusb_Sense
 */
void
sanei_pieusb_cmd_get_sense(SANE_Int device_number, struct Pieusb_Sense* sense, struct Pieusb_Command_Status *status, PIEUSB_Status *ret)
{
    SANE_Byte command[SCSI_COMMAND_LEN];
#define DATA_SIZE 14
    SANE_Int size = DATA_SIZE;
    SANE_Byte data[DATA_SIZE];
    PIEUSB_Status st;
  SANE_Char* sd;

    DBG (DBG_info_scan, "sanei_pieusb_cmd_get_sense()\n");

    _prep_scsi_cmd(command, SCSI_REQUEST_SENSE, size);

    memset(data, '\0', size);
    st = sanei_pieusb_command(device_number, command, data, size);
    if (st != PIEUSB_STATUS_GOOD) {
      status->pieusb_status = st;
      /*FIXME*/
        return;
    }

    /* Decode data recieved */
    sense->errorCode = _get_byte (data, 0);
    sense->segment = _get_byte (data, 1);
    sense->senseKey = _get_byte (data, 2);
    _copy_bytes (sense->info, data+3, 4);
    sense->addLength = _get_byte (data, 7);
    _copy_bytes (sense->cmdInfo, data+8, 4);
    sense->senseCode = _get_byte (data, 12);
    sense->senseQualifier = _get_byte (data, 13);
    status->pieusb_status = PIEUSB_STATUS_GOOD;
#undef DATA_SIZE
  DBG (DBG_info_scan, "\tsense details:\n");
  DBG (DBG_info_scan, "\t\terror......... : 0x%02x\n", sense->errorCode);
  DBG (DBG_info_scan, "\t\tsegment....... : %d\n", sense->segment);
  DBG (DBG_info_scan, "\t\tsenseKey...... : 0x%02x\n", sense->senseKey);
  DBG (DBG_info_scan, "\t\tinfo.......... : %02x %02x %02x %02x\n", sense->info[0], sense->info[1], sense->info[2], sense->info[3]);
  DBG (DBG_info_scan, "\t\taddLength..... : %d\n", sense->addLength);
  DBG (DBG_info_scan, "\t\tcmdInfo....... : %02x %02x %02x %02x\n", sense->cmdInfo[0], sense->cmdInfo[1], sense->cmdInfo[2], sense->cmdInfo[3]);
  DBG (DBG_info_scan, "\t\tsenseCode..... : 0x%02x\n", sense->senseCode);
  DBG (DBG_info_scan, "\t\tsenseQualifier : 0x%02x\n", sense->senseQualifier);
  sd = sanei_pieusb_decode_sense (sense, ret?ret:&st);
  DBG (DBG_info_scan, "\tsense: %s\n", sd);
  free(sd);
}

/**
 * Read the halftone pattern with the specified index. This requires two
 * commands, one to ask the device to prepare the pattern, and one to read it.
 *
 * @param device_number Device number
 * @param index index of halftone pattern
 * @param pattern Halftone pattern (not implemented)
 * @return Pieusb_Command_Status
 * @see Pieusb_Halftone_Pattern
 */
void
sanei_pieusb_cmd_get_halftone_pattern(SANE_Int device_number, SANE_Int index, struct Pieusb_Halftone_Pattern* pattern, struct Pieusb_Command_Status *status)
{
    SANE_Byte command[SCSI_COMMAND_LEN];
#define PATTERN_SIZE 256 /* Assumed maximum pattern size */
    SANE_Int size = PATTERN_SIZE;
    SANE_Byte data[PATTERN_SIZE];
    int psize;
    SANE_Char* desc;
    PIEUSB_Status st;

    DBG (DBG_info_scan, "sanei_pieusb_cmd_get_halftone_pattern()\n");

    /* Ask scanner to prepare the pattern with the given index. Only SCSI_COMMAND_LEN bytes of data. */
    _prep_scsi_cmd(command, SCSI_WRITE, SCSI_COMMAND_LEN);
    memset(data, '\0', SCSI_COMMAND_LEN);
    data[0] = SCSI_HALFTONE_PATTERN | 0x80; /* set bit 7 means prepare read */
    data[4] = index;

    st = sanei_pieusb_command(device_number, command, data, SCSI_COMMAND_LEN);
    if (st != PIEUSB_STATUS_GOOD) {
      status->pieusb_status = st;
      /* FIXME */
        return;
    }

    /* Read pattern */
    _prep_scsi_cmd(command, SCSI_READ, size);

    memset(data, '\0', size);
    status->pieusb_status = sanei_pieusb_command (device_number, command, data, size);

    /*FIXME: analyse */
    fprintf(stderr, "Halftone pattern %d:\n", index);
    psize = (data[3]<<8) + data[2];
    desc = (SANE_Char*)(data + 4 + psize);
    data[4 + psize + 16] = '\0';
    fprintf(stderr,"Descr. offset from byte 4 = %d, %16s, index = %d, size = %dx%d\n", psize, desc, data[4]&0x7F, data[6], data[7]);
#undef PATTERN_SIZE
}

/**
 * Read the scan frame with the specified index. This requires two
 * commands, one to ask the device to prepare the pattern, and one to read it.
 *
 * @param device_number Device number
 * @param frame Scan frame
 * @return Pieusb_Command_Status
 * @see Pieusb_Scan_Frame
 */
void
sanei_pieusb_cmd_get_scan_frame(SANE_Int device_number, SANE_Int index, struct Pieusb_Scan_Frame* frame, struct Pieusb_Command_Status *status)
{
    SANE_Byte command[SCSI_COMMAND_LEN];
#define FRAME_SIZE 256 /* Assumed maximum frame size */
    SANE_Int size = FRAME_SIZE;
    SANE_Byte data[FRAME_SIZE];
    PIEUSB_Status st;

    DBG (DBG_info_scan, "sanei_pieusb_cmd_get_scan_frame()\n");

    /* Ask scanner to prepare the scan frame with the given index. Only SCSI_COMMAND_LEN bytes of data. */
    _prep_scsi_cmd (command, SCSI_WRITE, SCSI_COMMAND_LEN);
    memset (data, '\0', SCSI_COMMAND_LEN);
    data[0] = SCSI_SCAN_FRAME | 0x80; /* set bit 7 means prepare read */
    data[4] = index;

    st = sanei_pieusb_command (device_number, command, data, SCSI_COMMAND_LEN);
    if (st != PIEUSB_STATUS_GOOD) {
      status->pieusb_status = st;
      /* FIXME */
        return;
    }

    /* Read scan frame */
    _prep_scsi_cmd (command, SCSI_READ, size);

    memset(data, '\0', size);
    status->pieusb_status = sanei_pieusb_command (device_number, command, data, size);

    /* Decode data */
    frame->index = _get_byte (data, 4);
    frame->x0 = _get_short (data, 6);
    frame->y0 = _get_short (data, 8);
    frame->x1 = _get_short (data, 10);
    frame->y1 = _get_short (data, 12);

    DBG (DBG_info_scan, "sanei_pieusb_cmd_get_scan_frame() set:\n");
    DBG (DBG_info_scan, " x0,y0 = %d,%d\n", frame->x0, frame->y0);
    DBG (DBG_info_scan, " x1,y1 = %d,%d\n", frame->x1, frame->y1);
    DBG (DBG_info_scan, " index = %d\n", frame->index);
#undef FRAME_SIZE
}

/**
 * command 17 - unknown
 */

void
sanei_pieusb_cmd_17(SANE_Int device_number, SANE_Int value, struct Pieusb_Command_Status *status)
{
    SANE_Byte command[SCSI_COMMAND_LEN];
#define CMD_17_SIZE 6
    SANE_Byte data[CMD_17_SIZE];

    DBG (DBG_info_scan, "sanei_pieusb_cmd_17(%d)\n", value);

    _prep_scsi_cmd (command, SCSI_WRITE, CMD_17_SIZE);
    memset (data, '\0', CMD_17_SIZE);
    _set_short (SCSI_CMD_17, data, 0);
    _set_short (2, data, 2);
    _set_short (value, data, 4);

    status->pieusb_status = sanei_pieusb_command (device_number, command, data, CMD_17_SIZE);
#undef CMD_17_SIZE
    if (status->pieusb_status != PIEUSB_STATUS_GOOD) {
      DBG (DBG_info_scan, "sanei_pieusb_cmd_17 failed: 0x%02x\n", status->pieusb_status);
      return;
    }
}

/**
 * Read the relative exposure time for the specified colorbits. This requires two
 * commands, one to ask the device to prepare the value, and one to read it.
 *
 * @param device_number Device number
 * @param time Relative exposure time(s)
 * @return Pieusb_Command_Status
 * @see Pieusb_Exposure_Time
 */
void
sanei_pieusb_cmd_get_exposure_time(SANE_Int device_number, SANE_Int colorbits, struct Pieusb_Exposure_Time* time, struct Pieusb_Command_Status *status)
{
    DBG (DBG_info_scan, "sanei_pieusb_cmd_get_exposure_time(): not implemented\n");
    status->pieusb_status = PIEUSB_STATUS_INVAL;
}

/**
 * Read the highlight and shadow levels with the specified colorbits. This requires two
 * commands, one to ask the device to prepare the value, and one to read it.
 *
 * @param device_number Device number
 * @param hgltshdw Highlight and shadow level(s)
 * @return Pieusb_Command_Status
 * @see Pieusb_Highlight_Shadow
 */
void
sanei_pieusb_cmd_get_highlight_shadow(SANE_Int device_number, SANE_Int colorbits, struct Pieusb_Highlight_Shadow* hgltshdw, struct Pieusb_Command_Status *status)
{
    DBG (DBG_info_scan, "sanei_pieusb_cmd_get_highlight_shadow(): not implemented\n");
    status->pieusb_status = PIEUSB_STATUS_INVAL;
}

/**
 * Read the shading data parameters. This requires two
 * commands, one to ask the device to prepare the value, and one to read it.
 *
 * @param device_number Device number
 * @param shading Shading data parameters
 * @return Pieusb_Command_Status
 * @see Pieusb_Shading_Parameters
 */
void
sanei_pieusb_cmd_get_shading_parms(SANE_Int device_number, struct Pieusb_Shading_Parameters_Info* shading, struct Pieusb_Command_Status *status)
{
    SANE_Byte command[SCSI_COMMAND_LEN];
#define SHADING_SIZE 32
#define PREP_READ_SIZE 6
    SANE_Int size = SHADING_SIZE;
    SANE_Byte data[SHADING_SIZE];
    int k;

    DBG (DBG_info_scan, "sanei_pieusb_cmd_get_shading_parms()\n");

    /* Ask scanner to prepare the scan frame with the given index. Only SCSI_COMMAND_LEN bytes of data. */
    _prep_scsi_cmd (command, SCSI_WRITE, SCSI_COMMAND_LEN);
    memset (data, '\0', PREP_READ_SIZE);
    data[0] = SCSI_CALIBRATION_INFO | 0x80; /* set bit 7 means prepare read */

    status->pieusb_status = sanei_pieusb_command (device_number, command, data, PREP_READ_SIZE);
    if (status->pieusb_status != PIEUSB_STATUS_GOOD) {
      return;
    }

    /* Read shading parameters */
    _prep_scsi_cmd(command, SCSI_READ, size);

    memset (data, '\0', size);
    status->pieusb_status = sanei_pieusb_command (device_number, command, data, size);
    if (status->pieusb_status != PIEUSB_STATUS_GOOD) {
      return;
    }

    /* Decode data [32 bytes]
       0: 95 00 type
       2: 1c 00 payload len
       4: 04    entries
       5: 06    entry size
       6: 04 00 ?
       8: 00 10 10 14 1a 1d type send recv nlines pixPerLine(2bytes)
      14: 08 10 10 14 1a 1d
      20: 10 10 10 14 1a 1d
      26: 20 10 10 14 1a 1d
     */
    for (k = 0; k < data[4]; k++) {
        shading[k].type = _get_byte (data, 8 + data[5]*k);
        shading[k].sendBits = _get_byte (data, 9 + data[5]*k);
        shading[k].recieveBits = _get_byte (data, 10 + data[5]*k);
        shading[k].nLines = _get_byte (data, 11 + data[5]*k);
        shading[k].pixelsPerLine = _get_short (data, 12 + data[5]*k);
    }
#undef PREP_READ_SIZE
#undef SHADING_SIZE
}

/**
 * Read scanned data from the scanner memory into a byte array. The lines
 * argument specifies how many lines will be read, the size argument specifies
 * the total amount of bytes in these lines. Use sanei_pieusb_cmd_get_parameters() to
 * determine the current line size and the number of available lines.\n
 * If there is scanned data available, it should be read. Waiting too long
 * causes the scan to stop, probably because a buffer is filled to its limits
 * (if so, it is approximately 2Mb in size). I haven't tried what happens if you
 * start reading after a stop. Reading to fast causes the scanner to return
 * a busy status, which is not a problem.
 * This is a SCSI READ command (code 0x08). It is distinguished from the other
 * READ commands by the context in which it is issued: see sanei_pieusb_cmd_start_scan().
 *
 * @param device_number
 * @param data
 * @param lines
 * @param size
 * @return Pieusb_Command_Status
 */
void
sanei_pieusb_cmd_get_scanned_lines(SANE_Int device_number, SANE_Byte* data, SANE_Int lines, SANE_Int size, struct Pieusb_Command_Status *status)
{
    SANE_Byte command[SCSI_COMMAND_LEN];

    DBG (DBG_info_scan, "sanei_pieusb_cmd_get_scanned_lines(): %d lines (%d bytes)\n", lines, size);

    _prep_scsi_cmd (command, SCSI_READ, lines);
    memset (data, '\0', size);

    status->pieusb_status = sanei_pieusb_command (device_number, command, data, size);
}

/**
 * Set the halftone pattern with the given index to the specified pattern. The
 * command is a SCSI WRITE command (code 0x0A, write code 0x11).
 *
 * @param device_number Device number
 * @param index Pattern index (0-7)
 * @param pattern Halftone pattern (not implemented)
 * @return Pieusb_Command_Status
 * @see Pieusb_Halftone_Pattern
 */
void
sanei_pieusb_cmd_set_halftone_pattern(SANE_Int device_number, SANE_Int index, struct Pieusb_Halftone_Pattern* pattern, struct Pieusb_Command_Status *status)
{
    DBG (DBG_info_scan, "sanei_pieusb_cmd_set_halftone_pattern(): not implemented\n");
    status->pieusb_status = PIEUSB_STATUS_INVAL;
}

/**
 * Set the scan frame with the given index to the frame. The command is a SCSI
 * WRITE command (code SCSI_WRITE, write code SCSI_SCAN_FRAME).
 *
 * @param device_number Device number
 * @param index Frame index (0-7)
 * @param frame Scan frame
 * @return Pieusb_Command_Status
 * @see Pieusb_Scan_Frame
 */
void
sanei_pieusb_cmd_set_scan_frame(SANE_Int device_number, SANE_Int index, struct Pieusb_Scan_Frame* frame, struct Pieusb_Command_Status *status)
{
    SANE_Byte command[SCSI_COMMAND_LEN];
#define FRAME_SIZE 14
    SANE_Int size = FRAME_SIZE;
    SANE_Byte data[FRAME_SIZE];

    DBG (DBG_info_scan, "sanei_pieusb_cmd_set_scan_frame()\n");

    _prep_scsi_cmd(command, SCSI_WRITE, size);

    DBG (DBG_info_scan, " x0,y0 = %d,%d\n", frame->x0, frame->y0);
    DBG (DBG_info_scan, " x1,y1 = %d,%d\n", frame->x1, frame->y1);
    DBG (DBG_info_scan, " index = %d\n", index);

    /* Code data */
    memset (data, '\0', size);
    _set_short (SCSI_SCAN_FRAME, data, 0);
    _set_short (size-4, data, 2); /* size: one frame, 5 shorts */
    _set_short (index, data, 4);
    _set_short (frame->x0, data, 6);
    _set_short (frame->y0, data, 8);
    _set_short (frame->x1, data, 10);
    _set_short (frame->y1, data, 12);

    status->pieusb_status = sanei_pieusb_command (device_number, command, data, size);
#undef FRAME_SIZE
}

/**
 * Set the relative exposure time to the given values. Only the first
 * Pieusb_Exposure_Time_Color is used. The command is a SCSI
 * WRITE command (code SCSI_WRITE, write code SCSI_EXPOSURE).
 *
 * @param device_number Device number
 * @param time Relative exposure time
 * @return Pieusb_Command_Status
 * @see Pieusb_Exposure_Time
 */
void
sanei_pieusb_cmd_set_exposure_time(SANE_Int device_number, struct Pieusb_Exposure_Time* time, struct Pieusb_Command_Status *status)
{
    SANE_Byte command[SCSI_COMMAND_LEN];
#define EXPOSURE_DATA_SIZE 8
    SANE_Byte data[EXPOSURE_DATA_SIZE];
    struct Pieusb_Exposure_Time_Color *exptime;
    int i;

    DBG (DBG_info_scan, "sanei_pieusb_cmd_set_exposure_time()\n");

    for (i = 0; i < 3; ++i) { /* R, G, B */
      _prep_scsi_cmd (command, SCSI_WRITE, EXPOSURE_DATA_SIZE);
      memset (data, '\0', EXPOSURE_DATA_SIZE);
      exptime = &(time->color[i]);
      _set_short (SCSI_EXPOSURE, data, 0);
      _set_short (EXPOSURE_DATA_SIZE-4, data, 2); /* short: RGB, short: value */
      _set_short (exptime->filter, data, 4);      /* 1: neutral, 2: R, 4: G, 8: B */
      _set_short (exptime->value, data, 6);
      status->pieusb_status = sanei_pieusb_command (device_number, command, data, EXPOSURE_DATA_SIZE);
      if (status->pieusb_status != PIEUSB_STATUS_GOOD)
	break;
    }

#undef EXPOSURE_DATA_SIZE
}

/**
 * Set the highlight and shadow levels to the given values. Only the first
 * Pieusb_Highlight_Shadow_Color is used. The command is a SCSI
 * WRITE command (code SCSI_WRITE, write code SCSI_HIGHLIGHT_SHADOW).
 *
 * @param device_number Device number
 * @param hgltshdw highlight and shadow level
 * @return Pieusb_Command_Status
 * @see Pieusb_Highlight_Shadow
 */
void
sanei_pieusb_cmd_set_highlight_shadow(SANE_Int device_number, struct Pieusb_Highlight_Shadow* hgltshdw, struct Pieusb_Command_Status *status)
{
    SANE_Byte command[SCSI_COMMAND_LEN];
#define HIGHLIGHT_SHADOW_SIZE 8
    SANE_Byte data[HIGHLIGHT_SHADOW_SIZE];
    struct Pieusb_Highlight_Shadow_Color *color;
    int i;

    DBG (DBG_info_scan, "sanei_pieusb_cmd_set_highlight_shadow()\n");

    for (i = 0; i < 3; ++i) { /* R, G, B */
      _prep_scsi_cmd (command, SCSI_WRITE, HIGHLIGHT_SHADOW_SIZE);
      memset (data, '\0', HIGHLIGHT_SHADOW_SIZE);
      color = &(hgltshdw->color[i]);
      _set_short (SCSI_HIGHLIGHT_SHADOW, data, 0);
      _set_short (HIGHLIGHT_SHADOW_SIZE-4, data, 2); /* short: RGB, short: value */
      _set_short (color->filter, data, 4);      /* 1: neutral, 2: R, 4: G, 8: B */
      _set_short (color->value, data, 6);
      status->pieusb_status = sanei_pieusb_command (device_number, command, data, HIGHLIGHT_SHADOW_SIZE);
      if (status->pieusb_status != PIEUSB_STATUS_GOOD)
	break;
    }

#undef HIGHLIGHT_SHADOW_SIZE
}

/**
 * Set the CCD-mask for the colors set in the given color bit mask. The mask
 * array must contain mask_size. The command is a SCSI WRITE command
 * (code 0x0A, write code 0x16).
 * (The command is able to handle more masks at once, but that is not implemented.)
 *
 * @param device_number Device number
 * @param colorbits 0000RGB0 color bit mask; at least one color bit must be set
 * @param mask CCD mask to use
 * @return Pieusb_Command_Status
 */
void
sanei_pieusb_cmd_set_ccd_mask(SANE_Int device_number, SANE_Byte colorbits, SANE_Byte* mask, SANE_Int mask_size, struct Pieusb_Command_Status *status)
{
    DBG (DBG_info_scan, "sanei_pieusb_cmd_set_ccd_mask(): not implemented\n");
    status->pieusb_status = PIEUSB_STATUS_INVAL;
}

/* SCSI PARAM, code 0x0F */
/**
 * Get the parameters of an executed scan, such as width, lines and bytes, which
 * are needed to calculate the parameters of the READ-commands which read the
 * actual scan data.
 *
 * @param device_number Device number
 * @param parameters Scan parameters
 * @return Pieusb_Command_Status
 * @see Pieusb_Scan_Parameters
 */
void
sanei_pieusb_cmd_get_parameters(SANE_Int device_number, struct Pieusb_Scan_Parameters* parameters, struct Pieusb_Command_Status *status)
{
    SANE_Byte command[SCSI_COMMAND_LEN];
#define PARAMETER_SIZE 18
    SANE_Int size = PARAMETER_SIZE;
    SANE_Byte data[PARAMETER_SIZE];

    DBG (DBG_info_scan, "sanei_pieusb_cmd_get_parameters()\n");

    _prep_scsi_cmd (command, SCSI_PARAM, size);
    memset (data, '\0', size);

    status->pieusb_status = sanei_pieusb_command(device_number, command, data, size);
    if (status->pieusb_status != PIEUSB_STATUS_GOOD) {
        return;
    }

    /* cyberview:
     * 0: e6 02       width 0x2e6 - 742
     * 2: e0 02       lines 0x2e0 - 736
     * 4: e6 02       bytes 0x2e6 - 742
     * 6: 08          filterOffeset1 8
     * 7: 08          filterOffset2  8
     * 8: c9 1c 00 00 period         7369
     * c: 00 00       scsi transfer rate
     * e: d7 00       available lines 215
     * 10:00 00
     */
    /* Decode data recieved */
    parameters->width = _get_short(data, 0);
    parameters->lines = _get_short(data, 2);
    parameters->bytes = _get_short(data, 4);
    parameters->filterOffset1 = _get_byte(data, 6);
    parameters->filterOffset2 = _get_byte(data, 7);
    parameters->period = _get_int(data, 8); /* unused */
    parameters->scsiTransferRate = _get_short(data, 12); /* unused */
    parameters->availableLines = _get_short(data, 14);

    DBG (DBG_info_scan, "sanei_pieusb_cmd_get_parameters() read:\n");
    DBG (DBG_info_scan, " width = %d\n", parameters->width);
    DBG (DBG_info_scan, " lines = %d\n", parameters->lines);
    DBG (DBG_info_scan, " bytes = %d\n", parameters->bytes);
    DBG (DBG_info_scan, " offset1 = %d\n", parameters->filterOffset1);
    DBG (DBG_info_scan, " offset2 = %d\n", parameters->filterOffset2);
    DBG (DBG_info_scan, " available lines = %d\n", parameters->availableLines);
#undef PARAMETER_SIZE
}

/**
 * Read INQUIRY block from device (SCSI command code 0x12). This block contains
 * information about the properties of the scanner.
 * Returns status->pieusb_status:
 * - PIEUSB_STATUS_GOOD if the INQUIRY command succeeded
 * - PIEUSB_STATUS_DEVICE_BUSY if device is busy after repeat retries
 * - other SANE status code if INQUIRY failed or if it returned CHECK CONDITION
 *   and REQUEST SENSE failed
 *
 * @param device_number Device number
 * @param data Input or output data buffer
 * @param size Size of the data buffer
 * @return Pieusb_Command_Status
 * @see Pieusb_Scanner_Properties
 */
void
sanei_pieusb_cmd_inquiry(SANE_Int device_number, struct Pieusb_Scanner_Properties* inq, SANE_Byte size, struct Pieusb_Command_Status *status)
{
    SANE_Byte command[SCSI_COMMAND_LEN];
#define INQUIRY_SIZE 256
    SANE_Byte data[INQUIRY_SIZE];
    int k;

    DBG (DBG_info_scan, "sanei_pieusb_cmd_inquiry()\n");

    _prep_scsi_cmd (command, SCSI_INQUIRY, size);
    memset (data, '\0', INQUIRY_SIZE); /* size may be less than INQUIRY_SIZE, so prevent returning noise */

    status->pieusb_status = sanei_pieusb_command (device_number, command, data, size);
    if (status->pieusb_status != PIEUSB_STATUS_GOOD) {
        return;
    }

    /* Decode data recieved */
    inq->deviceType = _get_byte(data, 0);
    inq->additionalLength = _get_byte(data, 4);
    _copy_bytes((SANE_Byte*)(inq->vendor), data+8, 8); /* Note: not 0-terminated */
    _copy_bytes((SANE_Byte*)(inq->product), data+16, 16); /* Note: not 0-terminated */
    _copy_bytes((SANE_Byte*)(inq->productRevision), data+32, 4); /* Note: not 0-terminated */
    /* 1st Vendor-specific block, 20 bytes, see pie_get_inquiry_values(), partially: */
    inq->maxResolutionX = _get_short(data, 36);
    inq->maxResolutionY = _get_short(data, 38);
    inq->maxScanWidth = _get_short(data, 40);
    inq->maxScanHeight = _get_short(data, 42);
    inq->filters = _get_byte(data, 44);
    inq->colorDepths = _get_byte(data, 45);
    inq->colorFormat = _get_byte(data, 46);
    inq->imageFormat = _get_byte(data, 48);
    inq->scanCapability = _get_byte(data, 49);
    inq->optionalDevices = _get_byte(data, 50);
    inq->enhancements = _get_byte(data, 51);
    inq->gammaBits = _get_byte(data, 52);
    inq->lastFilter = _get_byte(data, 53);
    inq->previewScanResolution = _get_short(data, 54);
    /* 2nd vendor specific block (36 bytes at offset 96) */
    _copy_bytes((SANE_Byte*)(inq->firmwareVersion), data+96, 4); inq->firmwareVersion[4]=0x00;
    inq->halftones = _get_byte(data, 100);
    inq->minumumHighlight = _get_byte(data, 101);
    inq->maximumShadow = _get_byte(data, 102);
    inq->calibrationEquation = _get_byte(data, 103);
    inq->maximumExposure = _get_short(data ,104);
    inq->minimumExposure = _get_short(data ,106);
    inq->x0 = _get_short(data, 108);
    inq->y0 = _get_short(data, 110);
    inq->x1 = _get_short(data, 112);
    inq->y1 = _get_short(data, 114);
    inq->model = _get_short(data, 116);
    _copy_bytes((SANE_Byte*)(inq->production), data+120, 4);
    _copy_bytes((SANE_Byte*)(inq->timestamp), data+124, 20);
    _copy_bytes((SANE_Byte*)(inq->signature), data+144, 40);
    /* remove newline in signature */
    for (k=0; k<40; k++) if (inq->signature[k]==0x0a || inq->signature[k]==0x0d) inq->signature[k]=' ';
#undef INQUIRY_SIZE
}

/**
 * Set scan mode parameters, such as resolution, colors to scan, color depth,
 * color format, and a couple of scan quality settings (sharpen, skip
 * calibration, fast infrared). It performs the SCSI-command MODE SELECT,
 * code 0x15.
 *
 * @param device_number Device number
 * @param mode Mode parameters
 * @return Pieusb_Command_Status
 * @see Pieusb_Mode
 */
void
sanei_pieusb_cmd_set_mode(SANE_Int device_number, struct Pieusb_Mode* mode, struct Pieusb_Command_Status *status)
{
    SANE_Byte command[SCSI_COMMAND_LEN];
#define MODE_SIZE 16
    SANE_Int size = MODE_SIZE;
    SANE_Byte data[MODE_SIZE];
    SANE_Byte quality;

    DBG (DBG_info_scan, "sanei_pieusb_cmd_set_mode()\n");

    _prep_scsi_cmd(command, SCSI_MODE_SELECT, size);

    DBG (DBG_info_scan, "sanei_pieusb_cmd_set_mode() set:\n");
    DBG (DBG_info_scan, " resolution = %d\n", mode->resolution);
    DBG (DBG_info_scan, " passes = %02x\n", mode->passes);
    DBG (DBG_info_scan, " depth = %02x\n", mode->colorDepth);
    DBG (DBG_info_scan, " color format = %02x\n", mode->colorFormat);
    DBG (DBG_info_scan, " sharpen = %d\n", mode->sharpen);
    DBG (DBG_info_scan, " skip calibration = %d\n", mode->skipShadingAnalysis);
    DBG (DBG_info_scan, " fast infrared = %d\n", mode->fastInfrared);
    DBG (DBG_info_scan, " halftone pattern = %d\n", mode->halftonePattern);
    DBG (DBG_info_scan, " line threshold = %d\n", mode->lineThreshold);

    /* Code data */
    /* cyberview
     * 00 0f entries
     * f4 01 resolution 500
     * 80    RGB (90: RGBI)
     * 04    color depth (4: 8 bit, 20: 16 bit)
     * 04    color format
     * 00
     * 01    byte order
     * 08    quality bitmask: 80=fast infrared, 08=skip shading analysis, 02=sharpen
     * 00 00
     * 00    halftone pattern
     * 80    line threshold
     * 10 00
     *
     * pieusb
     * 0: 00 0f
     * 2: e8 03 resolution 1000
     * 4: 80    passes
     * 5: 04    color depth
     * 6: 04    color format
     * 7: 00
     * 8: 01    byte order
     * 9: 02    quality bitmask: sharpen
     * a: 00 00
     * c: 00    halftone pattern
     * d: 7f    line threshold
     * e: 00 00
     */
    memset (data, '\0', size);
    _set_byte (size-1, data, 1);
    _set_short (mode->resolution, data, 2);
    _set_byte (mode->passes, data, 4);
    _set_byte (mode->colorDepth, data, 5);
    _set_byte (mode->colorFormat, data, 6);
    _set_byte (mode->byteOrder, data, 8);
    quality = 0x00;
    if (mode->sharpen) quality |= 0x02;
    if (mode->skipShadingAnalysis) quality |= 0x08;
    if (mode->fastInfrared) quality |= 0x80;
    _set_byte (quality, data, 9);
    _set_byte (mode->halftonePattern, data, 12);
    _set_byte (mode->lineThreshold, data, 13);
    _set_byte (0x10, data, 14); /* ? */

    status->pieusb_status = sanei_pieusb_command (device_number, command, data, size);
#undef MODE_SIZE
}

/* SCSI COPY, code 0x18 */
/**
 * Get the currently used CCD-mask, which defines which pixels have been used in
 * the scan, and which allows to relate scan data to shading data. A mask is a
 * 5340 byte array which consists only contains the values 0x00 and 0x70. A
 * value of 0x00 indicates the pixel is used, a value of 0x70 that it is not.\n
 * The number of 0x00 bytes equals the number of pixels on a line.\n
 * The mask begins with a number of 0x70 bytes equal to the scan frame x0-value
 * divided by 2.\n
 * The SCSI-command COPY (code 0x18) is used for function.
 *
 * @param device_number Device number
 * @param mask
 * @return Pieusb_Command_Status
 */
void
sanei_pieusb_cmd_get_ccd_mask(SANE_Int device_number, SANE_Byte* mask, SANE_Int mask_size, struct Pieusb_Command_Status *status)
{
    SANE_Byte command[SCSI_COMMAND_LEN];

    DBG (DBG_info_scan, "sanei_pieusb_cmd_get_ccd_mask()\n");

    _prep_scsi_cmd (command, SCSI_COPY, mask_size);

    memset (mask, '\0', mask_size);
    status->pieusb_status = sanei_pieusb_command (device_number, command, mask, mask_size);
}

/**
 * Get scan mode parameters, such as resolution, colors to scan, color depth,
 * color format, and a couple of scan quality settings (sharpen, skip
 * calibration, fast infrared). It performs the SCSI-command MODE SELECT,
 * code 0x1A.
 *
 * @param device_number Device number
 * @param mode Mode parameters
 * @return Pieusb_Command_Status
 * @see Pieusb_Mode
 */
void
sanei_pieusb_cmd_get_mode(SANE_Int device_number, struct Pieusb_Mode* mode, struct Pieusb_Command_Status *status)
{
    SANE_Byte command[SCSI_COMMAND_LEN];
#define MODE_SIZE 16
    SANE_Int size = MODE_SIZE;
    SANE_Byte data[MODE_SIZE];
    SANE_Byte quality;

    DBG (DBG_info_scan, "sanei_pieusb_cmd_get_mode()\n");

    _prep_scsi_cmd (command, SCSI_MODE_SENSE, size);
    memset (data, '\0', size);

    status->pieusb_status = sanei_pieusb_command(device_number, command, data, size);
    if (status->pieusb_status != PIEUSB_STATUS_GOOD) {
        return;
    }

    /* Decode data recieved */
    mode->resolution = _get_short (data, 2);
    mode->passes = _get_byte (data, 4);
    mode->colorDepth = _get_byte (data, 5);
    mode->colorFormat = _get_byte (data, 6);
    mode->byteOrder = _get_byte (data, 8);
    quality = _get_byte (data, 9);
    mode->sharpen = (quality |= 0x02) ? SANE_TRUE : SANE_FALSE;
    mode->skipShadingAnalysis = (quality |= 0x08) ? SANE_TRUE : SANE_FALSE;
    mode->fastInfrared = (quality |= 0x80) ? SANE_TRUE : SANE_FALSE;
    mode->halftonePattern = _get_byte (data, 12);
    mode->lineThreshold = _get_byte (data, 13);

    DBG (DBG_info_scan, "cmdGetMode():\n");
    DBG (DBG_info_scan, " resolution = %d\n", mode->resolution);
    DBG (DBG_info_scan, " passes = %02x\n", mode->passes);
    DBG (DBG_info_scan, " depth = %02x\n", mode->colorDepth);
    DBG (DBG_info_scan, " color format = %02x\n", mode->colorFormat);
    DBG (DBG_info_scan, " sharpen = %d\n", mode->sharpen);
    DBG (DBG_info_scan, " skip calibration = %d\n", mode->skipShadingAnalysis);
    DBG (DBG_info_scan, " fast infrared = %d\n", mode->fastInfrared);
    DBG (DBG_info_scan, " halftone pattern = %d\n", mode->halftonePattern);
    DBG (DBG_info_scan, " line threshold = %d\n", mode->lineThreshold);
#undef MODE_SIZE
}

/**
 * Start a scan (SCSI SCAN command, code 0x1B, size byte = 0x01).\n
 * There are four phases in a scan process. During each phase a limited number of
 * commands is available. The phases are:\n
 * 1. Calibration phase: make previously collected shading correction data available\n
 * 2. Line-by-line scan & read phase\n
 * 3. Output CCD-mask phase\n
 * 4. Scan and output scan data phase\n
 *
 * The calibration phase is skipped if Pieusb_Mode.skipCalibration is set. If
 * the scanner determines a calibration is necessary, a CHECK CONDIDITION response
 * is returned. Available command during this phase:\n
 * 1. sanei_pieusb_cmd_test_unit_ready()\n
 * 2. sanei_pieusb_cmd_get_scanned_lines(): read shading correction lines\n
 * 3. sanei_pieusb_cmd_stop_scan: abort scanning process\n
 * 4. sanei_pieusb_cmd_get_gain_offset() : the settings are generated during the initialisation of this phase, so they are current\n
 * 5. cmdSetSettings(): settings take effect in the next scan phase\n\n
 * The line-by-line phase is only entered if Pieusb_Mode.div_10[0] bit 5 is
 * set. It is not implemented.\n\n
 * In the CCD-mask output phase the CCD-mask is read. Available command during this phase:\n
 * 1. sanei_pieusb_cmd_test_unit_ready()\n
 * 2. sanei_pieusb_cmd_get_ccd_mask()\n
 * 3. sanei_pieusb_cmd_stop_scan: abort scanning process\n\n
 * In the 'scan and output scan data' phase, the slide is scanned while data is
 * read in the mean time. Available command during this phase:\n
 * 1. sanei_pieusb_cmd_test_unit_ready()\n
 * 2. sanei_pieusb_cmd_get_scanned_lines()\n
 * 2. sanei_pieusb_cmd_get_parameters()\n
 * 4. sanei_pieusb_cmd_stop_scan: abort scanning process\n
 *
 * @param device_number Device number
 * @return Pieusb_Command_Status
 */
void
sanei_pieusb_cmd_start_scan(SANE_Int device_number, struct Pieusb_Command_Status *status)
{
    SANE_Byte command[SCSI_COMMAND_LEN];

    DBG (DBG_info_scan, "sanei_pieusb_cmd_start_scan()\n");

    _prep_scsi_cmd (command, SCSI_SCAN, 1);

    status->pieusb_status = sanei_pieusb_command (device_number, command, NULL, 0);
}

/**
 * Stop a scan started with sanei_pieusb_cmd_start_scan(). It issues a SCSI SCAN command,
 * code 0x1B, with size byte = 0x00.
 *
 * @param device_number Device number
 * @return Pieusb_Command_Status
 */
void
sanei_pieusb_cmd_stop_scan(SANE_Int device_number, struct Pieusb_Command_Status *status)
{
    SANE_Byte command[SCSI_COMMAND_LEN];

    DBG (DBG_info_scan, "sanei_pieusb_cmd_stop_scan()\n");

    _prep_scsi_cmd (command, SCSI_SCAN, 0);

    status->pieusb_status = sanei_pieusb_command (device_number, command, NULL, 0);
}

/**
 * Set scan head to a specific position, depending on the value for mode:\n
 * mode = 1: Returns the scan head to the resting position, after a short move
 * forward. If this command is left out between two scans, the second scan is
 * up-down-mirrored, and scanning starts where the proevious scan stopped.\n
 * mode = 2: Resets the scan head an then moves it forward depending on 'size',
 * but it is a bit unpredictable to what position. The scanner may attempt to
 * move the head past its physical end position. The mode is not implemented.\n
 * mode = 3: This command positions the scan head to the start of the slide.\n
 * mode = 4 or 5: The command forwards (4) or retreats (5) the scan head the
 * given amount of steps (in size).\n
 * The SCSI code is 0xD2, there is no related command name.
 *
 * @param device_number Device number
 * @param mode
 * @param size
 * @return Pieusb_Command_Status
 */
void
sanei_pieusb_cmd_set_scan_head(SANE_Int device_number, SANE_Int mode, SANE_Int steps, struct Pieusb_Command_Status *status)
{
    SANE_Byte command[SCSI_COMMAND_LEN];
#define SCAN_HEAD_SIZE 4
    SANE_Int size = SCAN_HEAD_SIZE;
    SANE_Byte data[SCAN_HEAD_SIZE];

    DBG (DBG_info_scan, "sanei_pieusb_cmd_set_scan_head()\n");

    _prep_scsi_cmd (command, SCSI_SET_SCAN_HEAD, size);

    /* Code data */
    memset (data, '\0', size);
    switch (mode) {
        case 1:
            data[0] = 2;
            break;
        case 2:
            DBG (DBG_error, "sanei_pieusb_cmd_set_scan_head() mode 2 unreliable, possibly dangerous\n");
            status->pieusb_status = PIEUSB_STATUS_INVAL;
            return;
        case 3:
            data[0] = 8;
            break;
        case 4:
            data[0] = 0; /* forward */
            data[2] = (steps>>8) & 0xFF;
            data[3] = steps & 0xFF;
            break;
        case 5:
            data[0] = 1; /* backward */
            data[2] = (steps>>8) & 0xFF;
            data[3] = steps & 0xFF;
            break;
    }

    status->pieusb_status = sanei_pieusb_command(device_number, command, data, size);
#undef SCAN_HEAD_SIZE
}

/**
 * Get internal scanner settings which have resulted from an auto-calibration
 * procedure. This procedure only runs when calibrating (Scan phase 1), so the
 * data returned are relatively static.\n
 * The SCSI code is 0xD7, there is no related command name.
 *
 * @param device_number Device number
 * @param settings Settings for gain and offset for the four colors RGBI
 * @return Pieusb_Command_Status
 * @see Pieusb_Settings
 */
void
sanei_pieusb_cmd_get_gain_offset(SANE_Int device_number, struct Pieusb_Settings* settings, struct Pieusb_Command_Status *status)
{
    SANE_Byte command[SCSI_COMMAND_LEN];
#define GAIN_OFFSET_SIZE 103
    SANE_Int size = GAIN_OFFSET_SIZE;
    SANE_Byte data[GAIN_OFFSET_SIZE];
    int k;
    SANE_Byte val[3];

    DBG (DBG_info_scan, "sanei_pieusb_cmd_get_gain_offset()\n");

    _prep_scsi_cmd (command, SCSI_READ_GAIN_OFFSET, size);

    memset (data, '\0', size);
    status->pieusb_status = sanei_pieusb_command(device_number, command, data, size);
    if (status->pieusb_status != PIEUSB_STATUS_GOOD) {
        return;
    }

    /* Decode data received */
    _get_shorts (settings->saturationLevel, data+54, 3);
    _get_shorts (settings->exposureTime, data+60, 3);
    _copy_bytes (val, data+66, 3);
    for (k = 0; k < 3; k++) settings->offset[k] = val[k];
    _copy_bytes (val, data+72, 3);
    for (k = 0; k < 3; k++) settings->gain[k] = val[k];
    settings->light = _get_byte (data, 75);
    settings->exposureTime[3] = _get_short (data, 98);
    settings->offset[3] = _get_byte (data, 100);
    settings->gain[3] = _get_byte (data, 102);

    DBG (DBG_info, "sanei_pieusb_cmd_get_gain_offset() set:\n");
    DBG (DBG_info, " saturationlevels = %d-%d-%d\n", settings->saturationLevel[0], settings->saturationLevel[1], settings->saturationLevel[2]);
    DBG (DBG_info, " ---\n");
    DBG (DBG_info, " exposure times = %d-%d-%d-%d\n", settings->exposureTime[0], settings->exposureTime[1], settings->exposureTime[2], settings->exposureTime[3]);
    DBG (DBG_info, " gain = %d-%d-%d-%d\n", settings->gain[0], settings->gain[1], settings->gain[2], settings->gain[3]);
    DBG (DBG_info, " offset = %d-%d-%d-%d\n", settings->offset[0], settings->offset[1], settings->offset[2], settings->offset[3]);
    DBG (DBG_info, " light = %02x\n", settings->light);
    DBG (DBG_info, " double times = %02x\n", settings->doubleTimes);
    DBG (DBG_info, " extra entries = %02x\n", settings->extraEntries);
#undef GAIN_OFFSET_SIZE
}


/**
 * Set internal scanner settings such as gain and offset.\n
 * There are two effective moments for this command:\n
 * 1. For a scan without calibration phase: before the sanei_pieusb_cmd_start_scan() command;
 * 2. For a sccan with calibration phase: before (or during) reading the shading reference data.
 * The SCSI code is 0xDC, there is no related command name.
 *
 * @param device_number Device number
 * @param settings Settings for gain and offset for the four colors RGBI
 * @return Pieusb_Command_Status
 * @see Pieusb_Settings
 */
void
sanei_pieusb_cmd_set_gain_offset(SANE_Int device_number, struct Pieusb_Settings* settings, struct Pieusb_Command_Status *status)
{
    SANE_Byte command[SCSI_COMMAND_LEN];
#define GAIN_OFFSET_SIZE 29
    SANE_Int size = GAIN_OFFSET_SIZE;
    SANE_Byte data[GAIN_OFFSET_SIZE];
    int k;
    SANE_Byte val[3];

    DBG (DBG_info_scan, "sanei_pieusb_cmd_set_gain_offset()\n");

    _prep_scsi_cmd (command, SCSI_WRITE_GAIN_OFFSET, size);

    DBG (DBG_info, "sanei_pieusb_cmd_set_gain_offset() set:\n");
    DBG (DBG_info, " exposure times = %d-%d-%d-%d\n", settings->exposureTime[0], settings->exposureTime[1], settings->exposureTime[2], settings->exposureTime[3]);
    DBG (DBG_info, " gain = %d-%d-%d-%d\n", settings->gain[0], settings->gain[1], settings->gain[2], settings->gain[3]);
    DBG (DBG_info, " offset = %d-%d-%d-%d\n", settings->offset[0], settings->offset[1], settings->offset[2], settings->offset[3]);
    DBG (DBG_info, " light = %02x\n", settings->light);
    DBG (DBG_info, " double times = %02x\n", settings->doubleTimes);
    DBG (DBG_info, " extra entries = %02x\n", settings->extraEntries);

    /* Code data */
    memset (data, '\0', size);
    _set_shorts (settings->exposureTime, data, 3);
    for (k = 0; k < 3; k++) {
      val[k] = settings->offset[k];
    }
    _copy_bytes (data + 6, val, 3);
    for (k = 0; k < 3; k++) {
      val[k] = settings->gain[k];
    }
    _copy_bytes (data + 12, val, 3);
    _set_byte (settings->light, data, 15);
    _set_byte (settings->extraEntries, data, 16);
    _set_byte (settings->doubleTimes, data, 17);
    _set_short (settings->exposureTime[3], data, 18);
    _set_byte (settings->offset[3], data, 20);
    _set_byte (settings->gain[3], data, 22);
    /*
     * pieusb-get_gain_offset:
     *  00000000: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
     *  00000010: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
     *  00000020: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................
     *  00000030: a9 00 88 00 b1 00 00 00 00 00 00 00 04 10 04 10 )...1...........
     *  00000040: 04 10 53 4f 6e 00 00 00 2e 21 21 05 04 10 df 2d ..SOn....!!..._-
     *  00000050: a3 5c e7 f2 a1 2c b3 c4 42 df 32 42 eb 82 8e e0 #\gr!,3DB_2Bk..`
     *  00000060: 87 be 04 10 4f 00 2c                            .>..O.,
     *
     * cyberview:
     * 00000000: 65 22  57 18  19 19 - exposure time RGB
     * 00000006: 51 4e 6a - offset RGB
     * 00000009: 00 00 00
     * 0000000c: 21 21 21 - gain RGB
     * 0000000f: 05 - light
     * 00000010: 01 - extra entries
     * 00000011: 00 - double times
     * 00000012: 04 10 - exposure time I
     * 00000014: 4e 00 - offset I
     * 00000016: 2a - gain I
     * 00000017: 00 00 00 00 00 00
     *
     * pieusb:
     * 00000000: 04 10  04 10  04 10 - exposure time RGB
     * 00000006: 53 4f 6e - offset RGB
     * 00000009: 00 00 00
     * 0000000c: 2e 21 21 - gain RGB
     * 0000000f: 05 - light
     * 00000010: 00 - extra entries
     * 00000011: 00 - double times
     * 00000012: 04 10 - exposure time I
     * 00000014: 4f 00 - offset I
     * 00000016: 2c    - gain I
     * 00000017: 00 00 00 00 00 00
     */

    status->pieusb_status = sanei_pieusb_command(device_number, command, data, size);
#undef GAIN_OFFSET_SIZE
}

/**
 * Get scanner state information: button pushed,
 * warming up, scanning.
 *
 * @param device_number Device number
 * @param state State information
 * @return Pieusb_Command_Status
 */
void
sanei_pieusb_cmd_read_state(SANE_Int device_number, struct Pieusb_Scanner_State* state, struct Pieusb_Command_Status *status)
{
    SANE_Byte command[SCSI_COMMAND_LEN];
#define GET_STATE_SIZE 12
    SANE_Byte data[GET_STATE_SIZE];
    SANE_Int size = GET_STATE_SIZE;

    /* Execute READ STATUS command */
    DBG (DBG_info_scan, "sanei_pieusb_cmd_read_state()\n");

    _prep_scsi_cmd (command, SCSI_READ_STATE, size);

    memset (data, '\0', size);
    status->pieusb_status = sanei_pieusb_command (device_number, command, data, size);

    if (status->pieusb_status == PIEUSB_STATUS_WARMING_UP
        || status->pieusb_status == PIEUSB_STATUS_DEVICE_BUSY) {
      data[5] = 1;
      status->pieusb_status = PIEUSB_STATUS_GOOD;
    }
    /* Decode data recieved */
    state->buttonPushed = _get_byte(data, 0);
    state->warmingUp = _get_byte(data, 5);
    state->scanning = _get_byte(data, 6);
/*    state->busy = _get_byte(data, 8); */
    DBG (DBG_info_scan, "sanei_pieusb_cmd_read_state(): button %d, warmingUp %d, scanning %d, busy? %d\n", state->buttonPushed, state->warmingUp, state->scanning, _get_byte(data, 8));
#undef GET_STATE_SIZE
}

/**
 * Prepare SCSI_COMMAND_LEN-byte command array with command code and size value
 *
 * @param command
 * @param code
 * @param size
 */
static void
_prep_scsi_cmd(SANE_Byte* command, SANE_Byte code, SANE_Word size)
{
    memset(command, '\0', SCSI_COMMAND_LEN);
    command[0] = code;
    command[3] = (size>>8) & 0xFF; /* lsb first */
    command[4] = size & 0xFF;
}
