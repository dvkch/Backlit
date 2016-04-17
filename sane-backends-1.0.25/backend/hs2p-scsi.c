/* sane - Scanner Access Now Easy.
   Copyright (C) 2007 Jeremy Johnson
   This file is part of a SANE backend for Ricoh IS450
   and IS420 family of HS2P Scanners using the SCSI controller.
   
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
   If you do not wish that, delete this exception notice. */

#include <time.h>
#include "hs2p.h"


static SANE_String_Const
print_devtype (SANE_Byte devtype)
{
  int i = devtype;
  static SANE_String devtypes[] = {
    "disk",
    "tape",
    "printer",
    "processor",
    "CD-writer",
    "CD-drive",
    "scanner",
    "optical-drive",
    "jukebox",
    "communicator"
  };

  return (i >= 0 && i < NELEMS (devtypes)) ? devtypes[i] : "unknown-device";
}
static void
print_bytes (const void *buf, size_t bufsize)
{
  const SANE_Byte *bp;
  unsigned i;

  for (i = 0, bp = buf; i < bufsize; i++, bp++)
    DBG (DBG_error, "%3d: 0x%02x %d\n", i, *bp, *bp);
}

static void
ScannerDump (HS2P_Scanner * s)
{
  int i;
  HS2P_Info *info;
  SANE_Device *sdev;

  info = &s->hw->info;
  sdev = &s->hw->sane;

  DBG (DBG_info, "\n\n");
  DBG (DBG_info, ">> ScannerDump:\n");
  DBG (DBG_info, "SANE Device: '%s' Vendor: '%s' Model: '%s' Type: '%s'\n",
       sdev->name, sdev->vendor, sdev->model, sdev->type);

  DBG (DBG_info, "Type: '%s' Vendor: '%s' Product: '%s' Revision: '%s'\n",
       print_devtype (info->devtype), info->vendor, info->product,
       info->revision);

  DBG (DBG_info, "Automatic Document Feeder: %s%s%s%s\n",
       info->hasADF ? "Installed " : "Not Installed ",
       info->hasSimplex ? "simplex" : "",
       info->hasDuplex ? "duplex" : "",
       info->hasARDF ? "reverse double-sided" : "");
  DBG (DBG_info, "Endorser             :%s\n",
       info->hasEndorser ? " <Installed>" : " <Not Installed>");
  DBG (DBG_info, "Image Processing Unit:%s\n",
       info->hasIPU ? " <Installed>" : " <Not Installed>");
  DBG (DBG_info, "Extended Board       :%s\n",
       info->hasXBD ? " <Installed>" : " <Not Installed>");

  DBG (DBG_info, "\n");
  DBG (DBG_info, "Image Composition Support\n");
  DBG (DBG_info, "Line Art (B/W) Support      : %s\n",
       info->supports_lineart ? "Yes" : "No");
  DBG (DBG_info, "Dithering (Halftone) Support: %s\n",
       info->supports_dithering ? "Yes" : "No");
  DBG (DBG_info, "Error Diffusion Support     : %s\n",
       info->supports_errordiffusion ? "Yes" : "No");
  DBG (DBG_info, "Color Support               : %s\n",
       info->supports_color ? "Yes" : "No");
  DBG (DBG_info, "4 Bit Gray Support          : %s\n",
       info->supports_4bitgray ? "Yes" : "No");
  DBG (DBG_info, "5-8 Bit Gray Support        : %s\n",
       info->supports_8bitgray ? "Yes" : "No");

  DBG (DBG_info, "Image Data processing:%s%s%s%s%s%s\n",
       info->supports_whiteframing ? " <White Frame>" : "",
       info->supports_blackframing ? " <Black Frame>" : "",
       info->supports_edgeextraction ? " <Edge Extraction>" : "",
       info->supports_noiseremoval ? " <Noise Filter>" : "",
       info->supports_smoothing ? " <Smooth>" : "",
       info->supports_linebolding ? " <Line Bolding>" : "");

  DBG (DBG_info, "Image Compression:%s%s%s%s\n",
       info->supports_MH ? " <MH support>" : "",
       info->supports_MR ? " <MR support>" : "",
       info->supports_MMR ? " <MMR support>" : "",
       info->supports_MHB ? " <MH byte boundary support>" : "");
  DBG (DBG_info, "Marker Recognition: %s\n",
       info->supports_markerrecognition ? "<supported>" : "<not supported>");
  DBG (DBG_info, "Size Recognition  : %s\n",
       info->supports_sizerecognition ? "<supported>" : "<not supported>");
  DBG (DBG_info, "X Maximum Output Pixels = %d\n", info->xmaxoutputpixels);

  /*
     DBG (DBG_info, "Optional Features:%s%s%s%s\n",
     info->canBorderRecog ? " <Border Recognition>" : "",
     info->canBarCode ? " <BarCode Decoding>" : "",
     info->canIcon ? " <Icon Generation>" : "",
     info->canSection ? " <Section Support>" : "");
   */

  DBG (DBG_info, "Max bytes per scan-line: %d (%d pixels)\n",
       info->xmaxoutputpixels / 8, info->xmaxoutputpixels);

  DBG (DBG_info, "Basic resolution   (X/Y) : %d/%d\n", info->resBasicX,
       info->resBasicY);
  DBG (DBG_info, "Maximum resolution (X/Y) : %d/%d\n", info->resMaxX,
       info->resMaxY);
  DBG (DBG_info, "Minimum resolution (X/Y) : %d/%d\n", info->resMinX,
       info->resMinY);
  DBG (DBG_info, "Standard Resolutions:\n");
  for (i = 1; i <= info->resStdList[0]; i++)
    DBG (DBG_info, " %d\n", info->resStdList[i]);

  DBG (DBG_info,
       "Window Width/Height (in basic res) %d/%d (%.2f/%.2f inches)\n",
       info->winWidth, info->winHeight,
       (info->resBasicX !=
	0) ? ((float) info->winWidth) / info->resBasicX : 0.0,
       (info->resBasicY) ? ((float) info->winHeight) / info->resBasicY : 0.0);

  /*
     DBG (DBG_info, "Summary:%s%s%s\n",
     info->canDuplex ? "Duplex Scanner" : "Simplex Scanner",
     info->canACE ? " (ACE capable)" : "",
     info->canCheckADF ? " (ADF Paper Sensor capable)" : "");
   */

  DBG (DBG_info, "Buffer Full Ratio     = %#02x\n",
       info->cxn.buffer_full_ratio);
  DBG (DBG_info, "Buffer Empty Ratio    = %#02x\n",
       info->cxn.buffer_empty_ratio);
  DBG (DBG_info, "Bus Inactive Limit    = %#02x\n",
       info->cxn.bus_inactive_limit[0] << 8 | info->cxn.
       bus_inactive_limit[1]);
  DBG (DBG_info, "Disconnect Time Limit = %#04x\n",
       info->cxn.disconnect_time_limit[0] << 8 | info->cxn.
       disconnect_time_limit[1]);
  DBG (DBG_info, "Connect Time Limit    = %#02x\n",
       info->cxn.connect_time_limit[0] << 8 | info->cxn.
       connect_time_limit[1]);
  DBG (DBG_info, "Maximum Burst Size    = %#04x\n",
       info->cxn.maximum_burst_size[0] << 8 | info->cxn.
       maximum_burst_size[1]);
  DBG (DBG_info, "DTDC                  = %#02x\n", info->cxn.dtdc & 0x03);

  DBG (DBG_info, "White Balance is %s\n",
       info->white_balance == 1 ? "Absolute" : "Relative");
  DBG (DBG_info, "Medium Wait Timer is <not supported>\n");	/* get_medium_wait_timer(fd) */
  DBG (DBG_info, "Scan Wait Mode is %s\n",
       info->scan_wait_mode == 0 ? "OFF" : "ON");
  DBG (DBG_info, "Service Mode is in Select %s Mode\n",
       info->service_mode == 0 ? "Self-Diagnostics" : "Optical Adjustment");

  sprintf (info->inquiry_data, "Vendor: %s Product: %s Rev: %s %s%s\n",
	   info->vendor, info->product, info->revision,
	   info->hasADF && info->hasDuplex ? "Duplex Scanner" : "",
	   info->hasADF && info->hasSimplex ? "Simplex Scanner" : "");

  DBG (DBG_info, "duplex_default=%d\n", info->default_duplex);
  /*
     DBG (DBG_info, "autoborder_default=%d\n", info->autoborder_default);
     DBG (DBG_info, "batch_default=%d\n", info->batch_default);
     DBG (DBG_info, "deskew_default=%d\n", info->deskew_default);
     DBG (DBG_info, "check_adf_default=%d\n", info->check_adf_default);
     DBG (DBG_info, "timeout_adf_default=%d\n", info->timeout_adf_default);
     DBG (DBG_info, "timeout_manual_default=%d\n", info->timeout_manual_default);
     DBG (DBG_info, "control_panel_default=%d\n", info->control_panel_default);
   */

  DBG (DBG_info, "bmu = %d\n", info->bmu);
  DBG (DBG_info, "mud = %d\n", info->mud);
  DBG (DBG_info, "white balance = %#0x\n", info->white_balance);
  DBG (DBG_info, "adf control = %#0x\n", info->adf_control);
  DBG (DBG_info, "adf mode control = %#0x\n", info->adf_mode_control);
  DBG (DBG_info, "endorser control = %#0x\n", info->endorser_control);
  DBG (DBG_info, "endorser string = %s\n", info->endorser_string);
  DBG (DBG_info, "scan wait mode = %#0x\n", info->scan_wait_mode);
  DBG (DBG_info, "service mode = %#0x\n", info->service_mode);

  DBG (DBG_info, "BasicXRes = %d\n", info->resBasicX);
  DBG (DBG_info, "BasicYRes = %d\n", info->resBasicY);

  DBG (DBG_info, "XResStep  = %d\n", info->resXstep);
  DBG (DBG_info, "YResStep  = %d\n", info->resYstep);

  DBG (DBG_info, "MaxXres   = %d\n", info->resMaxX);
  DBG (DBG_info, "MaxYres   = %d\n", info->resMaxY);

  DBG (DBG_info, "MinXres   = %d\n", info->resMinX);
  DBG (DBG_info, "MinYres   = %d\n", info->resMinY);

  DBG (DBG_info, "Width     = %d\n", info->winWidth);
  DBG (DBG_info, "Height    = %d\n", info->winHeight);

  DBG (DBG_info, "<< ScannerDump\n");
}
static void
print_vpd_info (struct inquiry_vpd_data *vbuf)
{
  DBG (DBG_info, "VPD IDENTIFIER C0H\n");
  DBG (DBG_info, "[00] Peripheral             %#02x\n", vbuf->devtype);
  DBG (DBG_info, "[01] Page Code              %#02x\n", vbuf->pagecode);
  DBG (DBG_info, "[02] reserved               %#02x\n", vbuf->byte2);
  DBG (DBG_info, "[03] Page Length            %#02x\n", vbuf->pagelength);
  DBG (DBG_info, "[04] ADF ID                 %#02x\n", vbuf->adf_id);
  DBG (DBG_info, "[05] Endorser ID            %#02x\n", vbuf->end_id);
  DBG (DBG_info, "[06] Image Processing Unit  %#02x\n", vbuf->ipu_id);
  DBG (DBG_info, "[07] Image Composition      %#02x\n",
       vbuf->imagecomposition);
  DBG (DBG_info, "[08] Image Data Processing  %lu\n",
       _2btol (&vbuf->imagedataprocessing[0]));
  DBG (DBG_info, "[10] Compression            %#02x\n", vbuf->compression);
  DBG (DBG_info, "[11] Marker Recognition     %#02x\n",
       vbuf->markerrecognition);
  DBG (DBG_info, "[12] Size Recognition       %#02x\n",
       vbuf->sizerecognition);
  DBG (DBG_info, "[13] reserved               %#02x\n", vbuf->byte13);
  DBG (DBG_info, "[14] X Maximum Output Pixel %lu\n",
       _2btol (&vbuf->xmaxoutputpixels[0]));
}
static void
print_jis_info (struct inquiry_jis_data *jbuf)
{
  DBG (DBG_info, "JIS IDENTIFIER F0H\n");
  DBG (DBG_info, "[00] devtype   %#02x\n", jbuf->devtype);
  DBG (DBG_info, "[01] Page Code %#02x\n", jbuf->pagecode);
  DBG (DBG_info, "[02] JIS Ver   %#02x\n", jbuf->jisversion);
  DBG (DBG_info, "[03] reserved1 %#02x\n", jbuf->reserved1);
  DBG (DBG_info, "[04] Page Len  %#02x\n", jbuf->alloclen);
  DBG (DBG_info, "[05] BasicXRes %lu\n", _2btol (&jbuf->BasicRes.x[0]));
  DBG (DBG_info, "[07] BasicYRes %lu\n", _2btol (&jbuf->BasicRes.y[0]));
  DBG (DBG_info, "[09] Resolution step %#02x\n", jbuf->resolutionstep);
  DBG (DBG_info, "[10] MaxXRes   %lu\n", _2btol (&jbuf->MaxRes.x[0]));
  DBG (DBG_info, "[12] MaxYRes   %lu\n", _2btol (&jbuf->MaxRes.y[0]));
  DBG (DBG_info, "[14] MinXRes   %lu\n", _2btol (&jbuf->MinRes.x[0]));
  DBG (DBG_info, "[16] MinYRes   %lu\n", _2btol (&jbuf->MinRes.y[0]));
  DBG (DBG_info, "[18] Std Res   %#0x\n",
       (jbuf->standardres[0] << 8) | jbuf->standardres[1]);
  DBG (DBG_info, "[20] Win Width %lu\n", _4btol (&jbuf->Window.width[0]));	/* Manual says 4787/12B3H pixels @400dpi = 12in */
  DBG (DBG_info, "[24] Win Len   %lu\n", _4btol (&jbuf->Window.length[0]));	/* Manual says 6803/1A93H pixels @400dpi = 17in) */
  DBG (DBG_info, "[28] function  %#02x\n", jbuf->functions);
  DBG (DBG_info, "[29] reserved  %#02x\n", jbuf->reserved2);
}

