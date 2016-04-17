/* sane - Scanner Access Now Easy.
   Copyright (C) 2002 Frank Zago (sane at zago dot net)

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
   If you do not wish that, delete this exception notice. 
*/

/* 
	$Id$
*/

/* Commands supported by the KV-SS 25 scanner. */
#define SCSI_TEST_UNIT_READY			0x00
#define SCSI_INQUIRY					0x12
#define SCSI_SET_WINDOW					0x24
#define SCSI_READ_10					0x28
#define SCSI_REQUEST_SENSE				0x03

typedef struct
{
  unsigned char data[16];
  int len;
}
CDB;


/* Set a specific bit depending on a boolean.
 *   MKSCSI_BIT(TRUE, 3) will generate 0x08. */
#define MKSCSI_BIT(bit, pos) ((bit)? 1<<(pos): 0)

/* Set a value in a range of bits.
 *   MKSCSI_I2B(5, 3, 5) will generate 0x28 */
#define MKSCSI_I2B(bits, pos_b, pos_e) ((bits) << (pos_b) & ((1<<((pos_e)-(pos_b)+1))-1))

/* Store an integer in 2, 3 or 4 byte in an array. */
#define Ito16(val, buf) { \
 ((unsigned char *)buf)[0] = ((val) >> 8) & 0xff; \
 ((unsigned char *)buf)[1] = ((val) >> 0) & 0xff; \
}

#define Ito24(val, buf) { \
 ((unsigned char *)buf)[0] = ((val) >> 16) & 0xff; \
 ((unsigned char *)buf)[1] = ((val) >>  8) & 0xff; \
 ((unsigned char *)buf)[2] = ((val) >>  0) & 0xff; \
}

#define Ito32(val, buf) { \
 ((unsigned char *)buf)[0] = ((val) >> 24) & 0xff; \
 ((unsigned char *)buf)[1] = ((val) >> 16) & 0xff; \
 ((unsigned char *)buf)[2] = ((val) >>  8) & 0xff; \
 ((unsigned char *)buf)[3] = ((val) >>  0) & 0xff; \
}

#define MKSCSI_TEST_UNIT_READY(cdb) \
	cdb.data[0] = SCSI_TEST_UNIT_READY; \
	cdb.data[1] = 0; \
	cdb.data[2] = 0; \
	cdb.data[3] = 0; \
	cdb.data[4] = 0; \
	cdb.data[5] = 0; \
	cdb.len = 6;

#define MKSCSI_INQUIRY(cdb, buflen) \
	cdb.data[0] = SCSI_INQUIRY; \
	cdb.data[1] = 0; \
	cdb.data[2] = 0; \
	cdb.data[3] = 0; \
	cdb.data[4] = buflen; \
	cdb.data[5] = 0; \
	cdb.len = 6;

#define MKSCSI_SET_WINDOW(cdb, buflen) \
	cdb.data[0] = SCSI_SET_WINDOW; \
	cdb.data[1] = 0; \
	cdb.data[2] = 0; \
	cdb.data[3] = 0; \
	cdb.data[4] = 0; \
	cdb.data[5] = 0; \
	cdb.data[6] = (((buflen) >> 16) & 0xff); \
	cdb.data[7] = (((buflen) >>  8) & 0xff); \
	cdb.data[8] = (((buflen) >>  0) & 0xff); \
	cdb.data[9] = 0; \
	cdb.len = 10;

#define MKSCSI_READ_10(cdb, dtc, dtq, buflen) \
	cdb.data[0] = SCSI_READ_10; \
	cdb.data[1] = 0; \
	cdb.data[2] = (dtc); \
	cdb.data[3] = 0; \
	cdb.data[4] = (((dtq) >> 8) & 0xff); \
	cdb.data[5] = (((dtq) >> 0) & 0xff); \
	cdb.data[6] = (((buflen) >> 16) & 0xff); \
	cdb.data[7] = (((buflen) >>  8) & 0xff); \
	cdb.data[8] = (((buflen) >>  0) & 0xff); \
	cdb.data[9] = 0; \
	cdb.len = 10;

#define MKSCSI_REQUEST_SENSE(cdb, buflen) \
	cdb.data[0] = SCSI_REQUEST_SENSE; \
	cdb.data[1] = 0; \
	cdb.data[2] = 0; \
	cdb.data[3] = 0; \
	cdb.data[4] = (buflen); \
	cdb.data[5] = 0; \
	cdb.len = 6;

