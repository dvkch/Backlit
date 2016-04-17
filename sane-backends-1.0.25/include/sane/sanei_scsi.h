/* sane - Scanner Access Now Easy.
   Copyright (C) 1996, 1997 David Mosberger-Tang
   This file is part of the SANE package.

   SANE is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   SANE is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

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

/** @file sanei_scsi.h
 *  Generic interface to SCSI drivers.  
 *  @sa sanei_usb.h, sanei_ab306.h,sanei_lm983x.h, sanei_pa4s2.h, sanei_pio.h,
 *  and man sane-scsi(5) for user-oriented documentation
 */

#ifndef sanei_scsi_h
#define sanei_scsi_h

#include <sys/types.h>

#include <sane/sane.h>
#include <sane/config.h>

/** Sense handler
 *
 * The sense handler can be implemented in backends. It's for deciding
 * which sense codes should be considered an error and which shouldn't.
 *
 * @param fd file descriptor
 * @param sense_buffer pointer to buffer containing sense codes
 * @param arg pointer to data buffer
 *
 * @return
 * - SANE_STATUS_GOOD - on success (sense isn't regarded as error)
 * - any other status if sense code is regarded as error
 */
typedef SANE_Status (*SANEI_SCSI_Sense_Handler) (int fd,
						 u_char *sense_buffer,
						 void *arg);
/** Maximum size of a SCSI request
 */
extern int sanei_scsi_max_request_size;

/** Find SCSI devices.
 *
 * Find each SCSI device that matches the pattern specified by the
 * arguments.  String arguments can be "omitted" by passing NULL,
 * integer arguments can be "omitted" by passing -1.  
 * 
 *   Example: vendor="HP" model=NULL, type=NULL, bus=3, id=-1, lun=-1 would
 *	    attach all HP devices on SCSI bus 3.
 *
 * @param vendor
 * @param model
 * @param type
 * @param bus
 * @param channel
 * @param id
 * @param lun
 * @param attach callback invoked once for each device, dev is the real devicename (passed to attach callback)
 *
 */
extern void sanei_scsi_find_devices (const char *vendor, const char *model,
				     const char *type,
				     int bus, int channel, int id, int lun,
				     SANE_Status (*attach) (const char *dev));


/** Open a SCSI device
 *
 * Opens a SCSI device by its device filename and returns a file descriptor.
 * If it's necessary to adjust the SCSI buffer size, use 
 * sanei_scsi_open_extended().
 *
 * @param device_name name of the devicefile, e.g. "/dev/sg0"
 * @param fd file descriptor
 * @param sense_handler called whenever the SCSI driver returns a sense buffer
 * @param sense_arg pointer to data for the sense handler
 * 
 * @return 
 * - SANE_STATUS_GOOD - on success
 * - SANE_STATUS_ACCESS_DENIED - if the file couldn't be accessed due to
 *   permissions
 * - SANE_STATUS_NO_MEM - if malloc failed (not enough memory)
 * - SANE_STATUS_INVAL - if the filename was invalid or an unknown error occured
 *
 * @sa sanei_scsi_open_extended(), HAVE_SANEI_SCSI_OPEN_EXTENDED
 */
extern SANE_Status sanei_scsi_open (const char * device_name, int * fd,
				    SANEI_SCSI_Sense_Handler sense_handler,
				    void *sense_arg);

/** Open a SCSI device and set the buffer size
 *
 * The extended open call allows a backend to ask for a specific buffer
 * size. sanei_scsi_open_extended() tries to allocate a buffer of the size
 * given by *buffersize upon entry to this function. If
 * sanei_scsi_open_extended returns successfully, *buffersize contains the
 * available buffer size. This value may be both smaller or larger than the
 * value requested by the backend; it can even be zero. The backend must
 * decide, if it got enough buffer memory to work.
 *
 * Note that the value of *buffersize may differ for different files.
 *
 * @param device_name name of the devicefile, e.g. "/dev/sg0"
 * @param fd file descriptor
 * @param sense_handler called whenever the SCSI driver returns a sense buffer
 * @param sense_arg pointer to data for the sense handler
 * @param buffersize size of the SCAI request buffer (in bytes)
 * 
 * @return 
 * - SANE_STATUS_GOOD - on success
 * - SANE_STATUS_ACCESS_DENIED - if the file couldn't be accessed due to
 *   permissions
 * - SANE_STATUS_NO_MEM - if malloc failed (not enough memory)
 * - SANE_STATUS_INVAL - if the filename was invalid or an unknown error occured
 *
 * @sa sanei_scsi_open(), HAVE_SANEI_SCSI_OPEN_EXTENDED
 */
