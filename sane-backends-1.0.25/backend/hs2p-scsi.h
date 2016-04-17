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
#include "../include/sane/sane.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_scsi.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_thread.h"

/* 1-2 SCSI STATUS BYTE KEYS                      */
#define HS2P_SCSI_STATUS_GOOD			0x00
#define HS2P_SCSI_STATUS_CHECK			0x02
#define HS2P_SCSI_STATUS_BUSY			0x08
#define HS2P_SCSI_STATUS_RESERVATION CONFLICT	0x18
/* All other status byte keys are reserved        */

/*
 * SCSI Command List for Command Descriptor Block 
 * All reserved bit and fields in the CDB must be zero
 * Values in the CDB described as "Reserved" must no be specified
 * The FLAG and LINK bits in the CONTROL byte must be zero
 * Any values in the Vendor Unique field are ignored
 * The Logical Unit Number in the CDB must always be zero
 * All Reserved bit and fields in the data fields must be zero
 * Values of parameters in the data fields described as 
 *   "Reserved" or "Not supported" must not be specified
*/

/* 1-3 SCSI COMMANDS				  */
#define HS2P_SCSI_TEST_UNIT_READY       	0x00
#define HS2P_SCSI_REQUEST_SENSE         	0x03
#define HS2P_SCSI_INQUIRY               	0x12
#define HS2P_SCSI_MODE_SELECT           	0x15
#define HS2P_SCSI_RESERVE_UNIT          	0x16
#define HS2P_SCSI_RELEASE_UNIT          	0x17
#define HS2P_SCSI_MODE_SENSE            	0x1a
#define HS2P_SCSI_START_SCAN            	0x1b
#define HS2P_SCSI_RECEIVE_DIAGNOSTICS   	0x1c
#define HS2P_SCSI_SEND_DIAGNOSTICS      	0x1d
#define HS2P_SCSI_SET_WINDOW            	0x24
#define HS2P_SCSI_GET_WINDOW            	0x25
#define HS2P_SCSI_READ_DATA             	0x28
#define HS2P_SCSI_SEND_DATA             	0x2a
#define HS2P_SCSI_OBJECT_POSITION       	0x31
#define HS2P_SCSI_GET_BUFFER_STATUS     	0x34

/* Sense Key Defines                      */
#define HS2P_SK_NO_SENSE		0x00
#define HS2P_SK_RECOVERED_ERROR		0x01
#define HS2P_SK_NOT_READY		0x02
#define HS2P_SK_MEDIUM_ERROR		0x03
#define HS2P_SK_HARDWARE_ERROR		0x04
#define HS2P_SK_ILLEGAL_REQUEST		0x05
#define HS2P_SK_UNIT_ATTENTION		0x06
#define HS2P_SK_DATA_PROJECT		0x07
#define HS2P_SK_BLANK_CHECK		0x08
#define HS2P_SK_VENDOR_UNIQUE		0x09
#define HS2P_SK_COPY_ABORTED		0x0a
#define HS2P_SK_ABORTED_COMMAND		0x0b
#define HS2P_SK_EQUAL			0x0c
#define HS2P_SK_VOLUME_OVERFLOW		0x0d
#define HS2P_SK_MISCOMPARE		0x0e
#define HS2P_SK_RESERVED		0x0f

struct sense_key
{
  int key;
  char *meaning;
  char *description;
};
static struct sense_key sensekey_errmsg[16] = {
  {0x00, "NO SENSE", "Indicates that there is no Sense Key information"},
  {0x01, "RECOVERED ERROR", "Invalid"},
  {0x02, "NOT READY",
   "Indicates that the scanner is not ready, e.g. ADF cover not closed"},
  {0x03, "MEDIUM ERROR", "Error regarding document such as paper jam"},
  {0x04, "HARDWARE ERROR",
   "Error relating to hardware, e.g. CCD line clock error"},
  {0x05, "ILLEGAL REQUEST",
   "Used such as when illegal parameter exists in data or command"},
  {0x06, "UNIT ATTENTION",
   "Used when power on, BUS DEVICE RESET message or hardware reset"},
  {0x07, "DATA PROJECT", "Invalid"},
  {0x08, "BLANK CHECK", "Invalid"},
  {0x09, "VENDOR UNIQUE", "Invalid"},
  {0x0a, "COPY ABORTED", "Invalid"},
  {0x0b, "ABORTED COMMAND", "Used when scanner aborts a command execution"},
  {0x0c, "EQUAL", "Invalid"},
  {0x0d, "VOLUME OVERFLOW", "Invalid"},
  {0x0e, "MISCOMPARE", "Invalid"},
  {0x0f, "RESERVED", "Invalid"}
};

/* When Error_Code = 0x70 more detailed information is available: 
 * code, qualifier, description
*/
struct ASCQ
{				/* ADDITIONAL SENSE CODE QUALIFIER */
  unsigned int codequalifier;
  char *description;
};
static struct ASCQ ascq_errmsg[74] = {
  {0x0000, "No additional sense information"},
  {0x0002, "End of Medium detected"},
  {0x0005, "End of Data detected"},
  {0x0400, "Logical unit not ready. Don't know why."},
  {0x0401, "Logical unit is in process of becoming ready."},
  {0x0403, "Logical unit not ready. Manual intervention required."},
  {0x0500, "Logical unit does not respond to selection."},
  {0x0700, "Multiple peripheral devices selected."},
  {0x1100, "Unrecovered read error."},
  {0x1101, "Read retries exhausted."},
  {0x1501, "Mechanical positioning error."},
  {0x1a00, "Parameter list length error."},
  {0x2000, "Invalid command operation mode."},
  {0x2400, "Invalid field in CDB (check field pointer)."},
  {0x2500, "Logical unit not supported."},
  {0x2600, "Invalid field in parameter list (check field pointer)."},
  {0x2900, "Power on, reset, or BUS DEVICE RESET occurred."},
  {0x2a01, "(MODE parameter changed.)"},
  {0x2c00, "Command sequence error."},
  {0x2c01, "(Too many windows specified."},
  {0x2c02, "(Invalid combination of windows specified."},
  {0x3700, "(Rounded parameter.)"},
  {0x3900, "(Saving parameters not supported.)"},
  {0x3a00, "Medium not present."},
  {0x3b09, "(Read past end of medium.)"},
  {0x3b0b, "(Position past end of medium.)"},
  {0x3d00, "Invalid bits in IDENTIFY message."},
  {0x4300, "Message error."},
  {0x4500, "Select/Reselect failure."},
  {0x4700, "(SCSI parity error)"},
  {0x4800, "Initiator detected error message received."},
  {0x4900, "Invalid message error."},
  {0x4a00, "Command phase error."},
  {0x4b00, "Data phase error."},
  {0x5300, "(Media Load/Eject failed)"},
  {0x6000, "Lamp failure"},
  {0x6001, "(Shading Error)"},
  {0x6002, "White adjustment error"},
  {0x6010, "Reverse Side Lamp Failure"},
  {0x6200, "Scan head positioning error"},
  {0x6300, "Document Waiting Cancel"},
  {0x8000, "(PSU overheat)"},
  {0x8001, "(PSU 24V fuse down)"},
  {0x8002, "(ADF 24V fuse down)"},
  {0x8003, "(5V fuse down)"},
  {0x8004, "(-12V fuse down)"},
  {0x8100, "(ADF 24V power off)"},
  {0x8101, "(Base 12V power off)"},
  {0x8102, "(SCSI 5V power off)"},
  {0x8103, "Lamp cover open (Lamp 24V power off)"},
  {0x8104, "(-12V power off)"},
  {0x8105, "(Endorser 6V power off)"},
  {0x8106, "SCU 3.3V power down error"},
  {0x8107, "RCU 3.3V power down error"},
  {0x8108, "OIPU 3.3V power down error"},
  {0x8200, "Memory Error (Bus error)"},
  {0x8210, "Reverse-side memory error (Bus error)"},
  {0x8300, "(Image data processing LSI error)"},
  {0x8301, "(Interfac LSI error)"},
  {0x8302, "(SCSI controller error)"},
  {0x8303, "(Compression unit error)"},
  {0x8304, "(Marker detect unit error)"},
  {0x8400, "Endorser error"},
  {0x8500, "(Origin Positioning error)"},
  {0x8600, "Mechanical Time Out error (Pick Up Roller error)"},
  {0x8700, "(Heater error)"},
  {0x8800, "(Thermistor error)"},
  {0x8900, "ADF cover open"},
  {0x8901, "(ADF lift up)"},
  {0x8902, "Document jam error for ADF"},
  {0x8903, "Document misfeed for ADF"},
  {0x8a00, "(Interlock open)"},
  {0x8b00, "(Not enough memory)"},
  {0x8c00, "Size detection failed"}
};