/* 1-3-1                  TEST UNIT READY 
  Byte0: |                           0x00                       |
  Byte1: | 7-5 Logical Unit Number | Reserved                   |
  Byte2: |                           Reserved                   |
  Byte3: |                           Reserved                   |
  Byte4: |                           Reserved                   |
  Byte5: | 7-6 Vendor Unique | 5-2   Reserved | 1 Flag | 0 Link |
*/
static SANE_Status
test_unit_ready (int fd)
{
  static SANE_Byte cmd[6];
  SANE_Status status;
  DBG (DBG_proc, ">> test_unit_ready\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = HS2P_SCSI_TEST_UNIT_READY;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (DBG_proc, "<< test_unit_ready\n");
  return (status);
}

/* 1-3-2                  REQUEST SENSE
  Byte0: |                           0x00                       |
  Byte1: | 7-5 Logical Unit Number | Reserved                   |
  Byte2: |                           Reserved                   |
  Byte3: |                           Reserved                   |
  Byte4: |                     Allocation Length                |
  Byte5: | 7-6 Vendor Unique | 5-2   Reserved | 1 Flag | 0 Link |
*/

#if 0
static SANE_Status
get_sense_data (int fd, SENSE_DATA * sense_data)
{
  SANE_Status status;
  DBG (DBG_sane_proc, ">> get_sense_data\n");

  static SANE_Byte cmd[6];
  size_t len;

  len = sizeof (*sense_data);
  memset (sense_data, 0, len);
  memset (cmd, 0, sizeof (cmd));

  cmd[0] = HS2P_SCSI_REQUEST_SENSE;
  cmd[4] = len;

  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), sense_data, &len);

  DBG (DBG_proc, "<< get_sense_data\n");
  return (status);
}
#endif

static SANE_Status
print_sense_data (int dbg_level, SENSE_DATA * data)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Byte *bp, *end;
  SANE_Int i;

  DBG (DBG_sane_proc, ">> print_sense_data\n");

  bp = (SANE_Byte *) data;
  end = bp + (SANE_Byte) sizeof (SENSE_DATA);
  for (i = 0; bp < end; bp++, i++)
    {
      DBG (dbg_level, "Byte #%2d is %3d, 0x%02x\n", i, *bp, *bp);
    }

  DBG (dbg_level, "Valid=%1d, ErrorCode=%#x\n",
       (data->error_code & 0x80) >> 7, data->error_code & 0x7F);
  DBG (dbg_level, "Segment number = %d\n", data->segment_number);
  DBG (dbg_level,
       "F-mark=%1d, EOM=%1d, ILI=%1d, Reserved=%1d, SenseKey=%#x\n",
       (data->sense_key & 0x80) >> 7, (data->sense_key & 0x40) >> 6,
       (data->sense_key & 0x20) >> 5, (data->sense_key & 0x10) >> 4,
       (data->sense_key & 0x0F));
  DBG (dbg_level, "Information Byte = %lu\n", _4btol (data->information));
  DBG (dbg_level, "Additional Sense Length = %d\n", data->sense_length);
  DBG (dbg_level, "Command Specific Infomation = %lu\n",
       _4btol (data->command_specific_information));
  DBG (dbg_level, "Additional Sense Code = %#x\n", data->sense_code);
  DBG (dbg_level, "Additional Sense Code Qualifier = %#x\n",
       data->sense_code_qualifier);

  DBG (DBG_proc, "<< print_sense_data\n");
  return (status);
}

static struct sense_key *
lookup_sensekey_errmsg (int code)
{
  int i;
  struct sense_key *k = &sensekey_errmsg[0];

  for (i = 0; i < 16; i++, k++)
    if (k->key == code)
      return k;
  return NULL;
}
static struct ASCQ *
lookup_ascq_errmsg (unsigned int code)
{
  unsigned int i;
  struct ASCQ *k = &ascq_errmsg[0];

  for (i = 0; i < 74; i++, k++)
    if (k->codequalifier == code)
      return k;
  return NULL;
}

