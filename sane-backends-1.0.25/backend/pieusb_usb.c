/* sane - Scanner Access Now Easy.

   pieusb_usb.c

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

#define DEBUG_DECLARE_ONLY
#include "pieusb.h"
#include "pieusb_scancmd.h"
#include "pieusb_usb.h"

#include "../include/sane/sanei_usb.h"
#include <unistd.h> /* usleep */
#include <time.h> /* time */

/* USB functions */

static SANE_Status _ctrl_out_byte(SANE_Int device_number, SANE_Int port, SANE_Byte b);
static SANE_Status _bulk_size(SANE_Int device_number, unsigned int size);
static SANE_Status _ctrl_in_byte(SANE_Int device_number, SANE_Byte* b);
static SANE_Status _bulk_in(SANE_Int device_number, SANE_Byte* data, size_t *size);
static SANE_Status _ieee_command(SANE_Int device_number, SANE_Byte command);

/* Defines for use in USB functions */

#define REQUEST_TYPE_IN	(USB_TYPE_VENDOR | USB_DIR_IN)
#define REQUEST_TYPE_OUT (USB_TYPE_VENDOR | USB_DIR_OUT)
#define REQUEST_REGISTER 0x0c
#define REQUEST_BUFFER 0x04
#define ANYINDEX 0x00 /* wIndex value for USB control transfer - value is irrelevant */

/* from libieee1284 */
#define C1284_NSTROBE 0x01
#define C1284_NINIT   0x04

/* usb via ieee1284 */
#define IEEE1284_ADDR  0x00
#define IEEE1284_RESET 0x30
#define IEEE1284_SCSI  0xe0

#define PORT_SCSI_SIZE   0x0082
#define PORT_SCSI_STATUS 0x0084
#define PORT_SCSI_CMD    0x0085
#define PORT_PAR_CTRL    0x0087 /* IEEE1284 parallel control */
#define PORT_PAR_DATA    0x0088 /* IEEE1284 parallel data */

/* see also: SCSI Status Codes http://www.t10.org/lists/2status.htm */
typedef enum {
  USB_STATUS_OK    = 0x00, /* ok */
  USB_STATUS_READ  = 0x01, /* read: send expected length, then read data */
  USB_STATUS_CHECK = 0x02, /* check condition */
  USB_STATUS_BUSY  = 0x03, /* wait on usb */
  USB_STATUS_AGAIN = 0x08, /* re-send scsi cmd */
  USB_STATUS_FAIL  = 0x88, /* ??? */
  USB_STATUS_ERROR = 0xff  /* usb i/o error */
} PIEUSB_USB_Status;

static PIEUSB_USB_Status _pieusb_scsi_command(SANE_Int device_number, SANE_Byte command[], SANE_Byte data[], SANE_Int size);

#define SENSE_CODE_WARMING_UP 4

/* Standard SCSI Sense codes*/
#define SCSI_NO_ADDITIONAL_SENSE_INFORMATION 0x00

struct code_text_t { int code; char *text; };
static struct code_text_t usb_code_text[] = {
  { 0x00, "Ok" },
  { 0x01, "Read" },
  { 0x02, "Check" },
  { 0x03, "Busy" },
  { 0x08, "Again" },
  { 0xff, "Error" },
  { -1, NULL }
};

static struct code_text_t scsi_code_text[] = {
  { 0x00, "Test Unit Ready" }
  ,{ 0x01, "Calibrate" }
  ,{ 0x03, "Request Sense" }
  ,{ 0x04, "Format" }
  ,{ 0x08, "Read" }
  ,{ 0x0a, "Write" }
  ,{ 0x0f, "Get Param" }
  ,{ 0x10, "Mark" }
  ,{ 0x11, "Space" }
  ,{ 0x12, "Inquiry" }
  ,{ 0x15, "Mode Select" }
  ,{ 0x16, "Reserve Unit" }
  ,{ 0x18, "Copy" }
  ,{ 0x1a, "Mode Sense" }
  ,{ 0x1b, "Scan" }
  ,{ 0x1d, "Diagnose" }
  ,{ 0xa8, "Read Extended" }
  ,{ 0xd1, "Slide" }
  ,{ 0xd2, "Set Scan Head" }
  ,{ 0xd7, "Read Gain Offset" }
  ,{ 0xdc, "Write Gain Offset" }
  ,{ 0xdd, "Read State" }
  ,{ -1, NULL }
};

