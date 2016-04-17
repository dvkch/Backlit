/* sane - Scanner Access Now Easy.
   Copyright (C) 1996, 1997 David Mosberger-Tang, 1998 Andreas Bolsch for
   extension to ScanExpress models version 0.5,
   2000 - 2005 Henning Meier-Geinitz.
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

   This file implements a SANE backend for Mustek and some Trust flatbed
   scanners with SCSI or proprietary interface.  */

#ifndef mustek_h
#define mustek_h

#include "../include/sane/config.h"
#include <sys/types.h>

/* Some constants */
#define INQ_LEN	0x60		/* Length of SCSI inquiry */
#ifndef PATH_MAX
# define PATH_MAX 1024
#endif
#define MUSTEK_CONFIG_FILE "mustek.conf"

#define MAX_WAITING_TIME 60	/* How long to wait for scanner to become ready */
#define MAX_LINE_DIST 40	/* Extra lines needed for LD correction */

/* Flag values */
/* Scanner types */
#define MUSTEK_FLAG_THREE_PASS	 (1 << 0)	/* three pass scanner */
#define MUSTEK_FLAG_PARAGON_1    (1 << 1)	/* Paragon series I scanner */
#define MUSTEK_FLAG_PARAGON_2    (1 << 2)	/* Paragon series II(A4) scanner */
#define MUSTEK_FLAG_SE		 (1 << 3)	/* ScanExpress scanner */
#define MUSTEK_FLAG_SE_PLUS    	 (1 << 4)	/* ScanExpress Plus scanner */
#define MUSTEK_FLAG_PRO          (1 << 5)	/* Professional series scanner */
#define MUSTEK_FLAG_N		 (1 << 6)	/* N-type scanner (non SCSI) */
#define MUSTEK_FLAG_SCSI_PP      (1 << 22)	/* SCSI over parallel (e.g. 600 II EP) */
/* Additional equipment */
#define MUSTEK_FLAG_ADF		 (1 << 7)	/* automatic document feeder */
#define MUSTEK_FLAG_ADF_READY	 (1 << 8)	/* paper present */
#define MUSTEK_FLAG_TA		 (1 << 9)	/* transparency adapter */
/* Line-distance correction */
#define MUSTEK_FLAG_LD_NONE	 (1 << 10)	/* no line-distance corr */
#define MUSTEK_FLAG_LD_BLOCK     (1 << 11)	/* blockwise LD corr */
#define MUSTEK_FLAG_LD_N1	 (1 << 12)	/* LD corr for N-type v1 */
#define MUSTEK_FLAG_LD_N2	 (1 << 13)	/* LD corr for N-type v2 */
/* Manual fixes */
#define MUSTEK_FLAG_LD_FIX	 (1 << 14)	/* need line-distance fix? */
#define MUSTEK_FLAG_LINEART_FIX	 (1 << 15)	/* lineart fix/hack */
#define MUSTEK_FLAG_USE_EIGHTS	 (1 << 16)	/* use 1/8" lengths */
#define MUSTEK_FLAG_FORCE_GAMMA  (1 << 17)	/* force gamma table upload */
#define MUSTEK_FLAG_ENLARGE_X    (1 << 18)	/* need to enlarge x-res */
#define MUSTEK_FLAG_COVER_SENSOR (1 << 19)	/* scanner can detect open cover */
#define MUSTEK_FLAG_USE_BLOCK	 (1 << 20)	/* use blockmode */
#define MUSTEK_FLAG_LEGAL_SIZE	 (1 << 21)	/* scanner has legal size */
#define MUSTEK_FLAG_NO_BACKTRACK (1 << 21)	/* scanner has legal size */

/* Source values: */
#define MUSTEK_SOURCE_FLATBED	0
#define MUSTEK_SOURCE_ADF	1
#define MUSTEK_SOURCE_TA	2