/* a sensible sense handler
   arg is a pointer to the associated HS2P_Scanner structure 
   
   SENSE DATA FORMAT:  14 bytes bits[7-0]
   Byte  0: [7]:valid [6-0]:Error Code
   Byte  1: Segment Number
   Byte  2: [7]: F-mark; [6]:EOM; [5]:ILI; [4]:reserved; [3-0]:Sense Key
   Byte  3: Information Byte
   Byte  4: Information Byte
   Byte  5: Information Byte
   Byte  6: Information Byte
   Byte  7: Additional Sense Length (n-7)
   Byte  8: Command Specific Information
   Byte  9: Command Specific Information
   Byte 10: Command Specific Information
   Byte 11: Command Specific Information
   Byte 12: Additional Sense Code
   Byte 13: Additional Sense Code Qualifier
*/
static SANE_Status
sense_handler (int __sane_unused__ scsi_fd, u_char * sense_buffer, void *sd)
{
  u_char sense, asc, ascq, EOM, ILI, ErrorCode, ValidData;
  u_long MissingBytes;
  char *sense_str = "";

  struct sense_key *skey;
  struct ASCQ *ascq_key;
  SENSE_DATA *sdp = (SENSE_DATA *) sd;
  SANE_Int i;
  SANE_Status status = SANE_STATUS_INVAL;
  SANE_Char print_sense[(16 * 3) + 1];

  DBG (DBG_proc, ">> sense_handler\n");
  if (DBG_LEVEL >= DBG_info)
    print_sense_data (DBG_LEVEL, (SENSE_DATA *) sense_buffer);

  /* store sense_buffer */
  DBG (DBG_info, ">> copying %lu bytes from sense_buffer[] to sense_data\n",
       (u_long) sizeof (SENSE_DATA));
  memcpy (sdp, sense_buffer, sizeof (SENSE_DATA));
  if (DBG_LEVEL >= DBG_info)
    print_sense_data (DBG_LEVEL, sdp);

  ErrorCode = sense_buffer[0] & 0x7F;
  ValidData = (sense_buffer[0] & 0x80) != 0;
  sense = sense_buffer[2] & 0x0f;	/* Sense Key */
  asc = sense_buffer[12];	/* Additional Sense Code */
  ascq = sense_buffer[13];	/* Additional Sense Code Qualifier */
  EOM = (sense_buffer[2] & 0x40) != 0;	/* End Of Media */
  ILI = (sense_buffer[2] & 0x20) != 0;	/* Invalid Length Indicator */
  MissingBytes = ValidData ? _4btol (&sense_buffer[3]) : 0;

  DBG (DBG_sense,
       "sense_handler: sense_buffer=%#x, sense=%#x, asc=%#x, ascq=%#x\n",
       sense_buffer[0], sense, asc, ascq);
  DBG (DBG_sense,
       "sense_handler: ErrorCode %02x ValidData: %d "
       "EOM: %d ILI: %d MissingBytes: %lu\n", ErrorCode, ValidData, EOM,
       ILI, MissingBytes);

  memset (print_sense, '\0', sizeof (print_sense));
  for (i = 0; i < 16; i++)
    sprintf (print_sense + strlen (print_sense), "%02x ", sense_buffer[i]);
  DBG (DBG_sense, "sense_handler: sense=%s\n", print_sense);

  if (ErrorCode != 0x70 && ErrorCode != 0x71)
    {
      DBG (DBG_error, "sense_handler: error code is invalid.\n");
      return SANE_STATUS_IO_ERROR;	/* error code is invalid */
    }

  skey = lookup_sensekey_errmsg (sense);	/* simple sequential search */
  DBG (DBG_sense, "sense_handler: sense_key=%#x '%s - %s'\n", skey->key,
       skey->meaning, skey->description);

  DBG (DBG_sense, "Looking up ascq=(%#x,%#x)=%#x\n", asc, ascq,
       (asc << 8) | ascq);
  ascq_key = lookup_ascq_errmsg ((asc << 8) | ascq);	/* simple sequential search */
  DBG (DBG_sense, "sense_handler: ascq=(%#x,%#x): %#x '%s'\n", asc, ascq,
       ascq_key->codequalifier, ascq_key->description);

  /* handle each sense key: Translate from HS2P message to SANE_STATUS_ message 
   * SANE_STATUS_GOOD, _ACCESS_DEINIED, _NO_MEM, _INVAL, _IO_ERROR, _DEVICE_BUSY, 
   * _EOF, _UNSUPPORTED, _CANCELLED, _JAMMED, _NO_DOCS, _COVER_OPEN 
   */
  switch (sense)
    {
    case 0x00:			/* no sense */
      status = SANE_STATUS_GOOD;
      break;
    case 0x01:			/* recovered error */
      status = SANE_STATUS_INVAL;
      break;
    case 0x02:			/* not ready */
      status = SANE_STATUS_DEVICE_BUSY;
      break;
    case 0x03:			/* medium error */
      status = SANE_STATUS_JAMMED;
      break;
    case 0x04:			/* hardware error */
      status = SANE_STATUS_IO_ERROR;
      break;
    case 0x05:			/* illegal request */
      status = SANE_STATUS_INVAL;
      break;
    case 0x06:			/* unit attention */
      status = SANE_STATUS_GOOD;
      break;
    case 0x07:			/* data protect */
      status = SANE_STATUS_INVAL;
      break;
    case 0x08:			/* blank check */
      status = SANE_STATUS_INVAL;
      break;
    case 0x09:			/* vendor specific */
      status = SANE_STATUS_INVAL;
      break;
    case 0x0A:			/* copy aborted */
      status = SANE_STATUS_CANCELLED;
      break;
    case 0x0B:			/* aborted command */
      status = SANE_STATUS_CANCELLED;
      break;
    case 0x0C:			/* equal */
      status = SANE_STATUS_INVAL;
      break;
    case 0x0D:			/* volume overflow */
      status = SANE_STATUS_INVAL;
      break;
    case 0x0E:			/* miscompare */
      status = SANE_STATUS_INVAL;
      break;
    case 0x0F:			/* reserved */
      status = SANE_STATUS_INVAL;
      break;
    }
  if (ErrorCode == 0x70)	/* Additional Sense Codes available */
    switch ((asc << 8) | ascq)
      {
      case 0x0000:		/* No additional Information */
	status = SANE_STATUS_GOOD;
	break;
      case 0x0002:		/* End of Medium */
	status = SANE_STATUS_NO_DOCS;
	break;
      case 0x0005:		/* End of Data */
	status = SANE_STATUS_EOF;
	break;
      case 0x0400:		/* LUN not ready */
	status = SANE_STATUS_DEVICE_BUSY;
	break;
      case 0x0401:		/* LUN becoming ready */
	status = SANE_STATUS_DEVICE_BUSY;
	break;
      case 0x0403:		/* LUN not ready. Manual intervention needed */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x0500:		/* LUN doesn't respond to selection */
	status = SANE_STATUS_INVAL;
	break;
      case 0x0700:		/* Multiple peripheral devices selected */
	status = SANE_STATUS_INVAL;
	break;
      case 0x1100:		/* Unrecovered read error */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x1101:		/* Read retries exhausted */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x1501:		/* Mechanical positioning error */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x1A00:		/* Parameter list length error */
	status = SANE_STATUS_INVAL;
	break;
      case 0x2000:		/* Invalid command operation code */
	status = SANE_STATUS_INVAL;
	break;
      case 0x2400:		/* Invalid field in CDB (check field pointer) */
	status = SANE_STATUS_INVAL;
	break;
      case 0x2500:		/* LUN not supported */
	status = SANE_STATUS_UNSUPPORTED;
	break;
      case 0x2600:		/* Invalid field in parameter list (check field pointer) */
	status = SANE_STATUS_INVAL;
	break;
      case 0x2900:		/* Power on, reset, or BUS DEVICE RESET occurred */
	status = SANE_STATUS_GOOD;
	break;
      case 0x2A01:		/* (MODE parameter changed) */
	status = SANE_STATUS_INVAL;
	break;
      case 0x2C00:		/* Command sequence error */
	status = SANE_STATUS_INVAL;
	break;
      case 0x2C01:		/* Too many windows specified */
	status = SANE_STATUS_INVAL;
	break;
      case 0x2C02:		/* Invalid combination of windows specified */
	status = SANE_STATUS_INVAL;
	break;
      case 0x3700:		/* (Rounded paramter) */
	status = SANE_STATUS_INVAL;
	break;
      case 0x3900:		/* (Saving parameters not supported) */
	status = SANE_STATUS_INVAL;
	break;
      case 0x3A00:		/* Medium not present */
	status = SANE_STATUS_NO_DOCS;
	break;
      case 0x3B09:		/* Read past end of medium */
	status = SANE_STATUS_EOF;
	break;
      case 0x3B0B:		/* Position past end of medium */
	status = SANE_STATUS_EOF;
	break;
      case 0x3D00:		/* Invalid bits in IDENTIFY message */
	status = SANE_STATUS_INVAL;
	break;
      case 0x4300:		/* Message error */
	status = SANE_STATUS_INVAL;
	break;
      case 0x4500:		/* Select/Reselect failure */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x4700:		/* (SCSI parity error) */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x4800:		/* Initiator detected error message received */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x4900:		/* Invalid message error */
	status = SANE_STATUS_INVAL;
	break;
      case 0x4B00:		/* Data phase error */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x5300:		/* (Media Load/Eject failed) */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x6000:		/* Lamp failure */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x6001:		/* Shading error */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x6002:		/* White adjustment error */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x6010:		/* Reverse Side Lamp Failure */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x6200:		/* Scan head positioning error */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x6300:		/* Document Waiting Cancel */
	status = SANE_STATUS_CANCELLED;
	break;
      case 0x8000:		/* (PSU over heate) */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x8001:		/* (PSU 24V fuse down) */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x8002:		/* (ADF 24V fuse down) */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x8003:		/* (5V fuse down) */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x8004:		/* (-12V fuse down) */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x8100:		/* (ADF 24V power off) */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x8102:		/* (Base 12V power off) */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x8103:		/* Lamp cover open (Lamp 24V power off) */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x8104:		/* (-12V power off) */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x8105:		/* (Endorser 6V power off) */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x8106:		/* SCU 3.3V power down error */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x8107:		/* RCU 3.3V power down error */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x8108:		/* OIPU 3.3V power down error */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x8200:		/* Memory Error (Bus error) */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x8210:		/* Reverse-side memory error (Bus error) */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x8300:		/* (Image data processing LSI error) */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x8301:		/* (Interface LSI error) */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x8302:		/* (SCSI controller error) */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x8303:		/* (Compression unit error) */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x8304:		/* (Marker detect unit error) */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x8400:		/* Endorser error */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x8500:		/* (Origin Positioning error) */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x8600:		/* Mechanical Time Out error (Pick Up Roller error) */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x8700:		/* (Heater error) */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x8800:		/* (Thermistor error) */
	status = SANE_STATUS_IO_ERROR;
	break;
      case 0x8900:		/* ADF cover open */
	status = SANE_STATUS_COVER_OPEN;
	break;
      case 0x8901:		/* (ADF lift up) */
	status = SANE_STATUS_COVER_OPEN;
	break;
      case 0x8902:		/* Document jam error for ADF */
	status = SANE_STATUS_JAMMED;
	break;
      case 0x8903:		/* Document misfeed for ADF */
	status = SANE_STATUS_JAMMED;
	break;
      case 0x8A00:		/* (Interlock open) */
	status = SANE_STATUS_COVER_OPEN;
	break;
      case 0x8B00:		/* (Not enough memory) */
	status = SANE_STATUS_NO_MEM;
	break;
      case 0x8C00:		/* Size Detection failed */
	status = SANE_STATUS_IO_ERROR;
	break;
      default:			/* Should never get here */
	status = SANE_STATUS_INVAL;
	DBG (DBG_sense,
	     "sense_handler: 'Undocumented code': ascq=(%#x,%#x)\n",
	     asc & 0xFF00, ascq & 0x00FF);
	break;
      }


  DBG (DBG_proc, "sense_handler %s: '%s'-'%s' '%s' return:%d\n", sense_str,
       skey->meaning, skey->description, ascq_key->description, status);
  return status;
}

/* VPD IDENTIFIER Page Code 0x00
 * A list of all Page Codes supported by scanner is returned as data
 * Byte0 => bit7-5: Peripheral Qualifier, bits4-0: Peripheral Device Type
 * Byte1 => Page Code of CDB is set as Page Code 0
 * Byte2 => Reserved
 * Byte3 => Page Length is 2 because scanner supports just two page codes: C0H and F0H
 * Byte4 => First Support Page Code
 * Byte5 => Second Support Page Code
*/
#if 0
static SANE_Status
vpd_indentifier_00H (int fd)
{
  static SANE_Byte cmd[6];
  SANE_Status status;
  DBG (DBG_proc, ">> vpd_identifier_00H\n");

  cmd[0] = HS2P_SCSI_REQUEST_SENSE;
  memset (cmd, 0, sizeof (cmd));
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (DBG_proc, "<< vpd_identifier_00H\n");
  return (status);
}
#endif

#if 0
static SANE_Status
vpd_identifier_C0H (int fd)
{
  static SANE_Byte cmd[6];
  SANE_Status status;
  DBG (DBG_proc, ">> vpd_identifier_C0H\n");

  cmd[0] = HS2P_SCSI_REQUEST_SENSE;
  memset (cmd, 0, sizeof (cmd));
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (DBG_proc, "<< vpd_identifier_C0H\n");
  return (status);
}
#endif