typedef struct sense_data
{				/* HS2P_REQUEST_SENSE_DATA  */
  /* bit7:valid is 1 if information byte is valid, 
     bits6:0 error_code */
  SANE_Byte error_code;

  /* not used, set to 0 */
  SANE_Byte segment_number;

  /* bit7 file-mark (unused, set to 0),
     bit6 EOM is 1 if end of document detected before completing scan
     bit5 ILI (incorrect length indicator) is 1 when data length mismatch occurs on READ
     bits3:0 sense_key indicates error conditions.  */
  SANE_Byte sense_key;

  SANE_Byte information[4];

  /* fixed at 6 */
  SANE_Byte sense_length;

  /* not used and set to 0 */
  SANE_Byte command_specific_information[4];
  SANE_Byte sense_code;
  SANE_Byte sense_code_qualifier;
} SENSE_DATA;

/* page codes used with HS2P_SCSI_INQUIRY */
#define HS2P_INQUIRY_STANDARD_PAGE_CODE 0x00
#define HS2P_INQUIRY_VPD_PAGE_CODE      0xC0
#define HS2P_INQUIRY_JIS_PAGE_CODE      0xF0

/*
 * The EVPD and Page Code are used in pair. When the EVPD bit is 0, INQUIRY data 
 * in the standard format is returned to the initiator. When the EVPD bit is 1,
 * the EVPD information specified by each Page Code is returned in each Page Code
 * data format. 
 *
 * EVPD=0x00, Page_Code=0x00      => Standard Data Format
 *
 * EVPD=0x01, PAGE_CODE=0x00      => Return list of supported Page Codes
 * EVPD=0x01, PAGE_CODE=0x01~0x7F => Not Supported
 * EVPD=0x01, PAGE_CODE=0x80~0x82 => Not Supported
 * EVPD=0x01, PAGE_CODE=0x83~0xBF => Reserved
 * EVPD=0x01, PAGE_CODE=0xC0      => RICOH Scanner VPD information
 * EVPD=0x01, PAGE_CODE=0xF0      => JIS Version VPD information
*/
struct inquiry_standard_data
{
  /* bits7-5 peripheral qualifier
   * bits4-0 peripheral device
   * Peripheral Qualifier and Peripheral Devide Type are not supported on logical unit
   * Therefore LUN=0 and this field indicates scanner and is set to 0x06
   * When LUN!=0 this field becomes 0x1F and means undefined data
   */
  SANE_Byte devtype;		/* must be 0x06 */

  /* bit7: repaceable media bit is set to 0 
   * bits6-1: reserved
   * bit0: EVPD
   */
  SANE_Byte rmb_evpd;

  /* bits7-6: ISO Version  is set to 0
   * bits5-3: ECMA Version is set to 0
   * bits2-0: ANSI Version is set to 2
   */
  SANE_Byte version;

  /* bit7: AENC (asynchronous event notification capability) is set to 0
   * bit6: TrmIOP (terminate I/O process) is set to 0
   * bits5-4: reserved
   * bits3-0: Response Data Format is set to 2
   */
  SANE_Byte response_data_format;

  /* Additional Length indicate number of bytes which follows, set to 31
   */
  SANE_Byte length;

  SANE_Byte reserved[2];

  /* bit7: RelAdr (relative addressing) is set to 0
   * bit6: Wbus32 is set to 0
   * bit5: Wbus16 is set to 0
   * bit4: Sync   is set to 0
   * bit3: Linked is set to 0
   * bit2: reserved
   * bit1: CmdQue is set to 0
   * bit0: SftRe  is set to 0
   * Sync is set to 1 with this scanner to support synchronous data transfer
   * When DIPSW2 is on, Sync is set to 0 for asynchronous data transfer
   */
  SANE_Byte byte7;

  SANE_Byte vendor[8];		/* vendor_id="RICOH   " */
  SANE_Byte product[16];	/* product_id="IS450           " */
  SANE_Byte revision[4];	/* product_revision_level="xRxx" where x indicate firmware version number */
};