static char *
code_to_text(struct code_text_t *list, int code)
{
  while (list && list->text) {
    if (list->code == code)
      return list->text;
    list++;
  }
  return "**unknown**";
}

/**
 * Convert PIEUSB_Status to SANE_Status
 */
SANE_Status
sanei_pieusb_convert_status(PIEUSB_Status status)
{
  return (SANE_Status)status;
}


/**
 * hex dump 'size' bytes starting at 'ptr'
 */
static void
_hexdump(char *msg, unsigned char *ptr, int size)
{
  unsigned char *lptr = ptr;
  int count = 0;
  long start = 0;
  long clipped = 0;

  if (DBG_info_proc > DBG_LEVEL)
    return;

  if (size > 127) {
    clipped = size;
    size = 128;
  }
  while (size-- > 0)
  {
    if ((count % 16) == 0)
      fprintf (stderr, "%s\t%08lx:", msg?msg:"", start);
      msg = NULL;
	fprintf (stderr, " %02x", *ptr++);
	count++;
	start++;
	if (size == 0)
	{
	    while ((count % 16) != 0)
	    {
		fprintf (stderr, "   ");
		count++;
	    }
	}
	if ((count % 16) == 0)
	{
	    fprintf (stderr, " ");
	    while (lptr < ptr)
	    {
	        unsigned char c = *lptr & 0x7f;
		fprintf (stderr, "%c", ((c < 0x20)||(c == 0x7f)) ? '.' : c);
		lptr++;
	    }
	    fprintf (stderr, "\n");
	}
    }
    if ((count % 16) != 0)
	fprintf (stderr, "\n");
    if (clipped > 0)
      fprintf (stderr, "\t%08lx bytes clipped\n", clipped);

    fflush(stderr);
    return;
}


/* =========================================================================
 *
 * USB functions
 *
 * ========================================================================= */

/**
 * Send a command to the device, retry 10 times if device is busy
 * and return SENSE data in the sense fields of status if there is a CHECK
 * CONDITION response from the command.
 * If the REQUEST SENSE command fails, the SANE status code is unequal to
 * PIEUSB_STATUS_GOOD and the sense fields are empty.
 *
 * @param device_number Device number
 * @param command Command array
 * @param data Input or output data buffer
 * @param size Size of the data buffer
 * @param status Pieusb_Command_Status
 */

PIEUSB_Status
sanei_pieusb_command(SANE_Int device_number, SANE_Byte command[], SANE_Byte data[], SANE_Int size)
{
#define MAXTIME 60 /* max 60 seconds */
  time_t start;
  SANE_Status sane_status;
  PIEUSB_Status ret = PIEUSB_STATUS_DEVICE_BUSY;
  SANE_Byte usbstat;
  PIEUSB_USB_Status usb_status = USB_STATUS_AGAIN;

  DBG (DBG_info_usb, "*** sanei_pieusb_command(%02x:%s): size 0x%02x\n", command[0], code_to_text (scsi_code_text, command[0]), size);

  start = time(NULL);
  while ((time(NULL)-start) < MAXTIME) {
    DBG (DBG_info_usb, "\tsanei_pieusb_command loop, status %d:%s\n", usb_status, code_to_text (usb_code_text, usb_status));
    if (usb_status == USB_STATUS_AGAIN) {
      usb_status = _pieusb_scsi_command (device_number, command, data, size);
      DBG (DBG_info_usb, "\t_pieusb_scsi_command returned %d:%s\n", usb_status, code_to_text (usb_code_text, usb_status));
      continue;
    }
    if (usb_status == USB_STATUS_OK) {
      ret = PIEUSB_STATUS_GOOD;
      break;
    }
    if (usb_status == USB_STATUS_READ) {
      DBG (DBG_error, "\tsanei_pieusb_command() 2nd STATUS_READ ?!\n");
      ret = PIEUSB_STATUS_IO_ERROR;
      break;
    }
    if (usb_status == USB_STATUS_CHECK) {
      /* check condition */
      struct Pieusb_Sense sense;
      struct Pieusb_Command_Status senseStatus;

#define SCSI_REQUEST_SENSE      0x03

      if (command[0] == SCSI_REQUEST_SENSE) {
        DBG (DBG_error, "\tsanei_pieusb_command() recursive SCSI_REQUEST_SENSE\n");
        ret = PIEUSB_STATUS_INVAL;
        break;
      }

      /* A check sense may be a busy state in disguise
       * It is also practical to execute a request sense command by
       * default. The calling function should interpret
       * PIEUSB_STATUS_CHECK_SENSE as 'sense data available'. */

      sanei_pieusb_cmd_get_sense (device_number, &sense, &senseStatus, &ret);
      if (senseStatus.pieusb_status != PIEUSB_STATUS_GOOD) {
        DBG (DBG_error, "\tsanei_pieusb_command(): CHECK CONDITION, but REQUEST SENSE fails\n");
        ret = senseStatus.pieusb_status;
      }
      break;
    }
    if (usb_status == USB_STATUS_BUSY) {
      /* wait on usb */
      sane_status = _ctrl_in_byte (device_number, &usbstat);
      if (sane_status != SANE_STATUS_GOOD) {
        DBG (DBG_error, "\tpieusb_scsi_command() fails status in: %d\n", sane_status);
        ret = PIEUSB_STATUS_IO_ERROR;
        break;
      }
      usb_status = usbstat;
      if (usb_status == USB_STATUS_AGAIN) {
        sleep(1);
      }
      continue;
    }
    if (usb_status == USB_STATUS_AGAIN) {
      /* re-send scsi cmd */
      continue;
    }
    if (usb_status == USB_STATUS_FAIL) {
      DBG (DBG_error, "\tsanei_pieusb_command() usb status again2\n");
      usb_status = USB_STATUS_ERROR;
      sanei_pieusb_usb_reset(device_number);
      ret = PIEUSB_STATUS_IO_ERROR;
      break;
    }
    if (usb_status == USB_STATUS_ERROR) {
      sanei_pieusb_usb_reset(device_number);
      ret = PIEUSB_STATUS_IO_ERROR;
      break;
    }

    DBG (DBG_error, "\tsanei_pieusb_command() unhandled usb status 0x%02x\n", usb_status);
    ret = PIEUSB_STATUS_IO_ERROR;
    break;
  }
  if ((time(NULL)-start) > MAXTIME) {
    DBG (DBG_info_usb, "\tsanei_pieusb_command() timeout !\n");
  }

  DBG (DBG_info_usb, "\tsanei_pieusb_command() finished with state %d\n", ret);
  return ret;
}