/* 1-3-3 INQUIRY : 6 bytes:
 * Byte0 => 0x12
 * Byte1 => bits7-5: Logical Unit number
 *          bits4-1: Reserved
 *          bit0:    EVPD
 * Byte2 => Page Code
 * Byte3 => Reserved
 * Byte4 => Allocation Length
 * Byte5 => bits7-6: Vendor Unique
 *          bits5-2: Reserved
 *          bit1:    Flag
 *          bit0:    Link
*/
static SANE_Status
inquiry (int fd, void *buf, size_t * buf_size, SANE_Byte evpd,
	 SANE_Byte page_code)
{
  static SANE_Byte cmd[6];
  SANE_Status status;
  DBG (DBG_proc, ">> inquiry\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = HS2P_SCSI_INQUIRY;
  cmd[1] = evpd;
  cmd[2] = page_code;
/*cmd[3] Reserved */
  cmd[4] = *buf_size;
/*cmd[5] vendorunique+reserved+flag+link */
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), buf, buf_size);

  DBG (DBG_proc, "<< inquiry\n");
  return (status);
}

/* 1-3-6 MODE SELECT -- sets various operation mode parameters for scanner */
static SANE_Status
mode_select (int fd, MP * settings)
{
  static struct
  {
    SELECT cmd;			/* Mode page Select command */
    MP mp;			/* Hdr + Parameters         */
  } msc;			/* Mode Select Command      */
  SANE_Status status;
  size_t npages;

  DBG (DBG_proc, ">> mode_select\n");

  memset (&msc, 0, sizeof (msc));	/* Fill struct with zeros     */
  msc.cmd.opcode = HS2P_SCSI_MODE_SELECT;	/* choose Mode Select Command */
  msc.cmd.byte1 &= ~SMS_SP;	/* unset bit0 SavePage to 0   */
  msc.cmd.byte1 |= SMS_PF;	/* set bit4 PageFormat to 1   */
  npages = (settings->page.code == 2) ? 16 : 8;
  msc.cmd.len = sizeof (msc.mp.hdr) + npages;	/* either 4+8 or 4+20      */

  memcpy (&msc.mp, settings, msc.cmd.len);	/* Copy hdr+pages from Settings to msc.mp  */
  memset (&msc.mp.hdr, 0, sizeof (msc.mp.hdr));	/* make sure the hdr is all zeros          */
  /*
     msc.hdr.data_len     = 0x00;
     msc.hdr.medium_type  = 0x00;
     msc.hdr.dev_spec     = 0x00;
     msc.hdr.blk_desc_len = 0x00;
   */

  /* Now execute the whole command */
  if ((status =
       sanei_scsi_cmd (fd, &msc, sizeof (msc.cmd) + msc.cmd.len, 0,
		       0)) != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "ERROR: mode_select: %s\n", sane_strstatus (status));
      DBG (DBG_error, "PRINTING CMD BLOCK:\n");
      print_bytes (&msc.cmd, sizeof (msc.cmd));
      DBG (DBG_error, "PRINTING MP HEADER:\n");
      print_bytes (&msc.mp.hdr, sizeof (msc.mp.hdr));
      DBG (DBG_error, "PRINTING MP PAGES:\n");
      print_bytes (&msc.mp.page, msc.cmd.len);
    }

  DBG (DBG_proc, "<< mode_select\n");
  return (status);
}

/* 1-3-7 MODE SENSE -- gets various operation mode parameters from scanner */
static SANE_Status
mode_sense (int fd, MP * buf, SANE_Byte page_code)
{
  SANE_Status status;
  SENSE cmd;			/* 6byte cmd */
  MP msp;			/* Mode Sense Page
				 * 4byte hdr + {2bytes +14 bytes}
				 * buffer to hold mode sense data gotten from scanner */

  size_t nbytes;

  DBG (DBG_proc, ">>>>> mode_sense: fd=%d, page_code=%#02x\n", fd, page_code);
  nbytes = sizeof (msp);

  DBG (DBG_info,
       ">>>>> mode_sense: Zero'ing ModeSenseCommand msc and msp structures\n");

  memset (&cmd, 0, sizeof (cmd));	/* Fill cmd struct with zeros */
  memset (&msp, 0, sizeof (msp));	/* Fill msp struct with zeros */

  /* set up Mode Sense Command */
  DBG (DBG_info, ">>>>> mode_sense: Initializing Mode Sense cmd\n");
  cmd.opcode = HS2P_SCSI_MODE_SENSE;
  cmd.dbd &= ~(1 << 3);		/* Disable Block Description (bit3) is set to 0 */
  cmd.pc = (page_code & 0x3F);	/* bits 5-0 */
  cmd.pc &= ~(0x03 << 6);	/* unset PC Field (bits7-6)
				 * 00 Curent Value is the only effective value
				 * 01 Changeable Value
				 * 10 Default Value
				 * 11 Saved Value */
  /* cmd.len = ??? Allocation Length */

  /* Now execute the whole command  and store results in msc */
  DBG (DBG_info, ">>>>> mode_sense: sanei_scsi_cmd\n");
  DBG (DBG_info, ">>>>> cmd.opcode=%#0x cmd.dbd=%#02x, cmd.pc=%#02x\n",
       cmd.opcode, cmd.dbd, cmd.pc);

  nbytes = (page_code == 2) ? 20 : 12;
  DBG (DBG_info,
       ">>>>> sizeof(cmd)=%lu sizeof(msp)=%lu sizeof(hdr)=%lu sizeof(page)=%lu requesting %lu bytes\n",
       (u_long) sizeof (cmd), (u_long) sizeof (msp),
       (u_long) sizeof (msp.hdr), (u_long) sizeof (msp.page),
       (u_long) nbytes);

  status = sanei_scsi_cmd (fd, &cmd, sizeof (cmd), &msp, &nbytes);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "ERROR mode_sense: sanei_scsi_cmd error \"%s\"\n",
	   sane_strstatus (status));
      DBG (DBG_error,
	   ">>>>> mode sense: number of bytes received from scanner: %lu\n",
	   (u_long) nbytes);
      DBG (DBG_error, "PRINTING CMD BLOCK:\n");
      print_bytes (&cmd, sizeof (cmd));
      DBG (DBG_error, "PRINTING MP HEADER:\n");
      print_bytes (&msp.hdr, sizeof (msp.hdr));
      DBG (DBG_error, "PRINTING MP PAGES:\n");
      print_bytes (&msp.page, sizeof (msp.page));
    }
  else
    {
      /* nbytes = (page_code==2)? 14 : 6; */
      DBG (DBG_info, ">> >> got %lu bytes from scanner\n", (u_long) nbytes);
      nbytes -= 4;		/* we won't copy 4 byte hdr */
      DBG (DBG_info, ">>>>> copying from msp to calling function's buf\n"
	   ">>>>> msp.page_size=%lu bytes=%lu buf_size=%lu\n",
	   (u_long) sizeof (msp.page), (u_long) nbytes,
	   (u_long) sizeof (*buf));
      memcpy (buf, &(msp.page), nbytes);
    }

  DBG (DBG_proc, "<<<<< mode_sense\n");
  return (status);
}

static SANE_Status
set_window (int fd, SWD * swd)
{
  static struct
  {
    struct set_window_cmd cmd;
    struct set_window_data swd;
  } win;
  SANE_Status status;
  static size_t wdl, tl;	/*window descriptor length, transfer length */
  DBG (DBG_proc, ">> set_window\n");

  /* initialize our struct with zeros */
  memset (&win, 0, sizeof (win));

  /* fill in struct with opcode */
  win.cmd.opcode = HS2P_SCSI_SET_WINDOW;

  /* bytes 1-5 are reserved */

  /* Transfer length is header + window data */
  tl = sizeof (*swd);
  _lto3b (tl, &win.cmd.len[0]);	/* 8 + (2*320) = 648 */
  DBG (DBG_info,
       "set_window: SET WINDOW COMMAND Transfer Length = %lu (should be 648)\n",
       (unsigned long) tl);

  /* Copy data from swd (including 8-byte header) to win.swd */
  DBG (DBG_info,
       "set_window: COPYING %lu bytes from settings to Set Window Command (%lu)\n",
       (u_long) sizeof (*swd), (u_long) sizeof (win.swd));
  if (!memcpy (&(win.swd), swd, sizeof (*swd)))
    {
      DBG (DBG_error, "set_window: error with memcpy\n");
    }

  /* Set Window Data Header: 0-5:reserved; 6-7:Window Descriptor Lenght=640 */
  wdl = sizeof (win.swd) - sizeof (win.swd.hdr);
  _lto2b (wdl, &win.swd.hdr.len[0]);
  DBG (DBG_info,
       "set_window: SET WINDOW COMMAND Window Descriptor Length = %lu (should be 640)\n",
       (unsigned long) wdl);

  /* Now execute command */
  DBG (DBG_info,
       "set_window: calling sanei_scsi_cmd(%d,&win,%lu, NULL, NULL)\n", fd,
       (u_long) sizeof (win));
  status = sanei_scsi_cmd (fd, &win, sizeof (win), NULL, NULL);
  /*
     status = sanei_scsi_cmd2 (fd, &win.cmd, sizeof(win.cmd),  &win.swd, sizeof(win.swd), NULL, NULL);
   */
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "*********************\n");
      DBG (DBG_error, "ERROR: set_window: %s\n", sane_strstatus (status));
      DBG (DBG_error, "PRINTING SWD CMD BLK:\n");
      print_bytes (&win.cmd, sizeof (win.cmd));
      DBG (DBG_error, "PRINTING SWD HEADER:\n");
      print_bytes (&win.swd.hdr, sizeof (win.swd.hdr));
      DBG (DBG_error, "PRINTING SWD DATA[0]:\n");
      print_bytes (&win.swd.data[0], sizeof (win.swd.data[0]));
      DBG (DBG_error, "PRINTING SWD DATA[1]:\n");
      print_bytes (&win.swd.data[1], sizeof (win.swd.data[1]));
      DBG (DBG_error, "*********************\n");
    }

  DBG (DBG_proc, "<< set_window\n");
  return (status);
}