/* VPD Information [EVPD=0x01, PageCode=0xC0] */
struct inquiry_vpd_data
{
  SANE_Byte devtype;		/* bits7-5: Peripheral Qualifier
				 * bits4-0: Peripheral Device Type */
  SANE_Byte pagecode;		/* Page Code  => 0xC0 */
  SANE_Byte byte2;		/* Reserved */
  SANE_Byte pagelength;		/* Page Length => 12 (0x0C) */
  SANE_Byte adf_id;		/* ADF Identification 
				 * 0: No ADF is mounted
				 * 1: Single sided ADF is mounted
				 * 2: Double sided ADF is mounted
				 * 3: ARDF is mounted. (Reverse double side scanning available)
				 * 4: Reserved 
				 * It should be 1 or 2 with this scanner.
				 */
  SANE_Byte end_id;		/* Endorser Identification 
				 * 0: No endorser
				 * 1: Endorser mounted
				 * 2: Reserved 
				 * It should be 0 or 1 with this scanner
				 */
  SANE_Byte ipu_id;		/* Image Processing Unit Identification 
				 * bits 7:2   Reserved
				 * bit 1    0:Extended board not mounted
				 *          1:Extended board is mounted
				 * bit 0    0:IPU is not mounted
				 *          1:IPU is mounted
				 * It should always be 0 with this scanner
				 */
  SANE_Byte imagecomposition;	/* indicates supported image data type. 
				 * This is set to 0x37 
				 * bit0 => Line art          supported ? 1:0
				 * bit1 => Dither            supported ? 1:0
				 * bit2 => Error Diffusion   supported ? 1:0
				 * bit3 => Color             supported ? 1:0
				 * bit4 => 4bits gray scale  supported ? 1:0
				 * bit5 => 5-8bit gray scale supported ? 1:0
				 * bit6 => 5-8bit gray scale supported ? 1:0
				 * bit7 => Reserved
				 */
  SANE_Byte imagedataprocessing[2];	/* Image Data Processing Method
					 * IPU installed ? 0x18 : 0x00
					 * Byte8  => White Framing   ? 1:0
					 * Byte9  => Black Framing   ? 1:0
					 * Byte10 => Edge Extraction ? 1:0
					 * Byte11 => Noise Removal   ? 1:0
					 * Byte12 => Smoothing       ? 1:0
					 * Byte13 => Line Bolding    ? 0:1
					 * Byte14 => Reserved
					 * Byte15 => Reserved
					 */
  SANE_Byte compression;	/* Compression Method is set to 0x00
				 * bit0 => MH                 supported ? 1:0
				 * bit1 => MR                 supported ? 1:0
				 * bit2 => MMR                supported ? 1:0
				 * bit3 => MH (byte boundary) supported ? 1:0
				 * bit4 => Reserved
				 */
  SANE_Byte markerrecognition;	/* Marker Recognition Method is set to 0x00
				 * bit0    => Marker Recognition supported ? 1:0
				 * bits1-7 => Reserved
				 */
  SANE_Byte sizerecognition;	/* Size Detection 
				 * bit0    => Size Detection Supported ? 1:0
				 * bits1-7 => Reserved
				 */
  SANE_Byte byte13;		/* Reserved */
  SANE_Byte xmaxoutputpixels[2];	/* X Maximum Output Pixel is set to 4960 (0x1360)
					 * indicates maximum number of pixels in the main 
					 * scanning direction that can be output by scanner
					 */

};

struct inquiry_jis_data
{				/* JIS INFORMATION  VPD_IDENTIFIER_F0H */
  SANE_Byte devtype;		/* 7-5: peripheral qualifier, 4-0: peripheral device type */
  SANE_Byte pagecode;
  SANE_Byte jisversion;
  SANE_Byte reserved1;
  SANE_Byte alloclen;		/* page length: Set to 25 (19H)  */
  struct
  {
    SANE_Byte x[2];		/* Basic X Resolution: Set to 400 (01H,90H) */
    SANE_Byte y[2];		/* Basic Y Resolution: Set to 400 (01H,90H) */
  } BasicRes;
  SANE_Byte resolutionstep;	/* 7-4: xstep, 3-0 ystep: Both set to 1 (11H) */
  struct
  {
    SANE_Byte x[2];		/* Maximum X resolution: Set to 800 (03H,20H) */
    SANE_Byte y[2];		/* Maximum Y resolution: Set to 800 (03H,20H) */
  } MaxRes;
  struct
  {
    SANE_Byte x[2];		/* Minimum X resolution: Set to 100 (00H,64H) */
    SANE_Byte y[2];		/* Minimum Y resolution */
  } MinRes;
  SANE_Byte standardres[2];	/* Standard Resolution: bits 7-0:
				 * byte18:  60, 75,100,120,150,160,180, 200
				 * byte19: 240,300,320,400,480,600,800,1200
				 */
  struct
  {
    SANE_Byte width[4];		/* in pixels based on basic resolution. Set to 4787 (12B3H) */
    SANE_Byte length[4];	/* maximum number of scan lines based on basic resolution. Set to 6803 (1A93H) */
  } Window;
  SANE_Byte functions;		/* This is set to 0EH: 0001110
				 * bit0:    data overflow possible
				 * bit1:    line art support
				 * bit2:    dither support
				 * bit3:    gray scale support
				 * bits7-4: reserved
				 */
  SANE_Byte reserved2;
};



#define SMS_SP  0x01		/* Mask for Bit0                                       */
#define SMS_PF  0x10		/* Mask for Bit4                                       */
typedef struct scsi_mode_select_cmd
{
  SANE_Byte opcode;		/* 15H                                                 */
  SANE_Byte byte1;		/* 7-5:LUN; 4:PF; 2:Reserved; 1:SP                     
				 * Save Page Bit must be 0 since pages cannot be saved 
				 * Page Format Bit must be 1                           */
  SANE_Byte reserved[2];
  SANE_Byte len;		/* Parameter List Length                               */
  SANE_Byte control;		/* 7-6:Vendor Unique; 5-2:Reserved; 1:Flag; 0:Link     */
} SELECT;

/* MODE SELECT PARAMETERS:
 * 0-n Mode Parameter Header
 * 0-n mode Block Descriptor (not used)
 * 0-n mode Page
*/
typedef struct scsi_mode_parameter_header
{
  SANE_Byte data_len;		/* Mode Data Length          NOT USED so must be 0 */
  SANE_Byte medium_type;	/* Medium Type               NOT USED so must be 0 */
  SANE_Byte dev_spec;		/* Device Specific Parameter NOT USED so must be 0 */
  SANE_Byte blk_desc_len;	/* Block Descriptor Length             is set to 0 */
} MPHdr;

typedef struct page
{
  SANE_Byte code;		/* 7:PS; 6:Reserved; 5-0:Page Code            */
  SANE_Byte len;		/* set to 14 when MPC=02H and 6 otherwise     */
  SANE_Byte parameter[14];	/* either 14 or 6, so let's allow room for 14 */
} MPP;				/* Mode Page Parameters */
typedef struct mode_pages
{
  MPHdr hdr;			/* Mode Page Header      */
  MPP page;			/* Mode Page Parameters  */
} MP;
					     /* MODE PAGE CODES  (MPC)                    */
					     /* 00H Reserved (Vendor Unique)              */
					     /* 01H Reserved                              */
#define PAGE_CODE_CONNECTION            0x02	/* 02H Disconnect/Reconnect Parameters       */
#define PAGE_CODE_SCANNING_MEASUREMENTS 0x03	/* 03H Scanning Measurement Parameters       */
					     /* 04H-08H Reserved                          */
					     /* 09H-0AH Reserved (Not supported)          */
					     /* 0BH-1FH Reserved                          */
#define PAGE_CODE_WHITE_BALANCE        0x20	/* 20H White Balance                         */
					     /* 21H Reserved (Vendor Unique)              */
#define PAGE_CODE_LAMP_TIMER_SET       0x22	/* 22H Lamp Timer Set                        */
#define PAGE_CODE_SCANNING_SPEED       0x23	/* 23H Reserved (Scanning speed select)      */
					     /* 24H Reserved (Vendor Unique)              */
					     /* 25H Reserved (Vendor Unique)              */
#define PAGE_CODE_ADF_CONTROL          0x26	/* 26H ADF Control                           */
#define PAGE_CODE_ENDORSER_CONTROL     0x27	/* 27H Endorser Control                      */
					     /* 28H Reserved (Marker Area Data Processing) */
					     /* 29H-2AH Reserved (Vendor Unique)          */
