/*
  Snapscan 1212U modifications for the Snapscan SANE backend

  Copyright (C) 2000 Henrik Johansson

  Henrik Johansson (henrikjo@post.urfors.se)

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

  This file implements USB equivalents to the SCSI routines used by the Snapscan
  backend.
*/

/* $Id$
   SnapScan backend scan data sources */

#ifndef snapscan_usb_h
#define snapscan_usb_h

typedef SANE_Status (*sense_handler_type)(int fd, u_char *sense_buffer, void *arg);

static SANE_Status snapscani_usb_cmd(int fd, const void *src, size_t src_size,
                         void *dst, size_t * dst_size);
static SANE_Status snapscani_usb_open(const char *dev, int *fdp,
    sense_handler_type, void*);
static void snapscani_usb_close(int fd);

/*
 *  USB status codes
 */
#define GOOD                 0x00
#define CHECK_CONDITION      0x01
#define CONDITION_GOOD       0x02
#define BUSY                 0x04
#define INTERMEDIATE_GOOD    0x08
#define INTERMEDIATE_C_GOOD  0x0a
#define RESERVATION_CONFLICT 0x0c
#define COMMAND_TERMINATED   0x11
#define QUEUE_FULL           0x14

#define STATUS_MASK          0x3e

/*
 *  USB transaction status
 */
#define TRANSACTION_COMPLETED 0xfb   /* Scanner considers the transaction done */
#define TRANSACTION_READ 0xf9        /* Scanner has data to deliver */
#define TRANSACTION_WRITE 0xf8       /* Scanner is expecting more data */

/*
 *  Busy queue data structure and prototypes
 */
struct usb_busy_queue {
    int fd;
    void *src;
    size_t src_size;
    struct usb_busy_queue *next;
};

static struct usb_busy_queue *bqhead,*bqtail;
static int enqueue_bq(int fd,const void *src, size_t src_size);
static void dequeue_bq(void);
static int is_queueable(const char *src);

static SANE_Status atomic_usb_cmd(int fd, const void *src, size_t src_size,
                    void *dst, size_t * dst_size);
static SANE_Status usb_cmd(int fd, const void *src, size_t src_size,
                    void *dst, size_t * dst_size);

#endif

/*
 * $Log$
 * Revision 1.6  2003/07/26 17:16:55  oliverschwartz
 * Changed licence to GPL + SANE exception for snapscan-usb.[ch]
 *
 * Revision 1.5  2002/04/10 21:45:53  oliverschwartz
 * Removed illegal character / removed declaration of bqelements
 *
 * Revision 1.10  2001/12/09 23:06:45  oliverschwartz
 * - use sense handler for USB if scanner reports CHECK_CONDITION
 *
 * Revision 1.9  2001/11/16 20:23:16  oliverschwartz
 * Merge with sane-1.0.6
 *   - Check USB vendor IDs to avoid hanging scanners
 *   - fix bug in dither matrix computation
 *
 * Revision 1.8  2001/09/18 15:01:07  oliverschwartz
 * - Read scanner id string again after firmware upload
 *   to indentify correct model
 * - Make firmware upload work for AGFA scanners
 * - Change copyright notice
 *
 * */
