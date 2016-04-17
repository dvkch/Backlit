/* SANE - Scanner Access Now Easy.

   Copyright (C) 2011-2015 Rolf Bensch <rolf at bensch hyphen online dot de>
   Copyright (C) 2006-2007 Wittawat Yamwong <wittawat@web.de>

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
#ifndef PIXMA_COMMON_H
#define PIXMA_COMMON_H


#include <time.h>		/* time_t */
#include "pixma.h"


/*! \defgroup subdriver Subdriver Interface
 *  \brief Subdriver interface. */

/*! \defgroup debug Debug utilities
 *  \brief Debug utilities. */

#ifdef NDEBUG
# define PDBG(x)     do {} while(0)
# define PASSERT(x)  do {} while(0)
#else
# define PDBG(x) x
# define PASSERT(x) do {				\
    if (!(x))						\
	pixma_dbg(1, "ASSERT failed:%s:%d: "	\
		     #x "\n", __FILE__, __LINE__);	\
  } while(0)
#endif


#define PIXMA_STATUS_OK       0x0606
#define PIXMA_STATUS_FAILED   0x1515
#define PIXMA_STATUS_BUSY     0x1414

#define PIXMA_MAX_ID_LEN 30

/* These may have been defined elsewhere */
#ifndef MIN
#define MIN(x,y) (((x) < (y)) ? (x):(y))
#endif
#ifndef MAX
#define MAX(x,y) (((x) < (y)) ? (y):(x))
#endif
#define ALIGN_SUP(x,n) (((x) + (n) - 1) / (n) * (n))
#define ALIGN_INF(x,n) (((x) / (n)) * (n))

struct pixma_io_t;

struct pixma_limits_t
{
  unsigned xdpi, ydpi;
  unsigned width, height;
};

struct pixma_cmdbuf_t
{
  unsigned cmd_header_len, res_header_len, cmd_len_field_ofs;
  unsigned expected_reslen, cmdlen;
  int reslen;
  unsigned size;
  uint8_t *buf;
};

struct pixma_imagebuf_t
{
  uint8_t *wptr, *wend;
  const uint8_t *rptr, *rend;
};

struct pixma_t
{
  pixma_t *next;
  struct pixma_io_t *io;
  const pixma_scan_ops_t *ops;
  pixma_scan_param_t *param;
  const pixma_config_t *cfg;
  char id[PIXMA_MAX_ID_LEN + 1];
  int cancel;			/* NOTE: It can be set in a signal handler. */
  uint32_t events;
  void *subdriver;		/* can be used by model driver. */

  /* private */
  uint64_t cur_image_size;
  pixma_imagebuf_t imagebuf;
  unsigned scanning:1;
  unsigned underrun:1;
};

/** \addtogroup subdriver
 * @{ */
/** Scan operations for subdriver. */
struct pixma_scan_ops_t
{
    /** Allocate a data structure for the subdriver. It is called after the
     *  core driver connected to the scanner. The subdriver should reset the
     *  scanner to a known state in this function. */
  int (*open) (pixma_t *);

    /** Free resources allocated by the subdriver. Don't forget to send abort
     *  command to the scanner if it is scanning. */
  void (*close) (pixma_t *);

    /** Setup the scanner for scan parameters defined in \a s->param. */
  int (*scan) (pixma_t * s);

    /** Fill a buffer with image data. The subdriver has two choices:
     * -# Fill the buffer pointed by ib->wptr directly and leave
     *    ib->rptr and ib->rend untouched. The length of the buffer is
     *    ib->wend - ib->wptr. It must update ib->wptr accordingly.
     * -# Update ib->rptr and ib->rend to point to the beginning and
     *    the end of the internal buffer resp. The length of the buffer
     *    is ib->rend - ib->rptr. This function is called again if
     *    and only if pixma_read_image() has copied the whole buffer.
     *
     * The subdriver must wait until there is at least one byte to read or
     * return 0 for the end of image. */
  int (*fill_buffer) (pixma_t *, pixma_imagebuf_t * ib);

    /** Cancel the scan operation if necessary and free resources allocated in
     *  scan(). */
  void (*finish_scan) (pixma_t *);

    /** [Optional] Wait for a user's event, e.g. button event. \a timeout is
     *  in milliseconds. If an event occured before it's timed out, flags in
     *  \a s->events should be set accordingly.
     *  \see PIXMA_EV_* */
  void (*wait_event) (pixma_t * s, int timeout);

    /** Check the scan parameters. The parameters can be adjusted if they are
     *  out of range, e.g. width > max_width. */
  int (*check_param) (pixma_t *, pixma_scan_param_t *);

    /** Read the device status. \see pixma_get_device_status() */
  int (*get_status) (pixma_t *, pixma_device_status_t *);
};


/** \name Funtions for read and write big-endian integer values */
/**@{*/
void pixma_set_be16 (uint16_t x, uint8_t * buf);
void pixma_set_be32 (uint32_t x, uint8_t * buf);
uint16_t pixma_get_be16 (const uint8_t * buf);
uint32_t pixma_get_be32 (const uint8_t * buf);
/**@}*/

/** \name Utility functions */
/**@{*/
uint8_t pixma_sum_bytes (const void *data, unsigned len);
int pixma_check_dpi (unsigned dpi, unsigned max);
void pixma_sleep (unsigned long usec);
void pixma_get_time (time_t * sec, uint32_t * usec);
uint8_t * pixma_r_to_ir (uint8_t * gptr, uint8_t * sptr, unsigned w, unsigned c);
uint8_t * pixma_rgb_to_gray (uint8_t * gptr, uint8_t * sptr, unsigned w, unsigned c);
uint8_t * pixma_binarize_line(pixma_scan_param_t *, uint8_t * dst, uint8_t * src, unsigned width, unsigned c);
/**@}*/

/** \name Command related functions */
/**@{*/
int pixma_cmd_transaction (pixma_t *, const void *cmd, unsigned cmdlen,
			   void *data, unsigned expected_len);
int pixma_check_result (pixma_cmdbuf_t *);
uint8_t *pixma_newcmd (pixma_cmdbuf_t *, unsigned cmd,
		       unsigned dataout, unsigned datain);
int pixma_exec (pixma_t *, pixma_cmdbuf_t *);
int pixma_exec_short_cmd (pixma_t *, pixma_cmdbuf_t *, unsigned cmd);
int pixma_map_status_errno (unsigned status);
/**@}*/

#define pixma_fill_checksum(start, end) do {		\
    *(end) = -pixma_sum_bytes(start, (end)-(start));	\
} while(0)

/** @} end of group subdriver */

/** \addtogroup debug
 *  @{ */
void pixma_set_debug_level (int level);
#ifndef NDEBUG
void pixma_hexdump (int level, const void *d_, unsigned len);

/* len:   length of data or error code.
   size:  if >= 0, force to print 'size' bytes.
   max:   maximum number of bytes to print(-1 means no limit). */
void pixma_dump (int level, const char *type, const void *data, int len,
		 int size, int max);
#  define DEBUG_DECLARE_ONLY
#  include "../include/sane/sanei_debug.h"
#endif /* NDEBUG */
/** @} end of group debug */

#endif