#define PAGE_CODE_SCAN_WAIT_MODE       0x2B	/* 2BH Scan Wait Mode (Medium Wait Mode)     */
					     /* 2CH-3DH Reserved (Vendor Unique)          */
#define PAGE_CODE_SERVICE_MODE_SELECT  0x3E	/* 3EH Service Mode Select                   */
					     /* 3FH Reserved (Not Supported)              */

typedef struct mode_page_connect
{
  MPHdr hdr;			/* Mode Page Header: 4 bytes            */
  SANE_Byte code;		/* 7-6:Reserved; 5-0: 02H  */
  SANE_Byte len;		/* Parameter Length 0EH    */
  SANE_Byte buffer_full_ratio;	/* Ignored                 */
  SANE_Byte buffer_empty_ratio;	/* Ignored                 */
  SANE_Byte bus_inactive_limit[2];	/* Ignored                 */
  SANE_Byte disconnect_time_limit[2];	/* indicates minimum time to disconnect SCSI bus until reconnection.
					 * It is expressed in 100msec increments; i.e. "1" for 100msec, "2" for 200msec
					 * The maximum time is 2sec */
  SANE_Byte connect_time_limit[2];	/* Ignored                  */
  SANE_Byte maximum_burst_size[2];	/* expressed in 512 increments, i.e. "1" for 512 bytes, "2" for 1024 bytes
					 * "0" indicates unlimited amount of data */
  SANE_Byte dtdc;		/* 7-2:Reserved; 1-0:DTDC indicates limitations of disconnection (bit1,bit0):
				 * 00 (DEFAULT) Controlled by the other field in this page
				 * 01 Once the command data transfer starts, the target never disconnects until
				 *    the whole data transfer completes
				 * 10 Reserved
				 * 11 Once the command data transfer starts, the target never disconnects until
				 *    the completion of the command
				 */
  SANE_Byte reserved[3];
} MP_CXN;

/* 1 inch = 6 picas = 72 points = 25.4 mm */
#define DEFAULT_MUD 1200	/* WHY ? */
/* BASIC MEASUREMENT UNIT
 * 00H INCH
 * 01H MILLIMETER
 * 02H POINT
 * 03H-FFH Reserved
*/
enum BMU
{ INCHES = 0, MILLIMETERS, POINTS };	/* Basic Measurement Unit */

typedef struct mode_page_scanning_measurement
{
  MPHdr hdr;			/* Mode Page Header: 4 bytes         */
  SANE_Byte code;		/* 7-6:Reserved; 5-0:Page Code (03H) */
  SANE_Byte len;		/* Parameter Length (06H)            */
  SANE_Byte bmu;		/* Basic Measurement Unit            */
  SANE_Byte reserved0;
  SANE_Byte mud[2];		/* Measurement Unit Divisor          
				 * produces an error if 0
				 * mud is fixed to 1 for millimeter or point
				 * point is default when scanner powers on */
  SANE_Byte reserved1[2];
} MP_SMU;			/* Scanning Measurement Units */

typedef struct mode_page_white_balance
{
  MPHdr hdr;			/* Mode Page Header: 4 bytes         */
  SANE_Byte code;		/* 7-6:Reserved; 5-0:Page Code (03H) */
  SANE_Byte len;		/* Parameter Length (06H)            */
  SANE_Byte white_balance;	/* "0" selects relative white mode (DEFAULT when power on)
				 * "1" selects absolute white mode */
  SANE_Byte reserved[5];
} MP_WhiteBal;			/* White Balance */

typedef struct mode_page_lamp_timer_set
{
  MPHdr hdr;			/* Mode Page Header: 4 bytes            */
  SANE_Byte code;		/* 7-6:Reserved; 5-0:Page Code (22H)    */
  SANE_Byte len;		/* Parameter Length (06H)               */
  SANE_Byte time_on;		/* indicates the time of lamp turned on */
  SANE_Byte ignored[5];
} MP_LampTimer;			/* Lamp Timer Set (Not supported ) */

typedef struct mode_page_adf_control
{
  MPHdr hdr;			/* Mode Page Header: 4 bytes            */
  SANE_Byte code;		/* 7-6:Reserved; 5-0:Page Code (26H)    */
  SANE_Byte len;		/* Parameter Length (06H)               */
  SANE_Byte adf_control;	/* 7-2:Reserved; 1-0:ADF selection:
				 * 00H Book Mode (DEFAULT when power on)
				 * 01H Simplex ADF
				 * 02H Duplex ADF
				 * 03H-FFH Reserved                     */
  SANE_Byte adf_mode_control;	/* 7-3:Reserved; 2:Prefeed Mode Validity 1-0:Ignored 
				 * Prefeed Mode "0" means invalid, "1" means valid */
  SANE_Byte medium_wait_timer;	/* indicates time for scanner to wait for media. Scanner 
				 * will send CHECK on timeout. NOT SUPPORTED */
  SANE_Byte ignored[3];
} MP_ADF;			/* ADF Control */

typedef struct mode_page_endorser_control
{
  MPHdr hdr;			/* Mode Page Header: 4 bytes            */
  SANE_Byte code;		/* 7-6:Reserved; 5-0:Page Code (27H)    */
  SANE_Byte len;		/* Parameter Length (06H)               */
  SANE_Byte endorser_control;	/* 7-3:Reserved; 2-0:Endorser Control:
				 * 0H Disable Endorser (DEFAULT)
				 * 1H Enable Endorser
				 * 3H-7H Reserved                       */
  SANE_Byte ignored[5];
} MP_EndCtrl;			/* Endorser Control */

typedef struct mode_page_scan_wait
{
  MPHdr hdr;			/* Mode Page Header: 4 bytes            */
  SANE_Byte code;		/* 7-6:Reserved; 5-0:Page Code (2BH)    */
  SANE_Byte len;		/* Parameter Length (06H)               */
  SANE_Byte swm;		/* 7-1:Reserved; 0:Scan Wait Mode
				 * 0H Disable Medium wait mode
				 * 1H Enable  Medium wait mode
				 * In Medium wait mode, when SCAN, READ, or LOAD (in ADF mode) is issued,
				 * the scanner waits until start button is pressed on operation panel
				 * When abort button is pressed, the command is cancelled
				 * In ADF mode, when there are no originals on ADF, CHECK condition is
				 * not given unless start button is pressed. */
  SANE_Byte ignored[5];
} MP_SWM;			/* Scan Wait */

typedef struct mode_page_service
{				/* Selectable when Send Diagnostic command is performed */
  MPHdr hdr;			/* Mode Page Header: 4 bytes            */
  SANE_Byte code;		/* 7-6:Reserved; 5-0:Page Code (3EH)    */
  SANE_Byte len;		/* Parameter Length (06H)               */
  SANE_Byte service;		/* 7-1:Reserved; 0:Service Mode
				 * "0" selects Self Diagnostics mode (DEFAULT when power on )
				 * "1" selects Optical Adjustment mode  */
  SANE_Byte ignored[5];
} MP_SRV;			/* Service */