/*--------------------------------------------------------------------------*/

static inline int
getbitfield (unsigned char *pageaddr, int mask, int shift)
{
  return ((*pageaddr >> shift) & mask);
}

/* defines for request sense return block */
#define get_RS_information_valid(b)       getbitfield(b + 0x00, 1, 7)
#define get_RS_error_code(b)              getbitfield(b + 0x00, 0x7f, 0)
#define get_RS_filemark(b)                getbitfield(b + 0x02, 1, 7)
#define get_RS_EOM(b)                     getbitfield(b + 0x02, 1, 6)
#define get_RS_ILI(b)                     getbitfield(b + 0x02, 1, 5)
#define get_RS_sense_key(b)               getbitfield(b + 0x02, 0x0f, 0)
#define get_RS_information(b)             getnbyte(b+0x03, 4)
#define get_RS_additional_length(b)       b[0x07]
#define get_RS_ASC(b)                     b[0x0c]
#define get_RS_ASCQ(b)                    b[0x0d]
#define get_RS_SKSV(b)                    getbitfield(b+0x0f,1,7)

/*--------------------------------------------------------------------------*/

#define mmToIlu(mm) (((mm) * 1200) / MM_PER_INCH)
#define iluToMm(ilu) (((ilu) * MM_PER_INCH) / 1200)

#define PAGE_FRONT		0x00
#define PAGE_BACK		0x80

/*--------------------------------------------------------------------------*/

enum Matsushita_Option
{
  OPT_NUM_OPTS = 0,

  OPT_MODE_GROUP,
  OPT_MODE,			/* scanner modes */
  OPT_RESOLUTION,		/* X and Y resolution */
  OPT_DUPLEX,			/* Duplex mode */
  OPT_FEEDER_MODE,		/* Feeding mode */

  OPT_GEOMETRY_GROUP,
  OPT_PAPER_SIZE,		/* Paper size */
  OPT_TL_X,			/* upper left X */
  OPT_TL_Y,			/* upper left Y */
  OPT_BR_X,			/* bottom right X */
  OPT_BR_Y,			/* bottom right Y */

  OPT_ENHANCEMENT_GROUP,
  OPT_BRIGHTNESS,		/* Brightness */
  OPT_CONTRAST,			/* Contrast */
  OPT_AUTOMATIC_THRESHOLD,	/* Automatic threshold */
  OPT_HALFTONE_PATTERN,		/* Halftone pattern */
  OPT_AUTOMATIC_SEPARATION,	/* Automatic separation */
  OPT_WHITE_LEVEL,		/* White level */
  OPT_NOISE_REDUCTION,		/* Noise reduction */
  OPT_IMAGE_EMPHASIS,		/* Image emphasis */
  OPT_GAMMA,			/* Gamma */


  /* must come last: */
  OPT_NUM_OPTIONS
};

/*--------------------------------------------------------------------------*/

#define BLACK_WHITE_STR		SANE_VALUE_SCAN_MODE_LINEART
#define GRAY4_STR			SANE_I18N("Grayscale 4 bits")
#define GRAY8_STR			SANE_I18N("Grayscale 8 bits")

/*--------------------------------------------------------------------------*/

#define SANE_NAME_DUPLEX			"duplex"
#define SANE_NAME_PAPER_SIZE		"paper-size"
#define SANE_NAME_AUTOSEP			"autoseparation"

#define SANE_TITLE_DUPLEX			SANE_I18N("Duplex")
#define SANE_TITLE_PAPER_SIZE		SANE_I18N("Paper size")
#define SANE_TITLE_AUTOSEP			SANE_I18N("Automatic separation")

#define SANE_DESC_DUPLEX \
SANE_I18N("Enable Duplex (Dual-Sided) Scanning")
#define SANE_DESC_PAPER_SIZE \
SANE_I18N("Physical size of the paper in the ADF");
#define SANE_DESC_AUTOSEP \
SANE_I18N("Automatic separation")

/*--------------------------------------------------------------------------*/

/* Differences between the scanners. 
 * The scsi_* fields are used to lookup the correcte entry. */
struct scanners_supported
{
  int scsi_type;
  char scsi_vendor[9];
  char scsi_product[17];

  SANE_Range x_range;
  SANE_Range y_range;
  SANE_Range brightness_range;
  SANE_Range contrast_range;