/**
 * Reset IEEE1284 interface
 *
 * @param device_number Device number
 * @returns SANE_Status
 */

SANE_Status
sanei_pieusb_usb_reset(SANE_Int device_number)
{
  DBG (DBG_info_sane, "\tsanei_pieusb_usb_reset()\n");
  return _ieee_command (device_number, IEEE1284_RESET);
}

/* http://www.t10.org/lists/2sensekey.htm */
static struct code_text_t sense_code_text[] = {
  { SCSI_SENSE_NO_SENSE, "No Sense" },
  { SCSI_SENSE_RECOVERED_ERROR, "Recovered Error" },
  { SCSI_SENSE_NOT_READY, "Not Ready" },
  { SCSI_SENSE_MEDIUM_ERROR, "Medium Error" },
  { SCSI_SENSE_HARDWARE_ERROR, "Hardware Error" },
  { SCSI_SENSE_ILLEGAL_REQUEST, "Illegal Request" },
  { SCSI_SENSE_UNIT_ATTENTION, "Unit Attention" },
  { SCSI_SENSE_DATA_PROTECT, "Data Protect" },
  { SCSI_SENSE_BLANK_CHECK, "Blank Check" },
  { SCSI_SENSE_VENDOR_SPECIFIC, "Vendor Specific" },
  { SCSI_SENSE_COPY_ABORTED, "Copy Aborted" },
  { SCSI_SENSE_ABORTED_COMMAND, "Aborted Command" },
  { SCSI_SENSE_EQUAL, "Equal" },
  { SCSI_SENSE_VOLUME_OVERFLOW, "Volume Overflow" },
  { SCSI_SENSE_MISCOMPARE, "Miscompare" },
  { SCSI_SENSE_COMPLETED, "Completed" },
  { -1, NULL }
};


/**
 * Return a textual description of the given sense code.
 *
 * See http://www.t10.org/lists/asc-num.txt
 *
 * @param sense
 * @return description
 */

