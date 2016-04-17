/* sane - Scanner Access Now Easy.

   This file is part of the SANE package.

   SANE is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License, or (at your
   option) any later version.

   SANE is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with sane; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

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

#ifndef SG_ERR_H
#define SG_ERR_H

/* Linux sg error codes taken from Doug Gilbert's sg_utils:
   http://www.torque.net/sg/ */

/* Some of the following error/status codes are exchanged between the
   various layers of the SCSI sub-system in Linux and should never
   reach the user. They are placed here for completeness. What appears
   here is copied from drivers/scsi/scsi.h which is not visible in
   the user space. */

/* The following are 'host_status' codes */
#ifndef DID_OK
#define DID_OK 0x00
#endif
#ifndef DID_NO_CONNECT
#define DID_NO_CONNECT 0x01     /* Unable to connect before timeout */
#define DID_BUS_BUSY 0x02       /* Bus remain busy until timeout */
#define DID_TIME_OUT 0x03       /* Timed out for some other reason */
#define DID_BAD_TARGET 0x04     /* Bad target (id?) */
#define DID_ABORT 0x05          /* Told to abort for some other reason */
#define DID_PARITY 0x06         /* Parity error (on SCSI bus) */
#define DID_ERROR 0x07          /* Internal error */
#define DID_RESET 0x08          /* Reset by somebody */
#define DID_BAD_INTR 0x09       /* Received an unexpected interrupt */
#define DID_PASSTHROUGH 0x0a    /* Force command past mid-level */
#define DID_SOFT_ERROR 0x0b     /* The low-level driver wants a retry */
#endif



/* These defines are to isolate applictaions from kernel define changes */
#define SG_ERR_DID_OK           DID_OK
#define SG_ERR_DID_NO_CONNECT   DID_NO_CONNECT
#define SG_ERR_DID_BUS_BUSY     DID_BUS_BUSY
#define SG_ERR_DID_TIME_OUT     DID_TIME_OUT
#define SG_ERR_DID_BAD_TARGET   DID_BAD_TARGET
#define SG_ERR_DID_ABORT        DID_ABORT
#define SG_ERR_DID_PARITY       DID_PARITY
#define SG_ERR_DID_ERROR        DID_ERROR
#define SG_ERR_DID_RESET        DID_RESET
#define SG_ERR_DID_BAD_INTR     DID_BAD_INTR
#define SG_ERR_DID_PASSTHROUGH  DID_PASSTHROUGH
#define SG_ERR_DID_SOFT_ERROR   DID_SOFT_ERROR

/* The following are 'driver_status' codes */
#ifndef DRIVER_OK
#define DRIVER_OK 0x00
#endif
#ifndef DRIVER_BUSY
#define DRIVER_BUSY 0x01
#define DRIVER_SOFT 0x02
#define DRIVER_MEDIA 0x03
#define DRIVER_ERROR 0x04
#define DRIVER_INVALID 0x05
#define DRIVER_TIMEOUT 0x06
#define DRIVER_HARD 0x07
#define DRIVER_SENSE 0x08       /* Sense_buffer has been set */

/* Following "suggests" are "or-ed" with one of previous 8 entries */
#define SUGGEST_RETRY 0x10
#define SUGGEST_ABORT 0x20
#define SUGGEST_REMAP 0x30
#define SUGGEST_DIE 0x40
#define SUGGEST_SENSE 0x80
#define SUGGEST_IS_OK 0xff
#endif
#ifndef DRIVER_MASK
#define DRIVER_MASK 0x0f
#endif
#ifndef SUGGEST_MASK
#define SUGGEST_MASK 0xf0
#endif

/* These defines are to isolate applictaions from kernel define changes */
#define SG_ERR_DRIVER_OK        DRIVER_OK
#define SG_ERR_DRIVER_BUSY      DRIVER_BUSY
#define SG_ERR_DRIVER_SOFT      DRIVER_SOFT
#define SG_ERR_DRIVER_MEDIA     DRIVER_MEDIA
#define SG_ERR_DRIVER_ERROR     DRIVER_ERROR
#define SG_ERR_DRIVER_INVALID   DRIVER_INVALID
#define SG_ERR_DRIVER_TIMEOUT   DRIVER_TIMEOUT
#define SG_ERR_DRIVER_HARD      DRIVER_HARD
#define SG_ERR_DRIVER_SENSE     DRIVER_SENSE
#define SG_ERR_SUGGEST_RETRY    SUGGEST_RETRY
#define SG_ERR_SUGGEST_ABORT    SUGGEST_ABORT
#define SG_ERR_SUGGEST_REMAP    SUGGEST_REMAP
#define SG_ERR_SUGGEST_DIE      SUGGEST_DIE
#define SG_ERR_SUGGEST_SENSE    SUGGEST_SENSE
#define SG_ERR_SUGGEST_IS_OK    SUGGEST_IS_OK
#define SG_ERR_DRIVER_MASK      DRIVER_MASK
#define SG_ERR_SUGGEST_MASK     SUGGEST_MASK

#endif