/* Mode values: */
#define MUSTEK_MODE_LINEART	(1 << 0)	/* grayscale 1 bit / pixel */
#define MUSTEK_MODE_GRAY	(1 << 1)	/* grayscale 8 bits / pixel */
#define MUSTEK_MODE_COLOR	(1 << 2)	/* color 24 bits / pixel */
#define MUSTEK_MODE_HALFTONE	(1 << 3)	/* use dithering */

/* Color band codes: */
#define MUSTEK_CODE_GRAY	0
#define MUSTEK_CODE_RED		1
#define MUSTEK_CODE_GREEN	2
#define MUSTEK_CODE_BLUE	3

/* SCSI commands that the Mustek scanners understand (or not): */
#define MUSTEK_SCSI_TEST_UNIT_READY	0x00
#define MUSTEK_SCSI_REQUEST_SENSE       0x03
#define MUSTEK_SCSI_AREA_AND_WINDOWS	0x04
#define MUSTEK_SCSI_READ_SCANNED_DATA	0x08
#define MUSTEK_SCSI_GET_IMAGE_STATUS	0x0f
#define MUSTEK_SCSI_ADF_AND_BACKTRACK	0x10
#define MUSTEK_SCSI_CCD_DISTANCE	0x11
#define MUSTEK_SCSI_INQUIRY		0x12
#define MUSTEK_SCSI_MODE_SELECT 	0x15
#define MUSTEK_SCSI_START_STOP		0x1b
#define MUSTEK_SCSI_SET_WINDOW		0x24
#define MUSTEK_SCSI_GET_WINDOW		0x25
#define MUSTEK_SCSI_READ_DATA		0x28
#define MUSTEK_SCSI_SEND_DATA		0x2a
#define MUSTEK_SCSI_LOOKUP_TABLE	0x55

/* Convenience macros */
#if defined(MIN)
#undef MIN
#endif
#if defined(MAX)
#undef MAX
#endif
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#define MAX(a,b)	((a) > (b) ? (a) : (b))
/* Copy values to memory ('L' = little endian, 'B' = big endian */
#define STORE16L(cp,v)				\
do {						\
    int value = (v);				\
						\
    *(cp)++ = (value >> 0) & 0xff;		\
    *(cp)++ = (value >> 8) & 0xff;		\
} while (0)
#define STORE16B(cp,v)				\
do {						\
    int value = (v);				\
						\
    *(cp)++ = (value >> 8) & 0xff;		\
    *(cp)++ = (value >> 0) & 0xff;		\
} while (0)
#define STORE32B(cp,v)				\
do {						\
    long int value = (v);			\
						\
    *(cp)++ = (value >> 24) & 0xff;		\
    *(cp)++ = (value >> 16) & 0xff;		\
    *(cp)++ = (value >>  8) & 0xff;		\
    *(cp)++ = (value >>  0) & 0xff;		\
} while (0)

/* declarations */
enum Mustek_Option
{
  OPT_NUM_OPTS = 0,

  OPT_MODE_GROUP,
  OPT_MODE,
  OPT_FAST_GRAY_MODE,
  OPT_RESOLUTION,
  OPT_BIT_DEPTH,
  OPT_SPEED,
  OPT_SOURCE,
  OPT_PREVIEW,
  OPT_FAST_PREVIEW,
  OPT_LAMP_OFF_TIME,
  OPT_LAMP_OFF_BUTTON,
  OPT_GEOMETRY_GROUP,
  OPT_TL_X,			/* top-left x */
  OPT_TL_Y,			/* top-left y */
  OPT_BR_X,			/* bottom-right x */
  OPT_BR_Y,			/* bottom-right y */

  OPT_ENHANCEMENT_GROUP,
  OPT_BRIGHTNESS,
  OPT_BRIGHTNESS_R,
  OPT_BRIGHTNESS_G,
  OPT_BRIGHTNESS_B,
  OPT_CONTRAST,
  OPT_CONTRAST_R,
  OPT_CONTRAST_G,
  OPT_CONTRAST_B,
  OPT_CUSTOM_GAMMA,		/* use custom gamma tables? */
  /* The gamma vectors MUST appear in the order gray, red, green,
     blue.  */
  OPT_GAMMA_VECTOR,
  OPT_GAMMA_VECTOR_R,
  OPT_GAMMA_VECTOR_G,
  OPT_GAMMA_VECTOR_B,
  OPT_QUALITY_CAL,
  OPT_HALFTONE_DIMENSION,
  OPT_HALFTONE_PATTERN,