typedef struct scsi_mode_sense_cmd
{
  SANE_Byte opcode;		/* 1AH */
  SANE_Byte dbd;		/* 7-5:LUN; 4:Reserved; 3:DBD (Disable Block Desciption) set to "0"; 2-0:Reserved */
  SANE_Byte pc;			/* 7-6:PC; 5-0:Page Code
				 * PC field indicates the type of data to be returned (bit7,bit6):
				 * 00 Current Value   (THIS IS THE ONLY VALUE WHICH WORKS!)
				 * 01 Changeable Value
				 * 10 Default Value
				 * 11 Saved Value
				 *
				 * Page Code indicates requested page. (See PAGE_CODE defines) */
  SANE_Byte reserved;
  SANE_Byte len;		/* Allocation length */
  SANE_Byte control;		/* 7-6:Vendor Unique; 5-2:Reserved; 1:Flag; 0:Link */
} SENSE;
/* MODE SENSE DATA FORMAT -- 
 * The format of Sense Data to be returned is Mode Parameter Header + Page 
 * see struct scsi_mode_parameter_header 
 *     struct mode_pages
*/

/* 1-3-8 SCAN command */
typedef struct scsi_start_scan_cmd
{
  SANE_Byte opcode;		/* 1BH                   */
  SANE_Byte byte1;		/* 7-5:LUN; 4-0:Reserved */
  SANE_Byte page_code;
  SANE_Byte reserved;
  SANE_Byte len;		/* Transfer Length       
				 * Length of Window List in bytes 
				 * Since scanner supports up to 2 windows, len is 1 or 2
				 */
  SANE_Byte control;		/* 7-6:Vendor Unique; 5-2:Reserved; 1:Flag; 0:Link */
} START_SCAN;

/* 1-3-9 RECEIVE DIAGNOSTIC
 * 1-3-10 SEND DIAGNOSTIC */

/* BinaryFilter Byte
 * bit7: Noise Removal '1':removal
 * bit6: Smoothing     '1':smoothing
 * bits5-2: ignored
 * bits1-0: Noise Removal Matrix
 * 00:3x3    01:4x4
 * 10:5x5    11:Reserved
*/
struct val_id
{
  SANE_Byte val;
  SANE_Byte id;
};
static SANE_String_Const noisematrix_list[] = {
  "None", "3x3", "4x4", "5x5", NULL
};
struct val_id noisematrix[] = {
  {0x03, 0},			/* dummy <reserved> value for "None" */
  {0x00, 1},
  {0x01, 2},
  {0x02, 3}
};
static SANE_String_Const grayfilter_list[] = {
  "none", "averaging", "MTF correction", NULL
};
struct val_id grayfilter[] = {
  {0x00, 0},
  {0x01, 1},
  {0x03, 2}
};

static SANE_String_Const paddingtype_list[] = {
  "Pad with 0's to byte boundary",
  "Pad with 1's to byte boundary",
  "Truncate to byte boundary",
  NULL
};
enum paddingtypes
{ PAD_WITH_ZEROS = 0x01, PAD_WITH_ONES, TRUNCATE };
struct val_id paddingtype[] = {
  {PAD_WITH_ZEROS, 0},
  {PAD_WITH_ONES, 1},
  {TRUNCATE, 2}
};

#define NPADDINGTYPES 3
#define PADDINGTYPE_DEFAULT 2
static SANE_String_Const auto_separation_list[] = {
  "Off", "On", "User", NULL
};
struct val_id auto_separation[] = {
  {0x00, 0},
  {0x01, 1},
  {0x80, 2}
};
static SANE_String_Const auto_binarization_list[] = {
  "Off",
  "On",
  "Enhancement of light characters",
  "Removal of background color",
  "User",
  NULL
};
struct val_id auto_binarization[] = {
  {0x00, 0},
  {0x01, 1},
  {0x02, 2},
  {0x03, 3},
  {0x80, 4}
};
enum imagecomposition
{ LINEART = 0x00, HALFTONE, GRAYSCALE };
enum halftonecode
{ DITHER = 0x02, ERROR_DIFFUSION };
static SANE_String_Const halftone_code[] = {
  "Dither", "Error Diffusion", NULL
};
static SANE_String_Const halftone_pattern_list[] = {
  "8x4, 45 degree",
  "6x6, 90 degree",
  "4x4, spiral",
  "8x8, 90 degree",
  "70 lines",
  "95 lines",
  "180 lines",
  "16x8, 45 degree",
  "16x16, 90 degree",
  "8x8, Bayer",
  "User #1",
  "User #2",
  NULL
};
struct val_id halftone[] = {
  {0x01, 1},
  {0x02, 2},
  {0x03, 3},
  {0x04, 4},
  {0x05, 5},
  {0x06, 6},
  {0x07, 7},
  {0x08, 9},
  {0x09, 9},
  {0x0A, 10},
  {0x80, 11},
  {0x81, 12}
};

#if 0
static struct
{
  SANE_Byte code;
  char *type;
} compression_types[] =
{
  {
  0x00, "No compression"},
  {
  0x01, "CCITT G3, 1-dimensional (MH)"},
  {
  0x02, "CCITT G3, 2-dimensional (MR)"},
  {
  0x03, "CCITT G4, 2-dimensional (MMR)"},
    /* 04H-0FH Reserved
     * 10H Reserved (not supported)
     * 11H-7FH Reserved
     */
  {
  0x80, "CCITT G3, 1-dimensional (MH) Padding with 0's to byte boundary"}
  /* 80H-FFH Reserved (Vendor Unique) */
};
static struct
{
  SANE_Byte code;
  char *argument;
} compression_argument[] =
{
  /* 00H Reserved */
  /* 01H Reserved */
  {
  0x02, "K factor-0~255"}
  /* 03H Reserved */
  /* 04H-0FH Reserved */
  /* 10H Reserved */
  /* 11H-7FH Reserved */
  /* 80H Reserved */
  /* 80H-FFH Reserved */
};
#endif
#define GAMMA_NORMAL  0x00
#define GAMMA_SOFT    0x01
#define GAMMA_SHARP   0x02
#define GAMMA_LINEAR  0x03
#define GAMMA_USER    0x08
/* 04H-07H Reserved */
/* 09H-0FH Reserved */
static SANE_String gamma_list[6] = {
  "Normal", "Soft", "Sharp", "Linear", "User", NULL
};

/* 1-3-11 SET WINDOW command */