SANE_String
sanei_pieusb_decode_sense(struct Pieusb_Sense* sense, PIEUSB_Status *status)
{
    SANE_Char* desc = malloc(200);
    SANE_Char* ptr;
    strcpy (desc, code_to_text (sense_code_text, sense->senseKey));
    ptr = desc + strlen(desc);

  switch (sense->senseKey) {
   case SCSI_SENSE_NOT_READY:
    if (sense->senseCode == SENSE_CODE_WARMING_UP && sense->senseQualifier == 1) {
      strcpy (ptr, ": Logical unit is in the process of becoming ready");
      *status = PIEUSB_STATUS_WARMING_UP;
    }
    else {
      sprintf (ptr, ": senseCode 0x%02x, senseQualifier 0x%02x", sense->senseCode, sense->senseQualifier);
      *status = PIEUSB_STATUS_INVAL;
    }
    break;
   case SCSI_SENSE_UNIT_ATTENTION:
    if (sense->senseCode == 0x1a && sense->senseQualifier == 0) {
        strcpy (ptr, ": Invalid field in parameter list");
        *status = PIEUSB_STATUS_INVAL;
      break;
    } else if (sense->senseCode == 0x20 && sense->senseQualifier == 0) {
        strcpy (ptr, ": Invalid command operation code");
        *status = PIEUSB_STATUS_INVAL;
      break;
    } else if (sense->senseCode == 0x82 && sense->senseQualifier == 0) {
        strcpy (ptr, ": Calibration disable not granted");
        *status = PIEUSB_STATUS_MUST_CALIBRATE;
      break;
    } else if (sense->senseCode == 0x00 && sense->senseQualifier == 6) {
        strcpy (ptr, ": I/O process terminated");
        *status = PIEUSB_STATUS_IO_ERROR;
      break;
    } else if (sense->senseCode == 0x26 && sense->senseQualifier == 0x82) {
        strcpy (ptr, ": MODE SELECT value invalid: resolution too high (vs)");
        *status = PIEUSB_STATUS_INVAL;
      break;
    } else if (sense->senseCode == 0x26 && sense->senseQualifier == 0x83) {
        strcpy (ptr, ": MODE SELECT value invalid: select only one color (vs)");
        *status = PIEUSB_STATUS_INVAL;
      break;
    } else if (sense->senseCode == 0x26 && sense->senseQualifier == 0x83) {
        strcpy (ptr, ": MODE SELECT value invalid: unsupported bit depth (vs)");
        *status = PIEUSB_STATUS_INVAL;
      break;
    }
    /*fallthru*/
   case SCSI_SENSE_NO_SENSE:
   case SCSI_SENSE_RECOVERED_ERROR:
   case SCSI_SENSE_MEDIUM_ERROR:
   case SCSI_SENSE_HARDWARE_ERROR:
   case SCSI_SENSE_ILLEGAL_REQUEST:
   case SCSI_SENSE_DATA_PROTECT:
   case SCSI_SENSE_BLANK_CHECK:
   case SCSI_SENSE_VENDOR_SPECIFIC:
   case SCSI_SENSE_COPY_ABORTED:
   case SCSI_SENSE_ABORTED_COMMAND:
   case SCSI_SENSE_EQUAL:
   case SCSI_SENSE_VOLUME_OVERFLOW:
   case SCSI_SENSE_MISCOMPARE:
   case SCSI_SENSE_COMPLETED:
   default:
      sprintf (ptr, ": senseCode 0x%02x, senseQualifier 0x%02x", sense->senseCode, sense->senseQualifier);
      *status = PIEUSB_STATUS_INVAL;
  }
  return desc;
}

/**
 * Prepare IEEE1284 interface
 * Issue one of IEEE1284_ADDR, IEEE1284_RESET, or IEEE1284_SCSI
 *
 * @param device_number Device number
 * @param command - IEEE1284 command
 * @returns SANE_Status
 */

