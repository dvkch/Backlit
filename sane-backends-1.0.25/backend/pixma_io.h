/* SANE - Scanner Access Now Easy.

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
#ifndef PIXMA_IO_H
#define PIXMA_IO_H

/* TODO: move to pixma_common.h, to reduce the number of files */

/*!
 * \defgroup IO IO interface
 * \brief The IO interface.
 *
 * Return value of functions that return \c int if not otherwise specified:
 *  - >=    if succeeded
 *  - < 0   if failed (e.g. \c PIXMA_ETIMEDOUT)
 * .
 * @{
 */

/** Timeout for pixma_read() in milliseconds */
#define PIXMA_BULKIN_TIMEOUT  20000
/** Timeout for pixma_write() in milliseconds */
#define PIXMA_BULKOUT_TIMEOUT 20000


struct pixma_io_t;
struct pixma_config_t;

/** IO handle */
typedef struct pixma_io_t pixma_io_t;


/** Initialize IO module. It must be called before any other functions in this
 *  module.
 *  \return 0 on success or
 *   - \c PIXMA_ENOMEM
 *   - \c PIXMA_EACCES
 *   - \c PIXMA_EIO */
int pixma_io_init (void);

/** Shutdown all connections and free resources allocated in this module. */
void pixma_io_cleanup (void);

/** Find devices currently connected to the computer.
 *  \c devnr passed to functions
 *  - pixma_get_device_config()
 *  - pixma_get_device_id()
 *  - pixma_connect()
 *  .
 *  should be less than the number of devices returned by this function.
 *  \param[in] pixma_devices A \c NULL terminated array of pointers to
 *             array of pixma_config_t which is terminated by setting
 *             pixma_config_t::name to \c NULL.
 *  \return Number of devices found */
unsigned pixma_collect_devices (const char ** conf_devices, 
                                const struct pixma_config_t *const
				pixma_devices[]);

/** Get device configuration. */
const struct pixma_config_t *pixma_get_device_config (unsigned devnr);

/** Get a unique ID of the device \a devnr. */
const char *pixma_get_device_id (unsigned devnr);

/** Connect to the device and claim the scanner interface.
 *  \param[in] devnr
 *  \param[out] handle
 *  \return 0 on success or
 *   - \c PIXMA_ENODEV the device is gone from the system.
 *   - \c PIXMA_EINVAL \a devnr is invalid.
 *   - \c PIXMA_EBUSY
 *   - \c PIXMA_EACCES
 *   - \c PIXMA_ENOMEM
 *   - \c PIXMA_EIO */
int pixma_connect (unsigned devnr, pixma_io_t ** handle);

/** Release the scanner interface and disconnect from the device. */
void pixma_disconnect (pixma_io_t *);

/** Activate connection to scanner */
int pixma_activate (pixma_io_t *);

/** De-activate connection to scanner */
int pixma_deactivate (pixma_io_t *);

/** Reset the USB interface. \warning Use with care! */
int pixma_reset_device (pixma_io_t *);

/** Write data to the device. This function may not be interrupted by signals.
 *  It will return iff
 *   - \a len bytes have been successfully written or
 *   - an error (inclusive timeout) occured.
 *  .
 *  \note Calling pixma_write(io, buf, n1) and pixma(io, buf+n1, n2) may
 *        not be the same as pixma_write(io, buf, n1+n2) if n1 is not
 *        multiple of the maximum packet size of the endpoint.
 *  \param[in] cmd Data
 *  \param[in] len Length of data
 *  \return Number of bytes successfully written (always = \a len) or
 *   - \c PIXMA_ETIMEDOUT
 *   - \c PIXMA_EIO
 *   - \c PIXMA_ENOMEM
 *  \see #PIXMA_BULKOUT_TIMEOUT */
int pixma_write (pixma_io_t *, const void *cmd, unsigned len);

/** Read data from the device. This function may not be interrupted by signals.
 *  It will return iff
 *   - \a size bytes have been successfully read,
 *   - a short packet has been read or
 *   - an error (inclusive timeout) occured.
 *  .
 *  \param[out] buf
 *  \param[in]  size of the buffer
 *  \return Number of bytes successfully read. A return value of zero means that
 *       a zero length USB packet was received. Or
 *   - \c PIXMA_ETIMEDOUT
 *   - \c PIXMA_EIO
 *   - \c PIXMA_ENOMEM
 *  \see #PIXMA_BULKIN_TIMEOUT */
int pixma_read (pixma_io_t *, void *buf, unsigned size);

/** Wait for an interrupt. This function can be interrupted by signals.
 *  \a size should be less than or equal to the maximum packet size.
 *  \param[out] buf
 *  \param[in]  size of the buffer
 *  \param[in]  timeout in milliseconds; if < 0, wait forever.
 *  \return Number of bytes successfully read or
 *   - \c PIXMA_ETIMEDOUT
 *   - \c PIXMA_EIO
 *   - \c PIXMA_ENOMEM
 *   - \c PIXMA_ECANCELED if it was interrupted by a signal. */
int pixma_wait_interrupt (pixma_io_t *, void *buf, unsigned size,
			  int timeout);

/** Enable or disable background interrupt monitoring.
 *  Background mode is enabled by default.
 *  \param[in] background if not zero, a URB is submitted in background
 *             for interrupt endpoint.
 *  \return 0 on success or
 *   - \c PIXMA_ENOTSUP
 *   - \c PIXMA_EIO
 *   - \c PIXMA_ENOMEM */
int pixma_set_interrupt_mode (pixma_io_t *, int background);

/** @} end of IO group */

#endif