struct window_section
{				/* 32 bytes */
  SANE_Byte sef;		/*byte1 7-2:ignored 1:SEF '0'-invalid section; '1'-valid section */
  SANE_Byte ignored0;
  SANE_Byte ulx[4];
  SANE_Byte uly[4];
  SANE_Byte width[4];
  SANE_Byte length[4];
  SANE_Byte binary_filtering;
  SANE_Byte ignored1;
  SANE_Byte threshold;
  SANE_Byte ignored2;
  SANE_Byte image_composition;
  SANE_Byte halftone_id;
  SANE_Byte halftone_code;
  SANE_Byte ignored3[7];
};
/* 1-3-11 SET WINDOW COMMAND
 * Byte0: 24H
 * Byte1: 7-5: LUN; 4-0: Reserved
 * Byte2-5: Reserved
 * Byte6-8: Transfer Length
 * Byte9: 7-6: Vendor Unique; 5-2: Reserved; 1: Flag; 0: Link
 *
 * Transfer length indicates the byte length of Window Parameters (Set Window Data Header +
 * Window Descriptor Bytes transferred from the initiator in the DATA OUT PHASE
 * The scanner supports 2 windows, so Transfer Length is 648 bytes:
 * Set Window Header 8 bytes + Window Descriptor Bytes 640 (320*2) bytes). 
 * If data length is longer than 648 bytes only the first 648 bytes are valid, The remainng data is ignored.
 * If data length is shorter than 648 only the specified byte length is valid data.
 *
 *
 * WINDOW DATA HEADER
 * Byte0-5: Reserved
 * Byte6-7: Window Descriptor Length (WDL)
 *          WDL indicates the number of bytes of one Window Descriptor Bytes which follows.
 *          In this scanner, this value is 640 since it supports 2 windows.
 *
 * WINDOW DESCRIPTOR BYTES
*/
#define HS2P_WINDOW_DATA_SIZE 640
struct hs2p_window_data
{				/* HS2P_WINDOW_DATA_FORMAT       */
  SANE_Byte window_id;		/*     0: Window Identifier      */
  SANE_Byte auto_bit;		/*     1: 1-1:Reserved; 0:Auto   */
  SANE_Byte xres[2];		/*   2-3: X-Axis Resolution      100-800dpi in 1dpi steps */
  SANE_Byte yres[2];		/*   4-5: Y-Axis Resolution      100-800dpi in 1dpi steps */
  SANE_Byte ulx[4];		/*   6-9: X-Axis Upper Left      */
  SANE_Byte uly[4];		/* 10-13: Y-Axis Upper Left      */
  SANE_Byte width[4];		/* 14-17: Window Width           */
  SANE_Byte length[4];		/* 18-21: Window Length          */
  SANE_Byte brightness;		/*    22: Brightness  [0-255] dark-light 0 means default value of 128 */
  SANE_Byte threshold;		/*    23: Threshold   [0-255] 0 means default value of 128            */
  SANE_Byte contrast;		/*    24: Contrast    [0-255] low-high   0 means default value of 128 */
  SANE_Byte image_composition;	/*    25: Image Composition      
				 *        00H Lineart
				 *        01H Dithered Halftone
				 *        02H Gray scale
				 */
  SANE_Byte bpp;		/*    26: Bits Per Pixel         */
  SANE_Byte halftone_code;	/*    27: Halftone Code          
				 *        00H-01H Reserved
				 *        02H Dither (partial Dot)
				 *        03H Error Diffusion
				 *        04H-07H Reserved
				 */
  SANE_Byte halftone_id;	/*    28: Halftone ID            
				 *        00H Reserved
				 *        01H 8x4, 45 degree
				 *        02H 6x6, 90 degree
				 *        03H 4x4, Spiral
				 *        04H 8x8, 90 degree
				 *        05H 70 lines
				 *        06H 95 lines
				 *        07H 180 lines
				 *        08H 16x8, 45 degree
				 *        09H 16x16, 90 degree
				 *        0AH 8x8, Bayer
				 *        0Bh-7FH Reserved
				 *        80H Download #1
				 *        81H Download #2
				 *        82H-FFH Reserved
				 */
  SANE_Byte byte29;		/*    29:   7: RIF (Reverse Image Format) bit inversion
				 *             Image Composition field must be lineart or dithered halftone
				 *             RIF=0: White=0 Black=1
				 *             RIF=1: White=1 Black=0
				 *        6-3: Reserved; 
				 *        2-0: Padding Type:
				 *             00H Reserved
				 *             01H Pad with 0's to byte boundary
				 *             02H Pad with 1's to byte boundary
				 *             03H Truncate to byte boundary
				 *             04H-FFH Reserved
				 */
  SANE_Byte bit_ordering[2];	/* 30-31: Bit Ordering: Default 0xF8
				 *        0: 0=>output from bit0 of each byte; 1=>output from bit7
				 *        1: 0=>output from LSB; 1=>output from MSB
				 *        2: 0=>unpacked 4 bits gray; 1=>Packed 4 bits gray
				 *        3: 1=>Bits arrangment from LSB in grayscale; 0=>from MSB
				 *      4-6: reserved
				 *        7: 1=>Mirroring; 0=>Normal output
				 *     8-15: reserved
				 */
  SANE_Byte compression_type;	/*    32: Compression Type:     Unsupported in IS450   */
  SANE_Byte compression_arg;	/*    33: Compression Argument: Unsupported in IS450   */
  SANE_Byte reserved2[6];	/* 34-39: Reserved               */
  SANE_Byte ignored1;		/*    40: Ignored                */
  SANE_Byte ignored2;		/*    41: Ignored                */
  SANE_Byte byte42;		/*    42:   7: MRIF: Grayscale Reverse Image Format
				 *             MRIF=0: White=0 Black=1
				 *             MRIF=1: White=1 Black=0
				 *        6-4: Filtering: for Grayscale
				 *             000 No filter
				 *             001 Averaging
				 *             010 Reserved
				 *             011 MTF Correction
				 *             100 Reserved
				 *             110 Reserved
				 *             111 Reserved
				 *        3-0: Gamma ID 
				 *             00H Normal
				 *             01H Soft
				 *             02H Sharp
				 *             03H Linear
				 *             04H-07H Reserved
				 *             08H Download table
				 *             09H-0FH Reserved
				 */
  SANE_Byte ignored3;		/*    43: Ignored                */
  SANE_Byte ignored4;		/*    44: Ignored                */
  SANE_Byte binary_filtering;	/*    45: Binary Filtering              
				 *        0-1: Noise Removal Matrix:
				 *             00: 3x3
				 *             01: 4x4
				 *             10: 5x5
				 *             11: Reserved
				 *        5-2: Ignored
				 *          6: Smoothing Flag
				 *          7: Noise Removal Flag
				 *
				 *          Smoothing and Noise removal can be set when option IPU is installed
				 *          Setting is ignored for reverse side because optional IPU is not valid
				 *          for reverse side scanning
				 */
  /* 
   *  The following is only available when IPU is installed:
   *  SECTION, Automatic Separation, Automatic Binarization
   *  46-319 is ignored for Window 2       
   */
  SANE_Byte ignored5;		/*    46: Ignored                       */
  SANE_Byte ignored6;		/*    47: Ignored                       */
  SANE_Byte automatic_separation;	/*    48: Automatic Separation          
					 *            00H OFF
					 *            01H Default
					 *        02H-7FH Reserved
					 *            80H Download table
					 *        91H-FFH Reserved
					 */
  SANE_Byte ignored7;		/*    49: Ignored                       */
  SANE_Byte automatic_binarization;	/*    50: Automatic Binarization        
					 *            00H OFF
					 *            01H Default
					 *            02H Enhancement of light characters
					 *            03H Removal of background color
					 *        04H-7FH Reserved
					 *            80H Download table
					 *        81H-FFH Reserved
					 */
  SANE_Byte ignored8[13];	/* 51-63: Ignored                       */
  struct window_section sec[8];	/* Each window can have multiple sections, each of 32 bytes long
				 * 53-319: = 256 bytes = 8 sections of 32 bytes
				 * IS450 supports up to 4 sections, 
				 * IS420 supports up to 6 sections 
				 */
};
struct set_window_cmd
{
  SANE_Byte opcode;		/* 24H */
  SANE_Byte byte2;		/* 7-5:LUN 4-0:Reserve */
  SANE_Byte reserved[4];	/* Reserved */
  SANE_Byte len[3];		/* Transfer Length */
  SANE_Byte control;		/* 76543210
				 * XX       Vendor Unique
				 *   XXXX   Reserved
				 *       X  Flag
				 *        X Link
				 */
};
struct set_window_data_hdr
{
  SANE_Byte reserved[6];
  SANE_Byte len[2];
};
typedef struct set_window_data
{
  struct set_window_data_hdr hdr;
  struct hs2p_window_data data[2];
} SWD;