  /* must come last: */
  NUM_OPTIONS
};

typedef struct Mustek_Device
{
  struct Mustek_Device *next;
  SANE_String name;
  SANE_Device sane;
  SANE_Range dpi_range;
  SANE_Range x_range;
  SANE_Range y_range;
  /* scan area when transparency adapter is used: */
  SANE_Range x_trans_range;
  SANE_Range y_trans_range;
  SANE_Word flags;
  /* length of gamma table, probably always <= 4096 for the SE */
  SANE_Int gamma_length;
  /* values actually used by scanner, not necessarily the desired! */
  SANE_Int bpl, lines;
  /* what is needed for calibration (ScanExpress and Pro series) */
  struct
  {
    SANE_Int bytes;
    SANE_Int lines;
    SANE_Byte *buffer;
    SANE_Word *line_buffer[3];
  }
  cal;
  /* current and maximum buffer size used by the backend */
  /* the buffer sent to the scanner is actually half of this size */
  SANE_Int buffer_size;
  SANE_Int max_buffer_size;
  /* maximum size scanned in one block and corresponding lines */
  SANE_Int max_block_buffer_size;
  SANE_Int lines_per_block;
  SANE_Byte *block_buffer;

  /* firmware format: 0 = old, MUSTEK at pos 8; 1 = new, MUSTEK at
     pos 36 */
  SANE_Int firmware_format;
  /* firmware revision system: 0 = old, x.yz; 1 = new, Vxyz */
  SANE_Int firmware_revision_system;
}
Mustek_Device;

typedef struct Mustek_Scanner
{
  /* all the state needed to define a scan request: */
  struct Mustek_Scanner *next;

  SANE_Option_Descriptor opt[NUM_OPTIONS];
  Option_Value val[NUM_OPTIONS];
  SANE_Int gamma_table[4][256];
  SANE_Int *halftone_pattern;
  SANE_Bool custom_halftone_pattern;
  SANE_Int halftone_pattern_type;

  SANE_Bool scanning;
  SANE_Bool cancelled;
  SANE_Int pass;		/* pass number */
  SANE_Int line;		/* current line number */
  SANE_Parameters params;

  /* Parsed option values and variables that are valid only during
     actual scanning: */
  SANE_Word mode;
  SANE_Bool one_pass_color_scan;
  SANE_Int resolution_code;
  int fd;			/* SCSI filedescriptor */
  SANE_Pid reader_pid;		/* process id of reader */
  int reader_fds;		/* OS/2: pipe write handler for reader */
  int pipe;			/* pipe to reader process */
  long start_time;		/* at this time the scan started */
  SANE_Word total_bytes;	/* bytes transmitted by sane_read */
  SANE_Word total_lines;	/* lines transmitted to sane_read pipe */

  /* scanner dependent/low-level state: */
  Mustek_Device *hw;

  /* line-distance correction related state: */
  struct
  {
    SANE_Int color;		/* first color appearing in read data */
    SANE_Int max_value;
    SANE_Int peak_res;
    SANE_Int dist[3];		/* line distance */
    SANE_Int index[3];		/* index for R/G/B color assignment */
    SANE_Int quant[3];		/* for resolution correction */
    SANE_Int saved[3];		/* number of saved color lines */
    /* these are used for SE, MFS and N line-distance correction: */
    SANE_Byte *buf[3];
    /* these are used for N line-distance correction only: */
    SANE_Int ld_line;		/* line # currently processed in 
				   ld-correction */
    SANE_Int lmod3;		/* line # modulo 3 */
  }
  ld;
}
Mustek_Scanner;

#endif /* mustek_h */