static SANE_Status
_ieee_command(SANE_Int device_number, SANE_Byte command)
{
    SANE_Status st;
  static int sequence[] = { 0xff, 0xaa, 0x55, 0x00, 0xff, 0x87, 0x78 };
#define SEQUENCE_LEN 7
  unsigned int i;
    /* 2 x 4 + 3 bytes preceding command, then SCSI_COMMAND_LEN bytes command */
    /* IEEE1284 command, see hpsj5s.c:cpp_daisy() */
  for (i = 0; i < SEQUENCE_LEN; ++i) {
    st = _ctrl_out_byte (device_number, PORT_PAR_DATA, sequence[i]);
    if (st != SANE_STATUS_GOOD) {
      DBG (DBG_error, "\t\t_ieee_command fails after %d bytes\n", i);
      return st;
    }
  }
  st = _ctrl_out_byte (device_number, PORT_PAR_DATA, command);
  if (st == SANE_STATUS_GOOD) {
    usleep(3000); /* 3.000 usec -> 3 msec */
    st = _ctrl_out_byte (device_number, PORT_PAR_CTRL, C1284_NINIT|C1284_NSTROBE); /* CTRL_VAL_FINAL */
    if (st == SANE_STATUS_GOOD) {
      st = _ctrl_out_byte (device_number, PORT_PAR_CTRL, C1284_NINIT);
      if (st == SANE_STATUS_GOOD) {
	st = _ctrl_out_byte (device_number, PORT_PAR_DATA, 0xff);
	if (st != SANE_STATUS_GOOD) {
	  DBG (DBG_error, "\t\t_ieee_command fails to write final data\n");
	}
      }
      else {
	DBG (DBG_error, "\t\t_ieee_command fails to reset strobe\n");
      }
    }
    else {
      DBG (DBG_error, "\t\t_ieee_command fails to set strobe\n");
    }
  }

  return st;
#undef SEQUENCE_LEN
}

/**
 * Send a command to the device.
 * The command is a SCSI_COMMAND_LEN-byte array. The data-array is used for input and output.
 * The sense-fields of Pieusb_Command_Status are cleared.
 *
 * @param device_number Device number
 * @param command Command array
 * @param data Input or output data buffer
 * @param size Size of the data buffer
 * @returns PIEUSB_SCSI_Status
 */
static PIEUSB_USB_Status
_pieusb_scsi_command(SANE_Int device_number, SANE_Byte command[], SANE_Byte data[], SANE_Int size)
{
  SANE_Status st;
  SANE_Byte usbstat;
  int i;

  DBG (DBG_info_usb, "\t\t_pieusb_scsi_command(): %02x:%s\n", command[0], code_to_text (scsi_code_text, command[0]));

  st = _ieee_command (device_number, IEEE1284_SCSI);
  if (st != SANE_STATUS_GOOD) {
    DBG (DBG_error, "\t\t_pieusb_scsi_command can't prep scsi cmd: %d\n", st);
    return USB_STATUS_ERROR;
  }

  /* output command */
  for (i = 0; i < SCSI_COMMAND_LEN; ++i) {
    SANE_Status st;
    st = _ctrl_out_byte (device_number, PORT_SCSI_CMD, command[i]);
    if (st != SANE_STATUS_GOOD) {
      DBG (DBG_error, "\t\t_pieusb_scsi_command fails command out, after %d bytes: %d\n", i, st);
      return USB_STATUS_ERROR;
    }
  }
  _hexdump ("Cmd", command, SCSI_COMMAND_LEN);

  /* Verify this sequence */
  st = _ctrl_in_byte (device_number, &usbstat);
  if (st != SANE_STATUS_GOOD) {
    DBG (DBG_error, "\t\t_pieusb_scsi_command fails status after command out: %d\n", st);
    return USB_STATUS_ERROR;
  }
  /* Process rest of the data, if present; either input or output, possibly bulk */
  DBG (DBG_info_usb, "\t\t_pieusb_scsi_command usbstat 0x%02x\n", usbstat);
  if (usbstat == USB_STATUS_OK && size > 0) {
    /*
     * send additional data to usb
     */
    _hexdump ("Out", data, size);
    for (i = 0; i < size; ++i) {
      st = _ctrl_out_byte (device_number, PORT_SCSI_CMD, data[i]);
      if (st != SANE_STATUS_GOOD) {
	DBG (DBG_error, "\t\t_pieusb_scsi_command fails data out after %d bytes: %d\n", i, st);
	return USB_STATUS_ERROR;
      }
    }
    /* Verify data out */
    st = _ctrl_in_byte (device_number, &usbstat);
    if (st != SANE_STATUS_GOOD) {
      DBG (DBG_error, "\t\t_pieusb_scsi_command fails status after data out: %d\n", st);
      return USB_STATUS_ERROR;
    }
  }
  else if (usbstat == USB_STATUS_READ) {
    /* Intermediate status OK, device has made data available for reading */
    /* Read data */
    size_t remsize;
    size_t partsize;

    remsize = (size_t)size;

    DBG (DBG_info_usb, "\t\t_pieusb_scsi_command data in\n");
    while (remsize > 0) {
      partsize = remsize > 0x1000000 ? 0x1000000 : remsize; /* 0xc000 must be multiples of 0x4000, see _bulk_in() */
      /* send expected length */
      st = _bulk_size (device_number, partsize);
      if (st != SANE_STATUS_GOOD) {
	DBG (DBG_error, "\t\t_pieusb_scsi_command prepare read data failed for size %u: %d\n", (unsigned int)partsize, st);
	return USB_STATUS_ERROR;
      }
      /* read expected length bytes */
      st = _bulk_in (device_number, data + size - remsize, &partsize);
      if (st != SANE_STATUS_GOOD) {
	DBG (DBG_error, "\t\t_pieusb_scsi_command read data failed for size %u: %d\n", (unsigned int)partsize, st);
	return USB_STATUS_ERROR;
      }
      remsize -= partsize;
/*      DBG (DBG_info, "\t\t_pieusb_scsi_command partsize %08x, remsize %08x\n", (unsigned int)partsize, (unsigned int)remsize); */
    }
    /* Verify data in */
    st = _ctrl_in_byte (device_number, &usbstat);
    if (st != SANE_STATUS_GOOD) {
      DBG (DBG_error, "\t\t_pieusb_scsi_command fails status after data in: %d\n", st);
      return USB_STATUS_ERROR;
    }
    _hexdump ("In", data, size);
  }

  return usbstat;
}