/* 1-3-12 GET WINDOW command */
struct get_window_cmd
{
  SANE_Byte opcode;
  SANE_Byte byte1;		/* 7-5: LUN; * 4-1:Reserved; *   0:Single bit is 0 */
  SANE_Byte reserved[3];
  SANE_Byte win_id;		/* Window ID is either 0 or 1 */
  SANE_Byte len[3];		/* Transfer Length */
  SANE_Byte control;		/* 7-6:Vendor Unique; 5-2:Reserved; 1:Flag; 0:Link */
};
/* The data format to be returned is Get Window Data header + Window Descriptor Bytes
 * The format of Window Descriptor Bytes is the same as that for SET WINDOW 
*/
struct get_window_data_hdr
{
  SANE_Byte data_len[2];	/* Window Data Length indicates byte len of data which follows less its own 2 bytes */
  SANE_Byte reserved[4];
  SANE_Byte desc_len[2];	/* Window Descriptor Length indicates byte length of one Window Descriptor which is 640 */
};
typedef struct get_window_data
{
  struct get_window_data_hdr hdr;
  struct hs2p_window_data data[2];
} GWD;

/* READ/SEND DATA TYPE CODES  */
/* DATA TYPE CODES (DTC):                       */
#define DATA_TYPE_IMAGE                      0x00
/* 01H Reserved (Vendor Unique)                 */
#define DATA_TYPE_HALFTONE                   0x02
#define DATA_TYPE_GAMMA                      0x03
/*04H-7FH Reserved                              */
#define DATA_TYPE_ENDORSER                   0x80
#define DATA_TYPE_SIZE                       0x81
/* 82H Reserved                                 */
/* 83H Reserved (Vendor Unique)                 */
#define DATA_TYPE_PAGE_LEN                   0x84
#define DATA_TYPE_MAINTENANCE                0x85
#define DATA_TYPE_ADF_STATUS                 0x86
/* 87H Reserved (Skew Data)                     */
/* 88H-91H Reserved (Vendor Unique)             */
/* 92H Reserved (Scanner Extension I/O Access)  */
/* 93H Reserved (Vendor Unique)                 */
/* 94H-FFH Reserved (Vendor Unique)             */
#define DATA_TYPE_EOL			     -1	/* va_end */

/* DATA TYPE QUALIFIER CODES when DTC=93H       */
#define DTQ				     0x00	/* ignored */
#define DTQ_AUTO_PHOTOLETTER                 0x00	/* default */
#define DTQ_DYNAMIC_THRESHOLDING             0x01	/* default */
#define DTQ_LIGHT_CHARS_ENHANCEMENT          0x02
#define DTQ_BACKGROUND_REMOVAL               0x03
/* 04H-7FH Reserved                             */
#define DTQ_AUTO_PHOTOLETTER_DOWNLOAD_TABLE  0x80
#define DTQ_DYNAMIC_THRESHOLD_DOWNLOAD_TABLE 0x81
/* 82H-FFH Reserved                             */

/* 1-3-13 READ command */
/* 1-3-14 SEND command */
struct scsi_rs_scanner_cmd
{
  SANE_Byte opcode;		/* READ=28H  SEND=2AH                    */
  SANE_Byte byte1;		/* 7-5:LUN; 4-0:Reserved                 */
  SANE_Byte dtc;		/* Data Type Code: See DTC DEFINES above */
  SANE_Byte reserved;
  SANE_Byte dtq[2];		/* Data Type Qualifier valid only for DTC 02H,03H,93H */
  SANE_Byte len[3];		/* Transfer Length */
  SANE_Byte control;		/* 7-6:Vendor Unique; 5-2:Reserved; 1:Flag; 0:Link */
};
/*
 * Data Format for Image Data
 * Non-compressed: {first_line, second_line, ... nth_line}
 * MH/MR Compression: {EOL, 1st_line_compressed, EOL, 2nd_line_compressed,..., EOL, last_line_compressed, EOL,EOL,EOL,EOL,EOL,EOL
 *       where EOL = 000000000001
 * MMR Compression: = {1st_line_compressed, 2nd_line_compressed,...,last_line_compressed, EOL,EOL}
 *
 * Normal Binary Output: MSB-LSB 1stbytes,2nd,3rd...Last
 * Mirror Binary Output: MSB-LSB Last,...2nd,1st
 *
 * Normal Gray Output MSB-LSB: 1st,2nd,3rd...Last
 *       4 bit/pixel gray: [32103210]
 *       8 bit/pixel gray: [76543210]
 * Mirror Gray Output MSB-LSB Last,...2nd,1st
 *
 *
 * HALFTONE MASK DATA: 1byte(row,col) ={2,3,4,6,8,16}
 *   (r0,c0), (r0,c1), (r0,c2)...(r1,c0),(r1,c2)...(rn,cn)
 *
 * GAMMA FUNCTION TABLE Output (D) vs. Input (I)(0,0)=(Black,Black) (255,255)=(White,White)
 * The number of gray scale M = 8
 * 2^8 = 256 total table data
 * D0 = D(I=0), D1=D(I=1)...D255=D(I=255)
 * DATA= [1st byte ID],[2nd byte M],[D0],[D1],...[D255]
 *
 * ENDORSER DATA: 1st_char, 2nd_char,...last_char
 *
 * SIZE DATA: 1byte: 4bits-Start Position; 4bits-Width Info
 *
 * PAGE LENGTH: 5bytes: 1st byte is MSB, Last byte is LSB
*/