static SANE_Status
get_window (int fd, GWD * gwd)
{
  struct get_window_cmd cmd;
  SANE_Status status;
  static size_t gwd_size;

  DBG (DBG_proc, ">> get_window\n");

  gwd_size = sizeof (*gwd);
  DBG (DBG_info, ">> get_window datalen = %lu\n", (unsigned long) gwd_size);

  /* fill in get_window_cmd */
  memset (&cmd, 0, sizeof (cmd));	/* CLEAR cmd */
  cmd.opcode = HS2P_SCSI_GET_WINDOW;
  cmd.byte1 &= ~0x01;		/* unset single bit 0 */
  cmd.win_id = 0x00;		/* either 0 or 1 */
  _lto3b (gwd_size, cmd.len);	/* Transfer Length is byte length of DATA to be returned */

  status = sanei_scsi_cmd (fd, &cmd, sizeof (cmd), gwd, &gwd_size);

  DBG (DBG_proc, "<< get_window, datalen = %lu\n", (unsigned long) gwd_size);
  return (status);
}
static void
print_window_data (SWD * buf)
{
  int i, j, k;
  struct hs2p_window_data *data;
  struct window_section *ws;

  DBG (DBG_proc, ">> print_window_data\n");
  DBG (DBG_info, "HEADER\n");
  for (i = 0; i < 6; i++)
    DBG (DBG_info, "%#02x\n", buf->hdr.reserved[i]);
  DBG (DBG_info, "Window Descriptor Length=%lu\n\n", _2btol (buf->hdr.len));

  for (i = 0; i < 2; i++)
    {
      data = &buf->data[i];
      DBG (DBG_info, "Window Identifier = %d\n", data->window_id);
      DBG (DBG_info, "AutoBit = %#x\n", data->auto_bit);
      DBG (DBG_info, "X-Axis Resolution = %lu\n", _2btol (data->xres));
      DBG (DBG_info, "Y-Axis Resolution = %lu\n", _2btol (data->yres));
      DBG (DBG_info, "X-Axis Upper Left = %lu\n", _4btol (data->ulx));
      DBG (DBG_info, "Y-Axis Upper Left = %lu\n", _4btol (data->uly));
      DBG (DBG_info, "Window Width  = %lu\n", _4btol (data->width));
      DBG (DBG_info, "Window Length = %lu\n", _4btol (data->length));
      DBG (DBG_info, "Brightness = %d\n", data->brightness);
      DBG (DBG_info, "Threshold  = %d\n", data->threshold);
      DBG (DBG_info, "Contrast   = %d\n", data->contrast);
      DBG (DBG_info, "Image Composition   = %#0x\n", data->image_composition);
      DBG (DBG_info, "Bits per Pixel = %d\n", data->bpp);
      DBG (DBG_info, "Halftone Code = %#0x\n", data->halftone_code);
      DBG (DBG_info, "Halftone Id   = %#0x\n", data->halftone_id);
      DBG (DBG_info, "Byte29   = %#0x RIF=%d PaddingType=%d\n", data->byte29,
	   data->byte29 & 0x80, data->byte29 & 0x7);
      DBG (DBG_info, "Bit Ordering = %lu\n", _2btol (data->bit_ordering));
      DBG (DBG_info, "Compression Type = %#x\n", data->compression_type);
      DBG (DBG_info, "Compression Arg  = %#x\n", data->compression_arg);
      for (j = 0; j < 6; j++)
	DBG (DBG_info, "Reserved=%#x\n", data->reserved2[j]);
      DBG (DBG_info, "Ignored = %#x\n", data->ignored1);
      DBG (DBG_info, "Ignored = %#x\n", data->ignored2);
      DBG (DBG_info, "Byte42 = %#x MRIF=%d Filtering=%d GammaID=%d\n",
	   data->byte42, data->byte42 & 0x80, data->byte42 & 0x70,
	   data->byte42 & 0x0F);
      DBG (DBG_info, "Ignored = %#x\n", data->ignored3);
      DBG (DBG_info, "Ignored = %#x\n", data->ignored4);
      DBG (DBG_info, "Binary Filtering = %#x\n", data->binary_filtering);
      DBG (DBG_info, "Ignored = %#x\n", data->ignored5);
      DBG (DBG_info, "Ignored = %#x\n", data->ignored6);
      DBG (DBG_info, "Automatic Separation = %#x\n",
	   data->automatic_separation);
      DBG (DBG_info, "Ignored = %#x\n", data->ignored7);
      DBG (DBG_info, "Automatic Binarization = %#x\n",
	   data->automatic_binarization);
      for (j = 0; j < 13; j++)
	DBG (DBG_info, "Ignored = %#x\n", data->ignored8[j]);

      for (k = 0; k < 8; k++)
	{
	  ws = &data->sec[k];
	  DBG (DBG_info, "\n\n");
	  DBG (DBG_info, "SECTION %d\n", k);
	  DBG (DBG_info, "Section Enable Flat (sef bit) = %#x\n", ws->sef);
	  DBG (DBG_info, "ignored = %d\n", ws->ignored0);
	  DBG (DBG_info, "Upper Left X = %lu\n", _4btol (ws->ulx));
	  DBG (DBG_info, "Upper Left Y = %lu\n", _4btol (ws->uly));
	  DBG (DBG_info, "Width = %lu\n", _4btol (ws->width));
	  DBG (DBG_info, "Length = %lu\n", _4btol (ws->length));
	  DBG (DBG_info, "Binary Filtering = %#x\n", ws->binary_filtering);
	  DBG (DBG_info, "ignored = %d\n", ws->ignored1);
	  DBG (DBG_info, "Threshold = %#x\n", ws->threshold);
	  DBG (DBG_info, "ignored = %d\n", ws->ignored2);
	  DBG (DBG_info, "Image Composition = %#x\n", ws->image_composition);
	  DBG (DBG_info, "Halftone Id = %#x\n", ws->halftone_id);
	  DBG (DBG_info, "Halftone Code = %#x\n", ws->halftone_code);
	  for (j = 0; j < 7; j++)
	    DBG (DBG_info, "ignored = %d\n", ws->ignored3[j]);
	}
    }
  DBG (DBG_proc, "<< print_window_data\n");
}

static SANE_Status
read_data (int fd, void *buf, size_t * buf_size, SANE_Byte dtc, u_long dtq)
{
  static struct scsi_rs_scanner_cmd cmd;
  SANE_Status status;
  DBG (DBG_proc, ">> read_data buf_size=%lu dtc=0x%2.2x dtq=%lu\n",
       (unsigned long) *buf_size, (int) dtc, dtq);
  if (fd < 0)
    {
      DBG (DBG_error, "read_data: scanner is closed!\n");
      return SANE_STATUS_INVAL;
    }

  memset (&cmd, 0, sizeof (cmd));	/* CLEAR */
  cmd.opcode = HS2P_SCSI_READ_DATA;
  cmd.dtc = dtc;
  _lto2b (dtq, cmd.dtq);
  _lto3b (*buf_size, cmd.len);

  DBG (DBG_info, "read_data ready to send scsi cmd\n");
  DBG (DBG_info, "opcode=0x%2.2x, dtc=0x%2.2x, dtq=%lu, transfer len =%d\n",
       cmd.opcode, cmd.dtc, _2btol (cmd.dtq), _3btol (cmd.len));

  status = sanei_scsi_cmd (fd, &cmd, sizeof (cmd), buf, buf_size);

  if (status != SANE_STATUS_GOOD)
    DBG (DBG_error, "read_data: %s\n", sane_strstatus (status));
  DBG (DBG_proc, "<< read_data %lu\n", (unsigned long) *buf_size);
  return (status);
}

#if 0
static SANE_Status
send_data (int fd, void *buf, size_t * buf_size)
{
  static struct scsi_rs_scanner_cmd cmd;
  SANE_Status status;
  DBG (DBG_proc, ">> send_data %lu\n", (unsigned long) *buf_size);

  memset (&cmd, 0, sizeof (cmd));	/* CLEAR */
  memcpy (&cmd, buf, sizeof (*buf));	/* Fill in our struct with set values */
  cmd.opcode = HS2P_SCSI_SEND_DATA;
  _lto3b (*buf_size, cmd.len);
  status = sanei_scsi_cmd (fd, &cmd, sizeof (cmd), buf, buf_size);

  DBG (DBG_proc, "<< send_data %lu\n", (unsigned long) *buf_size);
  return (status);
}
#endif

static SANE_Bool
is_valid_endorser_character (char c)
{
  int i = (int) c;
  /* 44 characters can be printed by endorser */

  if (i >= 0x30 && i <= 0x3A)
    return SANE_TRUE;		/* 0123456789: */
  if (i == 0x23)
    return SANE_TRUE;		/* # */
  if (i == 0x27)
    return SANE_TRUE;		/* ` */
  if (i >= 0x2C && i <= 0x2F)
    return SANE_TRUE;		/* '-./ */
  if (i == 0x20)
    return SANE_TRUE;		/* space */
  if (i >= 0x41 && i <= 0x5A)
    return SANE_TRUE;		/* ABCDEFGHIJKLMNOPQRSTUVWXYZ <spaces> */
  if (i >= 0x61 && i <= 0x7A)
    return SANE_TRUE;		/* abcdefghijklmnopqrstuvwxyz <spaces> */

  return SANE_FALSE;
}

static SANE_Status
set_endorser_string (int fd, SANE_String s)
{
  struct
  {
    struct scsi_rs_scanner_cmd cmd;
    SANE_Byte endorser[19];
  } out;
  char *t;
  int i, len;

  SANE_Status status;
  DBG (DBG_proc, ">> set_endorser_string %s\n", s);

  for (i = 0, t = s; *t != '\0' && i < 19; i++)
    {
      DBG (DBG_info, "CHAR=%c\n", *t);
      if (!is_valid_endorser_character (*t++))
	return SANE_STATUS_INVAL;
    }
  len = strlen (s);

  memset (&out, 0, sizeof (out));	/* CLEAR            */
  out.cmd.opcode = HS2P_SCSI_SEND_DATA;	/* 2AH              */
  out.cmd.dtc = 0x80;		/* Endorser Data    */
  _lto3b (len, &out.cmd.len[0]);	/* 19 bytes max     */
  memset (&out.endorser[0], ' ', 19);	/* fill with spaces */
  memcpy (&out.endorser[0], s, len);

  status = sanei_scsi_cmd (fd, &out, sizeof (out), NULL, NULL);


  DBG (DBG_proc, "<< set_endorser_string s=\"%s\" len=%d\n", s, len);
  return (status);
}