extern SANE_Status sanei_scsi_open_extended (
       const char * device_name, int * fd,
       SANEI_SCSI_Sense_Handler sense_handler,
       void *sense_arg, int *buffersize);

/** Do we have sanei_scsi_open_extended()?
 *
 * Let backends decide, which open call to use: if
 * HAVE_SANEI_SCSI_OPEN_EXTENDED is defined, sanei_scsi_open_extended may be
 * used.  May also be used to decide, if sanei_scsi_req_flush_all or
 * sanei_scsi_req_flush_all_extended() should be used.
 *
 * @sa sanei_scsi_open(), sanei_scsi_open_extended()
*/
#define HAVE_SANEI_SCSI_OPEN_EXTENDED

/** Enqueue SCSI command
 *
 * One or more scsi commands can be enqueued by calling sanei_scsi_req_enter().
 *
 * NOTE: Some systems may not support multiple outstanding commands.  On such
 * systems, sanei_scsi_req_enter() may block.  In other words, it is not proper
 * to assume that enter() is a non-blocking routine.
 *
 * @param fd file descriptor
 * @param src pointer to the SCSI command and associated write data (if any)
 * @param src_size length of the command and data
 * @param dst pointer to a buffer in which data is returned; NULL if no data is
 *   returned
 * @param dst_size  on input, the size of the buffer pointed to by dst, on exit,
 *   set to the number of bytes returned in the buffer (which is less than or equal
 *   to the buffer size; may be NULL if no data is expected
 * @param idp pointer to a void* that uniquely identifies the entered request
 *
 * @return
 * - SANE_STATUS_GOOD - on success
 * - SANE_STATUS_IO_ERROR - if an error was received from the SCSI driver
 * - SANE_STATUS_NO_MEM - if malloc failed (not enough memory)
 * - SANE_STATUS_INVAL - if a locking or an unknown error occured
 * @sa sanei_scsi_req_enter2()
 *
*/
extern SANE_Status sanei_scsi_req_enter (int fd,
					 const void * src, size_t src_size,
					 void * dst, size_t * dst_size,
					 void **idp);

/** Enqueue SCSI command and separated data 
 *
 * Same as sanei_scsi_req_enter(), but with separate buffers for the SCSI
 * command and for the data to be sent to the device.
 *
 * With sanei_scsi_req_enter(), the length of te SCSI command block must be
 * guessed. While that works in most cases, Canon scanners for example use the
 * vendor specific commands 0xd4, 0xd5 and 0xd6. The Canon scanners want to
 * get 6 byte command blocks for these commands, but sanei_scsi_req_enter() and
 * sanei_scsi_cmd() send 12 bytes.
 *
 * If dst_size and *dst_size are non-zero, a "read command" (ie, data transfer
 * from the device to the host) is assumed.
 *
 * @param fd file descriptor
 * @param cmd pointer to SCSI command
 * @param cmd_size size of the command
 * @param src pointer to the buffer with data to be sent to the scanner
 * @param src_size size of src buffer
 * @param dst pointer to a buffer in which data is returned; NULL if no data is
 *   returned
 * @param dst_size  on input, the size of the buffer pointed to by dst, on exit,
 *   set to the number of bytes returned in the buffer (which is less than or equal
 *   to the buffer size; may be NULL if no data is expected
 * @param idp pointer to a void* that uniquely identifies the entered request
 * @return
 * - SANE_STATUS_GOOD - on success
 * - SANE_STATUS_IO_ERROR - if an error was received from the SCSI driver
 * - SANE_STATUS_NO_MEM - if malloc failed (not enough memory)
 * - SANE_STATUS_INVAL - if a locking or an unknown error occured
 * @sa sanei_scsi_req_enter()
 */