  SANE_String_Const *scan_mode_list;	/* array of scan modes */
  const SANE_Word *resolutions_list;	/* array of available resolutions */
  const SANE_Word *resolutions_round;	/* rounding values for each resolutions */

  SANE_String_Const *image_emphasis_list;	/* list of image emphasis options */
  const int *image_emphasis_val;	/* list of image emphasis values */

  /* Scanner capabilities. */
  int cap;			/* bit field */
#define MAT_CAP_DUPLEX					0x00000002	/* can do duplex */
#define MAT_CAP_CONTRAST				0x00000004	/* have contrast */
#define MAT_CAP_AUTOMATIC_THRESHOLD		0x00000008
#define MAT_CAP_WHITE_LEVEL				0x00000010
#define MAT_CAP_GAMMA					0x00000020
#define MAT_CAP_NOISE_REDUCTION			0x00000040
#define MAT_CAP_PAPER_DETECT			0x00000080
#define MAT_CAP_MIRROR_IMAGE			0x00000100
#define MAT_CAP_DETECT_DOUBLE_FEED		0x00000200
#define MAT_CAP_MANUAL_FEED				0x00000400
};

struct paper_sizes
{
  SANE_String_Const name;	/* name of the paper */
  int width;
  int length;
};

/*--------------------------------------------------------------------------*/

/* Define a scanner occurence. */
typedef struct Matsushita_Scanner
{
  struct Matsushita_Scanner *next;
  SANE_Device sane;

  char *devicename;
  int sfd;			/* device handle */

  /* Infos from inquiry. */
  char scsi_type;
  char scsi_vendor[9];
  char scsi_product[17];
  char scsi_version[5];

  /* Scanner infos. */
  int scnum;			/* index of that scanner in
				   * scanners_supported */

  SANE_String_Const *paper_sizes_list;	/* names of supported papers */
  int *paper_sizes_val;		/* indirection into paper_sizes[] */

  /* SCSI handling */
  size_t buffer_size;		/* size of the buffer */
  SANE_Byte *buffer;		/* for SCSI transfer. */

  /* Scanning handling. */
  int scanning;			/* TRUE if a scan is running. */
  int resolution;		/* resolution in DPI, for both X and Y */
  int x_tl;			/* X top left */
  int y_tl;			/* Y top left */
  int x_br;			/* X bottom right */
  int y_br;			/* Y bottom right */
  int width;			/* width of the scan area in mm */
  int length;			/* length of the scan area in mm */

  enum
  {
    MATSUSHITA_BW,
    MATSUSHITA_HALFTONE,
    MATSUSHITA_GRAYSCALE
  }
  scan_mode;

  int depth;			/* depth per color */
  int halftone_pattern;		/* haltone number, valid for MATSUSHITA_HALFTONE */

  size_t bytes_left;		/* number of bytes left to give to the backend */

  size_t real_bytes_left;	/* number of bytes left the scanner will return. */

  SANE_Parameters params;

  int page_side;		/* 0=front, 1=back */
  int page_num;			/* current number of the page */

  /* For Grayscale 4 bits only */
  SANE_Byte *image;		/* keep the current image there */
  size_t image_size;		/* allocated size of image */
  size_t image_begin;		/* first significant byte in image */
  size_t image_end;		/* first free byte in image */


  /* Options */
  SANE_Option_Descriptor opt[OPT_NUM_OPTIONS];
  Option_Value val[OPT_NUM_OPTIONS];
}
Matsushita_Scanner;

/*--------------------------------------------------------------------------*/

/* Debug levels. 
 * Should be common to all backends. */

#define DBG_error0  0
#define DBG_error   1
#define DBG_sense   2
#define DBG_warning 3
#define DBG_inquiry 4
#define DBG_info    5
#define DBG_info2   6
#define DBG_proc    7
#define DBG_read    8
#define DBG_sane_init   10
#define DBG_sane_proc   11
#define DBG_sane_info   12
#define DBG_sane_option 13

/*--------------------------------------------------------------------------*/

/* 32 bits from an array to an integer (eg ntohl). */
#define B32TOI(buf) \
	((((unsigned char *)buf)[0] << 24) | \
	 (((unsigned char *)buf)[1] << 16) | \
	 (((unsigned char *)buf)[2] <<  8) |  \
	 (((unsigned char *)buf)[3] <<  0))