static SANE_Status
hs2p_send_gamma (HS2P_Scanner * s)
{
  SANE_Status status;
  struct
  {
    struct scsi_rs_scanner_cmd cmd;
    SANE_Byte gamma[2 + GAMMA_LENGTH];
  } out;
  int i;
  size_t len = sizeof (out.gamma);

  DBG (DBG_proc, ">> teco_send_gamma\n");

  memset (&out, 0, sizeof (out));	/* CLEAR               */
  out.cmd.opcode = HS2P_SCSI_SEND_DATA;	/* 2AH                 */
  out.cmd.dtc = 0x03;		/* Gamma Function Data */
  _lto3b (len, &out.cmd.len[0]);	/* 19 bytes max        */
  out.gamma[0] = 0x08;		/* Gamma ID for Download table      */
  out.gamma[1] = 0x08;		/* The Number of gray scale (M) = 8 */
  for (i = 0; i < GAMMA_LENGTH; i++)
    {
      out.gamma[i + 2] = s->gamma_table[i];
    }
  status = sanei_scsi_cmd (s->fd, &out, sizeof (out), NULL, NULL);

  DBG (DBG_proc, "<< teco_send_gamma\n");
  return (status);
}

#if 0
static SANE_Status
clear_maintenance_data (int fd, int code, char XorY, int number)
{
  struct
  {
    struct scsi_rs_scanner_cmd cmd;
    char string[20];
  } out;

  SANE_Status status;
  DBG (DBG_proc, ">> set_maintenance data\n");

  memset (&out, 0, sizeof (out));	/* CLEAR */
  out.cmd.opcode = HS2P_SCSI_SEND_DATA;	/* 2AH   */
  out.cmd.dtc = 0x85;		/* Maintenance Data */
  _lto3b (20, out.cmd.len);	/* 20 bytes */
  switch (code)
    {
    case 1:
      strcpy (out.string, "EEPROM ALL ALL RESET");
      break;
    case 2:
      strcpy (out.string, "EEPROM ALL RESET");
      break;
    case 3:
      strcpy (out.string, "ADF RESET");
      break;
    case 4:
      strcpy (out.string, "FLATBED RESET");
      break;
    case 5:
      strcpy (out.string, "LAMP RESET");
      break;
    case 6:
      sprintf (out.string, "EEPROM ADF %c %+4.1d", XorY, number);
      break;
    case 7:
      sprintf (out.string, "EEPROM BOOK %c %4.1d", XorY, number);
      break;
    case 8:
      sprintf (out.string, "WHITE ADJUST DATA %3d", number);
      break;
    case 9:
      strcpy (out.string, "EEPROM FIRST WHITE ODD");
      break;
    case 10:
      strcpy (out.string, "EEPROM FIRST WHITE EVEN");
      break;
    case 11:
      strcpy (out.string, "R ADF RESET");
      break;
    case 12:
      strcpy (out.string, "R LAMP RESET");
      break;
    case 13:
      sprintf (out.string, "EEPROM R ADF %c %4.1d", XorY, number);
      break;
    case 14:
      strcpy (out.string, "ENDORSER RESET");
      break;
    }
  status = sanei_scsi_cmd (fd, &out, sizeof (out), NULL, NULL);

  DBG (DBG_proc, "<< set_maintenance data\n");
  return (status);
}
#endif

#if 0
static SANE_Status
read_halftone_mask (int fd, SANE_Byte halftone_id, void *buf,
		    size_t * buf_size)
{
  static struct scsi_rs_scanner_cmd cmd;
  SANE_Status status;
  SANE_Int len;
  DBG (DBG_proc, ">> read_halftone_mask\n");

  memset (&cmd, 0, sizeof (cmd));	/* CLEAR */
  cmd.opcode = HS2P_SCSI_READ_DATA;
  cmd.dtc = DATA_TYPE_HALFTONE;
  _lto2b (halftone_id, cmd.dtq);

  /* Each cell of an NxM dither pattern is 1 byte from the set {2,3,4,6,8,16} */
  switch (halftone_id)
    {
    case 0x01:
      len = 32;
      break;			/* 8x4, 45 degree */
    case 0x02:
      len = 36;
      break;			/* 6x6, spiral */
    case 0x03:
      len = 16;
      break;			/* 4x4, spiral */
    case 0x04:
      len = 64;
      break;			/* 8x8, 90 degree */
    case 0x05:
      len = 70;
      break;			/* 70 lines */
    case 0x06:
      len = 95;
      break;			/* 95 lines */
    case 0x07:
      len = 180;
      break;			/* 180 lines */
    case 0x08:
      len = 128;
      break;			/* 16x8, 45 degree */
    case 0x09:
      len = 256;
      break;			/* 16x16, 90 degree */
    case 0x0A:
      len = 64;
      break;			/* 8x8, Bayer */
    default:
      return SANE_STATUS_INVAL;	/* Reserved */
    }

  _lto3b (len, cmd.len);
  status = sanei_scsi_cmd (fd, &cmd, sizeof (cmd), buf, buf_size);

  DBG (DBG_proc, "<< read_halftone_mask\n");
  return (status);
}
#endif

#if 0
static SANE_Status
set_halftone_mask (int fd, SANE_Byte halftone_id, void *buf,
		   size_t * buf_size)
{
  static struct scsi_rs_scanner_cmd cmd;
  SANE_Status status;
  DBG (DBG_proc, ">> set_halftone_mask\n");

  memset (&cmd, 0, sizeof (cmd));	/* CLEAR */
  cmd.opcode = HS2P_SCSI_READ_DATA;
  cmd.dtc = DATA_TYPE_HALFTONE;
  _lto2b (halftone_id, cmd.dtq);

  /* Each cell of an NxM dither pattern is 1 byte from the set {2,3,4,6,8,16} 
   *  0x80, 0x81 are User definable custom dither patterns
   */
  if (halftone_id != 0x80 && halftone_id != 0x81)
    return SANE_STATUS_INVAL;

  _lto3b (*buf_size, cmd.len);
  status = sanei_scsi_cmd (fd, &cmd, sizeof (cmd), buf, buf_size);

  DBG (DBG_proc, "<< set_halftone_mask\n");
  return (status);
}
#endif

#if 0
static SANE_Status
read_gamma_function (int fd)
{
  SANE_Status status = SANE_STATUS_GOOD;
  return (status);
}

static SANE_Status
read_endorser_data (int fd)
{
  SANE_Status status = SANE_STATUS_GOOD;
  return (status);
}

static SANE_Status
read_size_data (int fd)
{
  SANE_Status status = SANE_STATUS_GOOD;
  return (status);
}
#endif

#if 0
static SANE_Status
read_maintenance_data (int fd)
{
  SANE_Status status = SANE_STATUS_GOOD;
  return (status);
}
#endif

/* Bit0: is 0 if document on ADF; else 1
 * Bit1: is 0 if ADF cover is closed; else 1
 * Bit2: reserved
 * Bits7-3: reserved
*/


#if 0
static SANE_Status
read_adf_status (int fd, SANE_Byte * adf_status_byte)
{
  SANE_Status status = SANE_STATUS_GOOD;
  struct scsi_rs_scanner_cmd cmd;
  static size_t len = 1;

  DBG (DBG_proc, ">> read_adf_status\n");

  memset (&cmd, 0, sizeof (cmd));
  cmd.opcode = HS2P_SCSI_READ_DATA;
  cmd.dtc = DATA_TYPE_ADF_STATUS;
  _lto3b (0x01, cmd.len);	/* convert 0x01 into 3-byte Transfer Length */
  if ((status =
       sanei_scsi_cmd (fd, &cmd, sizeof (cmd), adf_status_byte,
		       &len)) != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "read_adf_status ERROR: %s\n", sane_strstatus (status));
    }
  DBG (DBG_proc, "<< read_adf_status\n");
  return (status);
}
#endif

/*
 * read_ipu_photoletter_parameters
 * read_ipu_threshold_parameters
 * read_sensor_data (WHAT DATA TYPE CODE?)
*/
/* SEND CMD */
/*
 * send_halftone_mask
 * send_gamma_function
 * send_endorser_data
 * send_maintenance_data
 *     EPROM All Clear
 *     EPROM Counter Clear
 *     ADF Counter Clear
 *     Flatbed Counter Clear
 *     Lamp Counter Clear
 *     ADF Register Data
 *     Flatbed Register Data
 *     White Adjustment Data
 *     White level first Data (ODD)
 *     White level first Data (EVEN)
 *     Reverse side ADF Counter Clear
 *     Reverse side Lamp Counter Clear
 *     Reverse side ADF Register Data
 *     Endorser Character Counter Clear
 * send_ipu_parameters
*/

/* OBJECT POSITION        */
/* GET DATA BUFFER STATUS */

/* 1-3-4 MODE SELECT */

/* 1-3-5 Reserve Unit: 0x16 
 * 1-3-6 Release Unit: 0x17
*/
static SANE_Status
unit_cmd (int fd, SANE_Byte opcode)
{
  static struct
  {
    SANE_Byte opcode;		/* 16H: Reserve Unit    17H: Release Unit                     */
    SANE_Byte byte1;		/* 7-5: LUN; 4: 3rd Party; 3-1: 3rd Party Device; 0: Reserved */
    SANE_Byte reserved[3];
    SANE_Byte control;		/* 7-6: Vendor Unique; 5-2: Reserved; 1: Flag; 0: Link        */
  } cmd;

  SANE_Byte LUN = (0x00 & 0x07) << 5;
  SANE_Status status;
  DBG (DBG_proc, ">> unit_cmd\n");

  cmd.opcode = opcode;
  cmd.byte1 = LUN & 0xE1;	/* Mask=11100001 3rd Party and 3rd Party Device must be 0 */
  memset (&cmd, 0, sizeof (cmd));
  status = sanei_scsi_cmd (fd, &cmd, sizeof (cmd), 0, 0);

  DBG (DBG_proc, "<< unit_cmd\n");
  return (status);

}

/* The OBJECT POSITION command is used for carriage control or
  * document feed and eject with ADF
  *
  * Position Function: Byte1 bits2-0
  *  000 Unload instructs document eject
  *  001 Load   instructs document feed to scan start position
  *  010 Absolute Positioning - instructs carriage to move to carriage lock position
  *      The carriage moves in the Y-axis direction as the amount set in Count when
  *      count>0
  *      (Not supported in IS420)
  *
*/
static SANE_Status
object_position (int fd, int load)
{
  static struct scsi_object_position_cmd cmd;
  SANE_Status status;
  DBG (DBG_proc, ">> object_position\n");

  /* byte    0 opcode
   * byte    1 position function
   * bytes 2-4 reserved
   * bytes 5-8 reserved
   * byte    9 control
   */


  memset (&cmd, 0, sizeof (cmd));
  cmd.opcode = HS2P_SCSI_OBJECT_POSITION;
  if (load)
    cmd.position_func = OBJECT_POSITION_LOAD;
  else
    cmd.position_func = OBJECT_POSITION_UNLOAD;


  status = sanei_scsi_cmd (fd, &cmd, sizeof (cmd), 0, 0);

  DBG (DBG_proc, "<< object_position\n");
  return (status);
}