extern SANE_Status sanei_scsi_req_enter2 (int fd,
					 const void * cmd, size_t cmd_size,
					 const void * src, size_t src_size,
					 void * dst, size_t * dst_size,
					 void **idp);

/** Wait for SCSI command
 *
 * Wait for the completion of the SCSI command with id ID.  
 *
 * @param id id used in sanei_scsi_req_enter()
 *
 * @return
 * - SANE_STATUS_GOOD - on success
 * - SANE_STATUS_DEVICE_BUSY - if the device is busy (try again later)
 * - SANE_STATUS_IO_ERROR - if an error was received from the SCSI driver
*/
extern SANE_Status sanei_scsi_req_wait (void *id);

/** Send SCSI command
 *
 * This is a convenience function that is equivalent to a pair of
 * sanei_scsi_req_enter()/sanei_scsi_req_wait() calls.
 *
 * @param fd file descriptor
 * @param src pointer to the SCSI command and associated write data (if any)
 * @param src_size length of the command and data
 * @param dst pointer to a buffer in which data is returned; NULL if no data is
 *   returned
 * @param dst_size  on input, the size of the buffer pointed to by dst, on exit,
 *   set to the number of bytes returned in the buffer (which is less than or equal
 *   to the buffer size; may be NULL if no data is expected
 *
 * @return
 * - SANE_STATUS_GOOD - on success
 * - SANE_STATUS_IO_ERROR - if an error was received from the SCSI driver
 * - SANE_STATUS_NO_MEM - if malloc failed (not enough memory)
 * - SANE_STATUS_INVAL - if a locking or an unknown error occured
 *
 * @sa sanei_scsi_cmd2(), sanei_scsi_req_enter(), sanei_scsi_req_wait()
 */
extern SANE_Status sanei_scsi_cmd (int fd,
				   const void * src, size_t src_size,
				   void * dst, size_t * dst_size);

/** Send SCSI command and separated data
 *
 * This is a convenience function that is equivalent to a pair of
 * sanei_scsi_req_enter2()/sanei_scsi_req_wait() calls.
 *
 * @param fd file descriptor
 * @param cmd pointer to SCSI command
 * @param cmd_size size of the command
 * @param src pointer to the buffer with data to be sent to the scanner
 * @param src_size size of src buffer
 * @param dst pointer to a buffer in which data is returned; NULL if no data is
 *   returned
 * @param dst_size  on input, the size of the buffer pointed to by dst, on exit,
 *   set to the number of bytes returned in the buffer (which is less than or equal
 *   to the buffer size; may be NULL if no data is expected
 * @return
 * - SANE_STATUS_GOOD - on success
 * - SANE_STATUS_IO_ERROR - if an error was received from the SCSI driver
 * - SANE_STATUS_NO_MEM - if malloc failed (not enough memory)
 * - SANE_STATUS_INVAL - if a locking or an unknown error occured
 *
 * @sa sanei_scsi_cmd(), sanei_scsi_req_enter(), sanei_scsi_req_wait()
 */
extern SANE_Status sanei_scsi_cmd2 (int fd,
				   const void * cmd, size_t cmd_size,
				   const void * src, size_t src_size,
				   void * dst, size_t * dst_size);

/** Flush queue
 *
 * Flush all pending SCSI commands. This function work only, if zero or one
 * SCSI file handles are open.
 *
 * @sa sanei_scsi_req_flush_all_extended()
*/
extern void sanei_scsi_req_flush_all (void);

/** Flush queue for handle
 *
 * Flush all SCSI commands pending for one handle
 *
 * @param fd file descriptor
 *
 * @sa sanei_scsi_req_flush_all()
 */
extern void sanei_scsi_req_flush_all_extended (int fd);

/** Close a SCSI device
 *
 * @param fd file descriptor
 *
 */
extern void sanei_scsi_close (int fd);

#endif /* sanei_scsi_h */