typedef struct maintenance_data
{
  SANE_Byte nregx_adf;		/* number of registers of main-scanning in ADF mode */
  SANE_Byte nregy_adf;		/* number of registers of sub-scanning  in ADF mode */
  SANE_Byte nregx_book;		/* number of registers of main-scanning in Book mode */
  SANE_Byte nregy_book;		/* number of registers of sub-scanning  in Book mode */
  SANE_Byte nscans_adf[4];	/* Number of scanned pages in ADF mode */
  SANE_Byte nscans_book[4];	/* Number of scanned pages in Book mode */
  SANE_Byte lamp_time[4];	/* Lamp Time */
  SANE_Byte eo_odd;		/* Adjustment data of E/O balance in black level (ODD) */
  SANE_Byte eo_even;		/* Adjustment data of E/O balance in black level (EVEN) */
  SANE_Byte black_level_odd;	/* The adjustment data in black level (ODD) */
  SANE_Byte black_level_even;	/* The adjustment data in black level (EVEN) */
  SANE_Byte white_level_odd[2];	/* The adjustment data in white level (ODD) */
  SANE_Byte white_level_even[2];	/* The adjustment data in white level (EVEN) */
  SANE_Byte first_adj_white_odd[2];	/* First adjustment data in white level (ODD) */
  SANE_Byte first_adj_white_even[2];	/* First adjustment data in white level (EVEN) */
  SANE_Byte density_adj;	/* Density adjustment */
  SANE_Byte nregx_reverse;	/* The number of registers of main-scanning of the reverse-side ADF */
  SANE_Byte nregy_reverse;	/* The number of registers of sub-scanning of the reverse-side ADF */
  SANE_Byte nscans_reverse_adf[4];	/* Number of scanned pages of the reverse side ADF */
  SANE_Byte reverse_time[4];	/* The period of lamp turn on of the reverse side */
  SANE_Byte nchars[4];		/* The number of endorser characters */
  SANE_Byte reserved0;
  SANE_Byte reserved1;
  SANE_Byte reserved2;
  SANE_Byte zero[2];		/* All set as 0 */
} MAINTENANCE_DATA;
/* ADF status 1byte: 
 * 7-3:Reserved; 
 *   2:Reserved; 
 *   1: '0'-ADF cover closed; '1'-ADF cover open
 *   0: '0'-Document on ADF; '1'-No document on ADF
 *
*/

struct IPU
{
  SANE_Byte byte0;		/* 7-4:Reserved; 3:White mode; 2:Reserved; 1-0: Gamma Table Select */
  SANE_Byte byte1;		/* 7-2:Reserved; 1-0: MTF Filter Select */
};
struct IPU_Auto_PhotoLetter
{
  /* Halftone Separations for each level 
   * 256 steps of relative value with 0 the sharpest and 255 the softest
   * The relation of strength is Strength2 > Strength3 > Strength4 ...
   */
  struct
  {
    SANE_Byte level[6];
  } halftone_separation[2];

  /* 7-2:Reversed 1-0:Halftone 
   * 00 Default
   * 01 Peak Detection Soft
   * 10 Peak Detection Sharp
   * 11 Don't Use
   */
  SANE_Byte byte12;

  SANE_Byte black_correction;	/* Black correction strength: 0-255 sharpest-softest */
  SANE_Byte edge_sep[4];	/* Edge Separation strengths: 0-255 sharpest-softest 1-4 */
  SANE_Byte white_background_sep_strength;	/* 0-255 sharpest-softest */
  SANE_Byte byte19;		/* 7-1:Reversed; 0:White mode    '0'-Default;    '1'-Sharp */
  SANE_Byte byte20;		/* 7-1:Reversed; 0:Halftone mode '0'-widen dots; '1'-Default */
  SANE_Byte halftone_sep_levela;
  SANE_Byte halftone_sep_levelb;
  SANE_Byte byte23;		/* 7-4:Reversed; 3-0:Adjustment of separation level: usually fixed to 0 */

  /* 7-4:Reversed; 3-0:Judge Conditions Select
   *  0XXX Black Correction OFF      1XXX Black Correction ON
   *  X0XX Halftone Separation OFF   X1XX Halftone Separation ON
   *  XX0X White Separation OFF      XX1X White Separation ON
   *  XXX0 Edge Separation OFF       XXX1 Edge Separation ON
   */
  SANE_Byte byte24;

  /* 7-4:Filter A; 3-0:Filter B 
   *  FilterA: 16 types are valid from 0000 to 1111
   *  FilterB: 0000 to 1110 are valid; 1111 is not valid
   */
  SANE_Byte MTF_correction;

  /* 7-4:Filter A; 3-0:Filter B 
   *  0000(soft) to 0111(sharp) are valid; 1000 to 1111 are invalid
   */
  SANE_Byte MTF_strength;

  /* 7-4:Filter A; 3-0:Filter B 
   * slightly adjusts the strength of the filters
   */
  SANE_Byte MTF_adjustment;

  /* 7-4:Reserved; 3-0: smoothing filter select 
   * 14 kinds are valid from 0000 to 1101; 1110 to 1111 are invalid
   */
  SANE_Byte smoothing;

  /* 7-2:Reversed; 1-0: Filter Select 
   *  10 MTF Correction Select
   *  11 Smoothing Select
   *  from 00 to 01 are not valid and basically it is set as 10
   */
  SANE_Byte byte29;

  /* 7-4:Reserved; 3-0: MTF Correction Filter C 
   * 16 kinds are valid from 0000 to 1111
   */
  SANE_Byte MTF_correction_c;

  /* 7-3:Reserved; 2-0: MTF Correction Filter strength C 
   *  000(soft) to 111(sharp) are valid
   */
  SANE_Byte MTF_strength_c;
};
/*
struct IPU_Dynamic {
  to be implemented
};
sensor data
*/

/* for object_position command */
#define OBJECT_POSITION_UNLOAD 0
#define OBJECT_POSITION_LOAD   1

/* 1-3-15 OBJECT POSITION */
typedef struct scsi_object_position_cmd
{
  SANE_Byte opcode;		/* 31H */
  SANE_Byte position_func;	/* 7-5:LUN; 4-3:Reserved; 2-0:Position Function (bit2,bit1,bit0):
				 * 000 Unload Object  (NO CHECK ERROR even though no document on ADF)
				 * 001 Load Object    (NO CHECK ERROR even though document already fed to start position)
				 * 010 Absolute Positioning in Y-axis. Not Supported in this scanner
				 * 3H-7H Reserved */
  SANE_Byte count[3];		/* Reserved */
  SANE_Byte reserved[4];
  SANE_Byte control;		/* 7-6:Vendor Unique; 5-2:Reserved; 1:Flag; 0:Link */
} POSITION;

/* 1-3-16 GET DATA BUFFER STATUS */
typedef struct scsi_get_data_buffer_status_cmd
{
  SANE_Byte opcode;		/* 34H */
  SANE_Byte wait;		/* 7-5:LUN; 4-1:Reserved; 0: Wait bit is "0" */
  SANE_Byte reserved[5];
  SANE_Byte len[2];		/* Allocation Length */
  SANE_Byte control;		/* 7-6:Vendor Unique; 5-2:Reserved; 1:Flag; 0:Link */
} GET_DBS_CMD;
typedef struct scsi_status_hdr
{
  SANE_Byte len[3];		/* Data Buffer Status Length */
  SANE_Byte block;		/* 7-1:Reserved; 0:Block bit is 0 */
} STATUS_HDR;
typedef struct scsi_status_data
{
  SANE_Byte wid;		/* window identifier is 0 or 1 */
  SANE_Byte reserved;
  SANE_Byte free[3];		/* Available Space Data `Buffer */
  SANE_Byte filled[3];		/* Scan Data Available (Filled Data Bufferj) */
} STATUS_DATA;
/* BUFFER STATUS DATA FORMAT */
typedef struct scsi_buffer_status
{
  STATUS_HDR hdr;
  STATUS_DATA data;
} STATUS_BUFFER;