static SANE_Status
get_data_status (int fd, STATUS_DATA * dbs)
{
  static GET_DBS_CMD cmd;
  static STATUS_BUFFER buf;	/* hdr + data */
  size_t bufsize = sizeof (buf);
  SANE_Status status;
  DBG (DBG_proc, ">> get_data_status %lu\n", (unsigned long) bufsize);

  /* Set up GET DATA BUFFER STATUS cmd */
  memset (&cmd, 0, sizeof (cmd));	/* CLEAR cmd */
  cmd.opcode = HS2P_SCSI_GET_BUFFER_STATUS;
  cmd.wait &= ~0x01;		/* unset Wait bit0 */
  _lto2b (bufsize, cmd.len);

  /* Now execute cmd, and put returned results in buf */
  status = sanei_scsi_cmd (fd, &cmd, sizeof (cmd), &buf, &bufsize);

  /* Now copy from buf.data to dbs */
  memcpy (dbs, &buf.data, sizeof (*dbs));

  if (status == SANE_STATUS_GOOD &&
      ((unsigned int) _3btol (buf.hdr.len) <= sizeof (*dbs)
       || _3btol (buf.data.filled) == 0))
    {
      DBG (DBG_info, "get_data_status: busy\n");
      status = SANE_STATUS_DEVICE_BUSY;
    }
  DBG (DBG_proc, "<< get_data_status %lu\n", (unsigned long) bufsize);
  return (status);
}

/* 1-3-7 MODE SENSE */
/* 1-3-8 SCAN       */

/* 1-3-9 Receive Diagnostic
 * Byte0: 1CH
 * Byte1: 7-5 LUN; 4-0: reserved
 * Byte2: Reserved
 * Byte3-4: Allocation Length
 * Byte5: 7-6: Vendor Unique; 5-2: Reserved; 1: Flag; 0: Link
 *
 * This command is treated as a dummy command
 * Return GOOD unless there is an error in command in which case it returns CHECK
*/

/*
*  The IS450 performs 7 self-diagnostics tests
*  1) Home position error check
*  2) Exposure lamp error check
*  3) White level error check
*  4) Document table error check
*  5) SCU error check
*  6) RCU error check
*  7) Memory error check
*
*  and uses the lights on the scanner to indicate the result
*
*                               PowerOn    MachineBusy  DocumentInPlace   Error
*                               (green)    (green)      (green)           (red)
*
*  SCU error check              Blinking                                  Blinking
*  RCU error check              Blinking                On                Blinking
*  Home position error check    Blinking   Blinking     Blinking          On
*  Exposure lamp error check    Blinking   Blinking     On                On
*  White level error check      Blinking   Blinking
*  Memory Error (Simplex)       Blinking
*  Memory Error (Duplex)                   Blinking
*
*/
#if 0
static SANE_Status
receive_diagnostic (int fd)
{
  static SANE_Byte cmd[6];
  SANE_Status status;
  DBG (DBG_proc, ">> receive_diagnostic\n");

  cmd[0] = HS2P_SCSI_RECEIVE_DIAGNOSTICS;
  memset (cmd, 0, sizeof (cmd));
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (DBG_proc, "<< receive_diagnostic\n");
  return (status);
}
#endif

/* 1-3-10 Send Diagnostic
 * Byte0: 1DH
 * Byte1: 7-5 LUN; 4: PF; 3: Reserved; 2: S-Test; 1: DevOfl; 0: U-Ofl
 * Byte2: Reserved
 * Byte3-4: Parameter List Length
 * Byte5: 7-6: Vendor Unique; 5-2: Reserved; 1: Flag; 0: Link
 * This command executes self-diagnostic and optical-adjustment
 * PF, DevOfl, and Parameter List Length must be 0 or CHECK condition is returned.
*/
#if 0
static SANE_Status
send_diagnostic (int fd)
{
  static SANE_Byte cmd[6];
  SANE_Status status;
  DBG (DBG_proc, ">> send_diagnostic\n");

  cmd[0] = HS2P_SCSI_SEND_DIAGNOSTICS;
  cmd[1] = 0x00 & (1 << 2) & 0xED;	/* Set Self-Test bit  and clear PF, DevOfl bits */
  cmd[3] = 0x00;
  cmd[4] = 0x00;		/* Parameter list (bytes3-4) must  be 0x00 */
  memset (cmd, 0, sizeof (cmd));
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (DBG_proc, "<< send_diagnostic\n");
  return (status);
}
#endif


/* 1-3-8 SCAN command is used to instruct scanner to start scanning */
static SANE_Status
trigger_scan (HS2P_Scanner * s)
{
  static struct
  {
    START_SCAN cmd;
    SANE_Byte wid[2];		/* scanner supports up to 2 windows */
  } scan;
  SANE_Status status;
  DBG (DBG_proc, ">> trigger scan\n");

  memset (&scan, 0, sizeof (scan));	/* CLEAR scan */
  scan.cmd.opcode = HS2P_SCSI_START_SCAN;
  /* Transfer length is the byte length of Window List transferred
   * Window List is a list of Window Identifier created by SET WINDOW command
   * Since only 1 Window is supported by SCAN command, 0 or 1 is used for Window Identifier
   * and 1 or 2 for length 
   status = sanei_scsi_cmd (s->fd, &trigger, sizeof (trigger), &window_id_list[0], &wl_size);
   */
  scan.cmd.len = (s->val[OPT_DUPLEX].w == SANE_TRUE) ? 2 : 1;

  DBG (DBG_info, "trigger_scan: sending %d Window Id to scanner\n",
       scan.cmd.len);
  status =
    sanei_scsi_cmd (s->fd, &scan, sizeof (scan.cmd) + scan.cmd.len, NULL,
		    NULL);

  DBG (DBG_proc, "<< trigger scan\n");
  return (status);
}

#define MAX_WAITING_TIME       15
static SANE_Status
hs2p_wait_ready (HS2P_Scanner * s)
{
  STATUS_DATA dbs;		/* Status Buffer Status DATA */
  time_t now, start;
  SANE_Status status;

  start = time (NULL);

  while (1)
    {
      status = get_data_status (s->fd, &dbs);

      switch (status)
	{
	default:
	  /* Ignore errors while waiting for scanner to become ready.
	     Some SCSI drivers return EIO while the scanner is
	     returning to the home position.  */
	  DBG (DBG_error, "scsi_wait_ready: get datat status failed (%s)\n",
	       sane_strstatus (status));
	  /* fall through */
	case SANE_STATUS_DEVICE_BUSY:
	  now = time (NULL);
	  if (now - start >= MAX_WAITING_TIME)
	    {
	      DBG (DBG_error,
		   "hs2p_wait_ready: timed out after %lu seconds\n",
		   (u_long) (now - start));
	      return SANE_STATUS_INVAL;
	    }
	  break;

	case SANE_STATUS_GOOD:
	  DBG (DBG_proc, "hs2p_wait_ready: %d bytes ready\n",
	       _3btol (dbs.filled));
	  return status;
	  break;
	}
      usleep (1000000);		/* retry after 100ms */
    }
  return SANE_STATUS_INVAL;
}


/* MODE PAGES GET/SET */
static SANE_Status
connection_parameters (int fd, MP_CXN * settings, SANE_Bool flag)
{
  SANE_Status status;
  MP_CXN buf;
  size_t nbytes;
  DBG (DBG_proc, ">> connection_parameters\n");
  nbytes = sizeof (buf);

  if (flag)
    {				/* GET */
      DBG (DBG_info, ">> GET connection_parameters >> calling mode_sense\n");
      status =
	mode_sense (fd, (MP *) & buf, (SANE_Byte) PAGE_CODE_CONNECTION);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "get_connection_parameters: MODE_SELECT failed with status=%d\n",
	       status);
	  return (status);
	}
      memcpy (settings, &buf, nbytes);
    }
  else
    {				/* SET */
      DBG (DBG_info, ">> SET connection_parameters >> calling mode_select\n");
      /* Fill in struct then hand off to mode_select */
      memset (&buf, 0, sizeof (buf));	/* Fill struct with zeros */
      memcpy (&buf, settings, nbytes);
      /* Make sure calling function didn't change these bytes           */
      memset (&buf.hdr, 0, sizeof (buf.hdr));	/* Make sure 4bytes are 0 */
      buf.code = PAGE_CODE_CONNECTION;	/* bits5-0: Page Code 02H */
      buf.code &= ~(1 << 7);	/* Bit7 PS is set to 0    */
      buf.len = 0x0E;		/* This is the only page with 14 bytes */

      status = mode_select (fd, (MP *) & buf);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "set_connection_parameters: MODE_SELECT failed with status=%d\n",
	       status);
	  return (-1);
	}
    }
  DBG (DBG_proc, "<< connection_parameters\n");
  return (status);
}

static SANE_Status
get_basic_measurement_unit (int fd, SANE_Int * bmu, SANE_Int * mud)
{
  SANE_Status status;
  MP_SMU buf;

  DBG (DBG_proc, ">> get_basic_measurement_unit: fd=\"%d\"\n", fd);

  status =
    mode_sense (fd, (MP *) & buf,
		(SANE_Byte) PAGE_CODE_SCANNING_MEASUREMENTS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "set_basic_measurement_unit: MODE_SELECT failed with status=%d\n",
	   status);
      return (SANE_STATUS_INVAL);
    }
  *bmu = buf.bmu;
  *mud = ((buf.mud[0] << 8) | buf.mud[1]);

  DBG (DBG_proc, "<< get_basic_measurement_unit: bmu=%d mud=%d\n", *bmu,
       *mud);
  return (status);
}

