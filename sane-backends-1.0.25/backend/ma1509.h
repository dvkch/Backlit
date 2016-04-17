/* sane - Scanner Access Now Easy.
   (C) 2003 Henning Meier-Geinitz <henning@meier-geinitz.de>.

   Based on the mustek (SCSI) backend.

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

   This file implements a SANE backend for scanners based on the Mustek
   MA-1509 chipset. Currently the Mustek BearPaw 1200F is known to work.
*/

#ifndef ma1509_h
#define ma1509_h

#include "../include/sane/config.h"
#include <sys/types.h>

/* Some constants */
#define INQ_LEN	0x60		/* Length of SCSI inquiry */
#define MA1509_COMMAND_LENGTH 8
#define MA1509_GAMMA_SIZE 1024
#define MA1509_BUFFER_SIZE (1024 * 128)
#define MA1509_WARMUP_TIME 30

#ifndef PATH_MAX
# define PATH_MAX 1024
#endif

#define MA1509_CONFIG_FILE "ma1509.conf"

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
enum Ma1509_Option
{
  OPT_NUM_OPTS = 0,

  OPT_MODE_GROUP,
  OPT_MODE,
  OPT_RESOLUTION,
  OPT_SOURCE,
  OPT_PREVIEW,

  OPT_GEOMETRY_GROUP,
  OPT_TL_X,			/* top-left x */
  OPT_TL_Y,			/* top-left y */
  OPT_BR_X,			/* bottom-right x */
  OPT_BR_Y,			/* bottom-right y */

  OPT_ENHANCEMENT_GROUP,
  OPT_THRESHOLD,
  OPT_CUSTOM_GAMMA,		/* use custom gamma tables? */
  OPT_GAMMA_VECTOR_R,
  OPT_GAMMA_VECTOR_G,
  OPT_GAMMA_VECTOR_B,
  /* must come last: */
  NUM_OPTIONS
};

typedef struct Ma1509_Device
{
  struct Ma1509_Device *next;
  SANE_String name;
  SANE_Device sane;
  SANE_Bool has_ta;
  SANE_Bool has_adf;
  SANE_Range x_range;
  SANE_Range y_range;
  /* scan area when transparency adapter is used: */
  SANE_Range x_trans_range;
  SANE_Range y_trans_range;
  /* values actually used by scanner, not necessarily the desired! */
  SANE_Int bpl, ppl, lines;
}
Ma1509_Device;

typedef struct Ma1509_Scanner
{
  /* all the state needed to define a scan request: */
  struct Ma1509_Scanner *next;

  SANE_Option_Descriptor opt[NUM_OPTIONS];
  Option_Value val[NUM_OPTIONS];

  SANE_Bool scanning;
  SANE_Bool cancelled;
  SANE_Parameters params;

  /* Parsed option values and variables that are valid only during
     actual scanning: */
  int fd;			/* filedescriptor */
  long start_time;		/* at this time the scan started */
  long lamp_time;		/* at this time the lamp was turned on */
  SANE_Word total_bytes;	/* bytes read from scanner */
  SANE_Word read_bytes;		/* bytes transmitted by sane_read */

  SANE_Int red_gamma_table[MA1509_GAMMA_SIZE];
  SANE_Int green_gamma_table[MA1509_GAMMA_SIZE];
  SANE_Int blue_gamma_table[MA1509_GAMMA_SIZE];

  SANE_Byte *buffer, *buffer_start;
  SANE_Int buffer_bytes;
  /* scanner dependent/low-level state: */
  Ma1509_Device *hw;
}
Ma1509_Scanner;

#endif /* ma1509_h */