/**
 * Simplified control transfer: one byte to given port
 *
 * @param device_number device number
 * @param b byte to send to device
 * @return SANE status
 */
static SANE_Status _ctrl_out_byte(SANE_Int device_number, SANE_Int port, SANE_Byte b) {
    /* int r = libusb_control_transfer(scannerHandle, CTRL_OUT, 0x0C, 0x0088, ANYINDEX, &b, 1, TIMEOUT); */
    return sanei_usb_control_msg(device_number, REQUEST_TYPE_OUT, REQUEST_REGISTER, port, ANYINDEX, 1, &b);
}


/**
 * Simplified control transfer for port/wValue = 0x82 - prepare bulk
 *
 * @param device_number device number
 * @param size Size of bulk transfer which follows (number of bytes)
 * @return SANE status
 */
static SANE_Status _bulk_size(SANE_Int device_number, unsigned int size) {
    SANE_Byte bulksize[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    bulksize[4] = size & 0xff;
    bulksize[5] = (size >> 8) & 0xff;
    bulksize[6] = (size >> 16) & 0xff;
    bulksize[7] = (size >> 24) & 0xff;
    return sanei_usb_control_msg(device_number, REQUEST_TYPE_OUT, REQUEST_BUFFER, PORT_SCSI_SIZE, ANYINDEX, 8, bulksize);
}


/*
 * Ctrl inbound, single byte
 */
/**
 * Inbound control transfer
 *
 * @param device_number device number
 * @param b byte received from device
 * @return SANE status
 */
static SANE_Status _ctrl_in_byte(SANE_Int device_number, SANE_Byte* b) {
    /* int r = libusb_control_transfer(scannerHandle, CTRL_IN, 0x0C, 0x0084, ANYINDEX, &b, 1, TIMEOUT); */
    /* int r = libusb_control_transfer(scannerHandle, CTRL_IN, 0x0C, 0x0084, ANYINDEX, &b, 1, TIMEOUT); */
    return sanei_usb_control_msg(device_number, REQUEST_TYPE_IN, REQUEST_REGISTER, PORT_SCSI_STATUS, ANYINDEX, 1, b);
}


/**
 * Bulk in transfer for data, in parts of 0x4000 bytes max
 *
 * @param device_number device number
 * @param data array holding or receiving data (must be preallocated)
 * @param size ptr to size of the data array / actual size on output
 * @return SANE status
 */
static SANE_Status
_bulk_in(SANE_Int device_number, SANE_Byte *data, size_t *size) {
    size_t remaining = 0;
    SANE_Status r = SANE_STATUS_GOOD;
    size_t part;

    remaining = *size;
    while (remaining > 0) {
        /* Determine bulk size */
        part = (remaining >= 0x4000) ? 0x4000 : remaining; /* max 16k per chunk */
/*        DBG (DBG_info, "\t\t_bulk_in: %08x @ %p, %08x rem\n", (unsigned int)part, data, (unsigned int)remaining); */
        r = sanei_usb_read_bulk(device_number, data, &part);
        if (r != SANE_STATUS_GOOD) {
            break;
        }
/*        DBG (DBG_info, "\t\t_bulk_in: -> %d : %08x\n", r, (unsigned int)part);*/
        remaining -= part;
        data += part;
    }
    *size -= remaining;
    return r;
}