static SANE_Status
set_basic_measurement_unit (int fd, SANE_Byte bmu)
{
  MP_SMU buf;			/* Mode Page Scanning Measurements Page Code */
  SANE_Status status;
  SANE_Int mud;
  size_t bufsize = sizeof (buf);

  DBG (DBG_proc, ">> set_basic_measurement_unit: %d\n", bmu);

  /* Set up buf */
  memset (&buf, 0, bufsize);	/* CLEAR buf            */
  buf.code = PAGE_CODE_SCANNING_MEASUREMENTS;	/* bits5-0: Page Code   */
  buf.code &= ~(1 << 7);	/* Bit7 PS is set to 0  */
  buf.len = 0x06;

  buf.bmu = bmu;		/* Power on default is POINTS */
  mud = (bmu == INCHES) ? DEFAULT_MUD : 1;
  DBG (DBG_info, "SET_BASIC_MEASUREMENT_UNIT: bmu=%d mud=%d\n", bmu, mud);
  _lto2b (mud, &buf.mud[0]);	/* buf.mud[0]  = (mud >> 8) & 0xff; buf.mud[1]  = (mud & 0xff); */

  status = mode_select (fd, (MP *) & buf);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "set_basic_measurement_unit: MODE_SELECT failed with status=%d\n",
	   status);
      status = SANE_STATUS_INVAL;
    }

  DBG (DBG_proc,
       "<< set_basic_measurement_unit: opcode=%d len=%d bmu=%d mud=%ld\n",
       buf.code, buf.len, buf.bmu, _2btol (&buf.mud[0]));
  return (status);
}

static SANE_Status
adf_control (int fd, SANE_Bool flag, SANE_Byte * adf_control,
	     SANE_Byte * adf_mode, SANE_Byte * mwt)
{
  SANE_Status status;
  MP_ADF buf;
  size_t bufsize = sizeof (buf);

  DBG (DBG_proc, ">> adf_control\n");

  memset (&buf, 0, bufsize);	/* Fill struct with zeros  */

  if (flag)
    {				/* GET */
      DBG (DBG_info, ">> GET ADF_control>> calling mode_sense\n");
      status =
	mode_sense (fd, (MP *) & buf, (SANE_Byte) PAGE_CODE_ADF_CONTROL);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "get_adf_control: MODE_SELECT failed\n");
	  return (status);
	}
      *adf_control = buf.adf_control;
      *adf_mode = buf.adf_mode_control;
      *mwt = buf.medium_wait_timer;
    }
  else
    {				/* SET */
      /* Fill in struct then hand off to mode_select                  */
      buf.code = PAGE_CODE_ADF_CONTROL;	/* bits5-0: Page Code      */
      buf.code &= ~(1 << 7);	/* Bit7 PS is set to 0     */
      buf.len = 0x06;
      /* Byte2: adf_control:      7-2:reserved; 1-0:adf_control: Default 00H Flatbed, 01H Simplex, 02H Duplex */
      /* Byte3: adf_mode_control: 7-3:reserved; 2: Prefeed Mode: 0 invalid, 1 valid; 1-0: ignored */
      /* Byte4: medium_wait_timer: timeout period. Not supported */
      buf.adf_control = (*adf_control & 0x03);
      buf.adf_mode_control = (*adf_mode & 0x04);
      buf.medium_wait_timer = *mwt;
      status = mode_select (fd, (MP *) & buf);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "set_adf_control: MODE_SELECT failed with status=%d\n",
	       status);
	  return (status);
	}
    }
  DBG (DBG_proc, ">> adf_control\n");
  return (status);
}

static SANE_Status
white_balance (int fd, int *val, SANE_Bool flag)
{
  SANE_Status status;
  MP_WhiteBal buf;		/* White Balance Page Code */
  size_t bufsize = sizeof (buf);

  memset (&buf, 0, bufsize);

  if (flag)
    {				/* GET */
      DBG (DBG_proc, ">> GET white_balance>> calling mode_sense\n");
      status =
	mode_sense (fd, (MP *) & buf, (SANE_Byte) PAGE_CODE_WHITE_BALANCE);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "get_white_balance: MODE_SELECT failed with status=%d\n",
	       status);
	  return (status);
	}
      *val = buf.white_balance;
    }
  else
    {				/* SET */
      /* Fill in struct then hand off to mode_select */
      memset (&buf, 0, sizeof (buf));	/* Fill struct with zeros */
      buf.code = PAGE_CODE_WHITE_BALANCE;	/* bits5-0: Page Code     */
      buf.code &= ~(1 << 7);	/* Bit7 PS is set to 0    */
      buf.len = 0x06;
      buf.white_balance = *val;	/* Power on default is RELATIVE_WHITE */
      status = mode_select (fd, (MP *) & buf);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "set_white_balance: MODE_SELECT failed with status=%d\n",
	       status);
	  return (status);
	}
    }
  DBG (DBG_proc, "<< white balance: buf.white_balance=%#02x\n",
       buf.white_balance);
  return (status);
}

#if 0
static SANE_Int
lamp_timer (int fd, int val, SANE_Bool flag)
{
  SANE_Status status;
  MP_LampTimer buf;		/* Lamp Timer Page Code */

  DBG (DBG_proc, ">> lamp timer\n");
  if (flag)
    {				/* GET */
      DBG (DBG_info, ">> GET lamp_timer>> calling mode_sense\n");
      if ((status =
	   mode_sense (fd, (MP *) & buf,
		       (SANE_Byte) PAGE_CODE_LAMP_TIMER_SET)) !=
	  SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "get_lamp_timer: MODE_SELECT failed\n");
	  return (-1);
	}
    }
  else
    {				/* SET */
      /* Fill in struct then hand off to mode_select */
      memset (&buf, 0, sizeof (buf));	/* Fill struct with zeros */
      buf.code = PAGE_CODE_LAMP_TIMER_SET;	/* bits5-0: Page Code     */
      buf.code &= ~(1 << 7);	/* Bit7 PS is set to 0    */
      buf.len = 0x06;
      buf.time_on = val;	/* time lamp has been on  */
      if ((status = mode_select (fd, (MP *) & buf)) != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "set_lamp_timer: MODE_SELECT failed with status=%d\n", status);
	  return (-1);
	}
    }
  DBG (DBG_proc, "<< lamp timer\n");
  return (buf.time_on);
}
#endif

static SANE_Status
endorser_control (int fd, int *val, SANE_Bool flag)
{
  SANE_Status status;
  MP_EndCtrl buf;		/* MPHdr (4bytes) + MPP (8bytes)       */
  SANE_Byte mask = 0x7;		/* 7-3:reserved; 2-0: Endorser Control */
  size_t bufsize = sizeof (buf);

  DBG (DBG_proc, ">> endorser_control: fd=%d val=%d flag=%d\n", fd, *val,
       flag);

  memset (&buf, 0, bufsize);	/* Fill struct with zeros  */

  if (flag)
    {				/* GET */
      DBG (DBG_info, ">> GET endorser control >> calling mode_sense\n");
      status =
	mode_sense (fd, (MP *) & buf, (SANE_Byte) PAGE_CODE_ENDORSER_CONTROL);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "get_endorser_control: MODE_SELECT failed with status=%d\n",
	       status);
	  return (status);
	}
      *val = buf.endorser_control & mask;
    }
  else
    {				/* SET */
      DBG (DBG_info, ">> SET endorser control >> calling mode_select\n");
      /* Fill in struct then hand off to mode_select */
      memset (&buf, 0, sizeof (buf));	/* Fill struct with zeros */
      buf.code = PAGE_CODE_ENDORSER_CONTROL;	/* bits5-0: Page Code     */
      buf.code &= ~(1 << 7);	/* Bit7 PS is set to 0    */
      buf.len = 0x06;

      buf.endorser_control = *val & mask;	/* Power on default is OFF */
      status = mode_select (fd, (MP *) & buf);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "set_endorser_control: MODE_SELECT failed with status=%d\n",
	       status);
	  return (status);
	}
    }
  DBG (DBG_proc, "<< endorser_control: endorser_control=%#02x\n",
       buf.endorser_control);
  return (status);
}

/* When SCAN, READ, or LOAD (in ADF mode) is issued, scanner waits until operator panel start button is pressed */
static SANE_Status
scan_wait_mode (int fd, int val, SANE_Bool flag)
{
  SANE_Status status;
  MP_SWM buf;			/* Scan Wait Mode Page Code */
  DBG (DBG_proc, ">> scan_wait_mode\n");

  if (flag)
    {				/* GET */
      DBG (DBG_info, ">> GET scan_wait_mode >> calling mode_sense\n");
      status =
	mode_sense (fd, (MP *) & buf, (SANE_Byte) PAGE_CODE_SCAN_WAIT_MODE);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "get_scan_wait_mode: MODE_SELECT failed with status=%d\n",
	       status);
	  return (-1);
	}
    }
  else
    {				/* SET */
      /* Fill in struct then hand off to mode_select */
      memset (&buf, 0, sizeof (buf));	/* Fill struct with zeros */
      buf.code = PAGE_CODE_SCAN_WAIT_MODE;	/* bits5-0: Page Code     */
      buf.code &= ~(1 << 7);	/* Bit7 PS is set to 0    */
      buf.len = 0x06;
      buf.swm = 0x00;
      if (val == 1)
	buf.swm |= 1;		/* set bit 1   if scan_wait_mode ON  */
      else
	buf.swm &= ~1;		/* unset bit 1 if scan_wait_mode OFF */

      DBG (DBG_info, ">> SET scan_wait_mode >> calling mode_sense\n");
      if ((status = mode_select (fd, (MP *) & buf)) != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "mode_select ERROR %s\n", sane_strstatus (status));
	}
    }
  DBG (DBG_proc, "<< scan_wait_mode: buf.swm=%#02x\n", buf.swm);
  return (status);
}

/* Selectable when Send Diagnostics command is performed */
static SANE_Int
service_mode (int fd, int val, SANE_Bool flag)
{
  SANE_Status status;
  MP_SRV buf;			/* Service Mode Page Code */
  DBG (DBG_proc, ">> service_mode\n");

  if (flag)
    {				/* GET */
      DBG (DBG_info, ">> GET service_mode >> calling mode_sense\n");
      status =
	mode_sense (fd, (MP *) & buf,
		    (SANE_Byte) PAGE_CODE_SERVICE_MODE_SELECT);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "get_service_mode: MODE_SELECT failed with status=%d\n",
	       status);
	  return (-1);
	}
    }
  else
    {				/* SET */
      /* Fill in struct then hand off to mode_select */
      memset (&buf, 0, sizeof (buf));	/* Fill struct with zeros */
      buf.code = PAGE_CODE_SERVICE_MODE_SELECT;	/* bits5-0: Page Code     */
      buf.code &= ~(1 << 7);	/* Bit7 PS is set to 0    */
      buf.len = 0x06;
      /* 0H: Self-Diagnostics Mode, 1H: Optical Adjustment Mode */
      buf.service = val & 0x01;
      status = mode_select (fd, (MP *) & buf);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "set_service_mode: MODE_SELECT failed with status=%d\n",
	       status);
	  return (-1);
	}
    }
  DBG (DBG_proc, "<< service_mode\n");
  return (buf.service & 0x01);
}
