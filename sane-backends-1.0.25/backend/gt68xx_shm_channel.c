/* sane - Scanner Access Now Easy.

   Copyright (C) 2002 Sergey Vlasov <vsu@altlinux.ru>
   
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

/** @file
 * @brief Shared memory channel implementation.
 */

#include "gt68xx_shm_channel.h"

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#ifndef SHM_R
#define SHM_R 0
#endif

#ifndef SHM_W
#define SHM_W 0
#endif

/** Shared memory channel.
 *
 */
struct Shm_Channel
{
  SANE_Int buf_size;			/**< Size of each buffer */
  SANE_Int buf_count;			/**< Number of buffers */
  void *shm_area;			/**< Address of shared memory area */
  SANE_Byte **buffers;			/**< Array of pointers to buffers */
  SANE_Int *buffer_bytes;		/**< Array of buffer byte counts */
  int writer_put_pipe[2];		/**< Notification pipe from writer */
  int reader_put_pipe[2];		/**< Notification pipe from reader */
};

/** Dummy union to find out the needed alignment */
union Shm_Channel_Align
{
  int i;
  long l;
  void *ptr;
  void (*func_ptr) (void);
  double d;
};

/** Check if shm_channel is valid */
#define SHM_CHANNEL_CHECK(shm_channel, func_name)               \
  do {                                                          \
    if ((shm_channel) == NULL)                                  \
      {                                                         \
        DBG (3, "%s: BUG: shm_channel==NULL\n", (func_name));   \
        return SANE_STATUS_INVAL;                               \
      }                                                         \
  } while (SANE_FALSE)

/** Alignment for shared memory contents */
#define SHM_CHANNEL_ALIGNMENT   (sizeof (union Shm_Channel_Align))

/** Align the given size up to a multiple of the given alignment */
#define SHM_CHANNEL_ROUND_UP(size, align) \
  ( ((size) % (align)) ? ((size)/(align) + 1)*(align) : (size) )

/** Align the size using SHM_CHANNEL_ALIGNMENT */
#define SHM_CHANNEL_ALIGN(size) \
  SHM_CHANNEL_ROUND_UP((size_t) (size), SHM_CHANNEL_ALIGNMENT)

/** Close a file descriptor if it is currently open.
 *
 * This function checks if the file descriptor is not -1, and sets it to -1
 * after close (so that it will not be closed twice).
 *
 * @param fd_var Pointer to a variable holding the file descriptor.
 */
static void
shm_channel_fd_safe_close (int *fd_var)
{
  if (*fd_var != -1)
    {
      close (*fd_var);
      *fd_var = -1;
    }
}

static SANE_Status
shm_channel_fd_set_close_on_exec (int fd)
{
  long value;

  value = fcntl (fd, F_GETFD, 0L);
  if (value == -1)
    return SANE_STATUS_IO_ERROR;
  if (fcntl (fd, F_SETFD, value | FD_CLOEXEC) == -1)
    return SANE_STATUS_IO_ERROR;

  return SANE_STATUS_GOOD;
}

#if 0
static SANE_Status
shm_channel_fd_set_non_blocking (int fd, SANE_Bool non_blocking)
{
  long value;

  value = fcntl (fd, F_GETFL, 0L);
  if (value == -1)
    return SANE_STATUS_IO_ERROR;

  if (non_blocking)
    value |= O_NONBLOCK;
  else
    value &= ~O_NONBLOCK;

  if (fcntl (fd, F_SETFL, value) == -1)
    return SANE_STATUS_IO_ERROR;

  return SANE_STATUS_GOOD;
}
#endif

/** Create a new shared memory channel.
 *
 * This function should be called before the fork to set up the shared memory.
 *
 * @param buf_size  Size of each shared memory buffer in bytes.
 * @param buf_count Number of shared memory buffers (up to 255).
 * @param shm_channel_return Returned shared memory channel object.
 */
SANE_Status
shm_channel_new (SANE_Int buf_size,
		 SANE_Int buf_count, Shm_Channel ** shm_channel_return)
{
  Shm_Channel *shm_channel;
  void *shm_area;
  SANE_Byte *shm_data;
  int shm_buffer_bytes_size, shm_buffer_size;
  int shm_size;
  int shm_id;
  int i;

  if (buf_size <= 0)
    {
      DBG (3, "shm_channel_new: invalid buf_size=%d\n", buf_size);
      return SANE_STATUS_INVAL;
    }
  if (buf_count <= 0 || buf_count > 255)
    {
      DBG (3, "shm_channel_new: invalid buf_count=%d\n", buf_count);
      return SANE_STATUS_INVAL;
    }
  if (!shm_channel_return)
    {
      DBG (3, "shm_channel_new: BUG: shm_channel_return==NULL\n");
      return SANE_STATUS_INVAL;
    }

  *shm_channel_return = NULL;

  shm_channel = (Shm_Channel *) malloc (sizeof (Shm_Channel));
  if (!shm_channel)
    {
      DBG (3, "shm_channel_new: no memory for Shm_Channel\n");
      return SANE_STATUS_NO_MEM;
    }

  shm_channel->buf_size = buf_size;
  shm_channel->buf_count = buf_count;
  shm_channel->shm_area = NULL;
  shm_channel->buffers = NULL;
  shm_channel->buffer_bytes = NULL;
  shm_channel->writer_put_pipe[0] = shm_channel->writer_put_pipe[1] = -1;
  shm_channel->reader_put_pipe[0] = shm_channel->reader_put_pipe[1] = -1;

  shm_channel->buffers =
    (SANE_Byte **) malloc (sizeof (SANE_Byte *) * buf_count);
  if (!shm_channel->buffers)
    {
      DBG (3, "shm_channel_new: no memory for buffer pointers\n");
      shm_channel_free (shm_channel);
      return SANE_STATUS_NO_MEM;
    }

  if (pipe (shm_channel->writer_put_pipe) == -1)
    {
      DBG (3, "shm_channel_new: cannot create writer put pipe: %s\n",
	   strerror (errno));
      shm_channel_free (shm_channel);
      return SANE_STATUS_NO_MEM;
    }

  if (pipe (shm_channel->reader_put_pipe) == -1)
    {
      DBG (3, "shm_channel_new: cannot create reader put pipe: %s\n",
	   strerror (errno));
      shm_channel_free (shm_channel);
      return SANE_STATUS_NO_MEM;
    }

  shm_channel_fd_set_close_on_exec (shm_channel->reader_put_pipe[0]);
  shm_channel_fd_set_close_on_exec (shm_channel->reader_put_pipe[1]);
  shm_channel_fd_set_close_on_exec (shm_channel->writer_put_pipe[0]);
  shm_channel_fd_set_close_on_exec (shm_channel->writer_put_pipe[1]);

  shm_buffer_bytes_size = SHM_CHANNEL_ALIGN (sizeof (SANE_Int) * buf_count);
  shm_buffer_size = SHM_CHANNEL_ALIGN (buf_size);
  shm_size = shm_buffer_bytes_size + buf_count * shm_buffer_size;

  shm_id = shmget (IPC_PRIVATE, shm_size, IPC_CREAT | SHM_R | SHM_W);
  if (shm_id == -1)
    {
      DBG (3, "shm_channel_new: cannot create shared memory segment: %s\n",
	   strerror (errno));
      shm_channel_free (shm_channel);
      return SANE_STATUS_NO_MEM;
    }

  shm_area = shmat (shm_id, NULL, 0);
  if (shm_area == (void *) -1)
    {
      DBG (3, "shm_channel_new: cannot attach to shared memory segment: %s\n",
	   strerror (errno));
      shmctl (shm_id, IPC_RMID, NULL);
      shm_channel_free (shm_channel);
      return SANE_STATUS_NO_MEM;
    }

  if (shmctl (shm_id, IPC_RMID, NULL) == -1)
    {
      DBG (3, "shm_channel_new: cannot remove shared memory segment id: %s\n",
	   strerror (errno));
      shmdt (shm_area);
      shmctl (shm_id, IPC_RMID, NULL);
      shm_channel_free (shm_channel);
      return SANE_STATUS_NO_MEM;
    }

  shm_channel->shm_area = shm_area;

  shm_channel->buffer_bytes = (SANE_Int *) shm_area;
  shm_data = ((SANE_Byte *) shm_area) + shm_buffer_bytes_size;
  for (i = 0; i < shm_channel->buf_count; ++i)
    {
      shm_channel->buffers[i] = shm_data;
      shm_data += shm_buffer_size;
    }

  *shm_channel_return = shm_channel;
  return SANE_STATUS_GOOD;
}

/** Close the shared memory channel and release associated resources.
 *
 * @param shm_channel Shared memory channel object.
 */
SANE_Status
shm_channel_free (Shm_Channel * shm_channel)
{
  SHM_CHANNEL_CHECK (shm_channel, "shm_channel_free");

  if (shm_channel->shm_area)
    {
      shmdt (shm_channel->shm_area);
      shm_channel->shm_area = NULL;
    }

  if (shm_channel->buffers)
    {
      free (shm_channel->buffers);
      shm_channel->buffers = NULL;
    }

  shm_channel_fd_safe_close (&shm_channel->reader_put_pipe[0]);
  shm_channel_fd_safe_close (&shm_channel->reader_put_pipe[1]);
  shm_channel_fd_safe_close (&shm_channel->writer_put_pipe[0]);
  shm_channel_fd_safe_close (&shm_channel->writer_put_pipe[1]);

  return SANE_STATUS_GOOD;
}

/** Initialize the shared memory channel in the writer process.
 *
 * This function should be called after the fork in the process which will
 * write data to the channel.
 *
 * @param shm_channel Shared memory channel object.
 */
SANE_Status
shm_channel_writer_init (Shm_Channel * shm_channel)
{
  SHM_CHANNEL_CHECK (shm_channel, "shm_channel_writer_init");

  shm_channel_fd_safe_close (&shm_channel->writer_put_pipe[0]);
  shm_channel_fd_safe_close (&shm_channel->reader_put_pipe[1]);

  return SANE_STATUS_GOOD;
}

/** Get a free shared memory buffer for writing.
 *
 * This function may block waiting for a free buffer (if the reader process
 * does not process the data fast enough).
 *
 * After successfull call to this function the writer process should fill the
 * buffer with the data and pass the buffer identifier from @a buffer_id_return
 * to shm_channel_writer_put_buffer() to give the buffer to the reader process.
 *
 * @param shm_channel Shared memory channel object.
 * @param buffer_id_return Returned buffer identifier.
 * @param buffer_addr_return Returned buffer address.
 *
 * @return
 * - SANE_STATUS_GOOD - a free buffer was available (or became available after
 *   waiting for it); @a buffer_id_return and @a buffer_addr_return are filled
 *   with valid values.
 * - SANE_STATUS_EOF - the reader process has closed its half of the channel.
 * - SANE_STATUS_IO_ERROR - an I/O error occured.
 */
SANE_Status
shm_channel_writer_get_buffer (Shm_Channel * shm_channel,
			       SANE_Int * buffer_id_return,
			       SANE_Byte ** buffer_addr_return)
{
  SANE_Byte buf_index;
  int bytes_read;

  SHM_CHANNEL_CHECK (shm_channel, "shm_channel_writer_get_buffer");

  do
    bytes_read = read (shm_channel->reader_put_pipe[0], &buf_index, 1);
  while (bytes_read == -1 && errno == EINTR);

  if (bytes_read == 1)
    {
      SANE_Int index = buf_index;
      if (index < shm_channel->buf_count)
	{
	  *buffer_id_return = index;
	  *buffer_addr_return = shm_channel->buffers[index];
	  return SANE_STATUS_GOOD;
	}
    }

  *buffer_id_return = -1;
  *buffer_addr_return = NULL;
  if (bytes_read == 0)
    return SANE_STATUS_EOF;
  else
    return SANE_STATUS_IO_ERROR;
}

/** Pass a filled shared memory buffer to the reader process.
 *
 * @param shm_channel Shared memory channel object.
 * @param buffer_id Buffer identifier from shm_channel_writer_put_buffer().
 * @param buffer_bytes Number of data bytes in the buffer.
 *
 * @return
 * - SANE_STATUS_GOOD - the buffer was successfully queued.
 * - SANE_STATUS_IO_ERROR - the reader process has closed its half of the
 *   channel, or another I/O error occured.
 */
SANE_Status
shm_channel_writer_put_buffer (Shm_Channel * shm_channel,
			       SANE_Int buffer_id, SANE_Int buffer_bytes)
{
  SANE_Byte buf_index;
  int bytes_written;

  SHM_CHANNEL_CHECK (shm_channel, "shm_channel_writer_put_buffer");

  if (buffer_id < 0 || buffer_id >= shm_channel->buf_count)
    {
      DBG (3, "shm_channel_writer_put_buffer: BUG: buffer_id=%d\n",
	   buffer_id);
      return SANE_STATUS_INVAL;
    }

  shm_channel->buffer_bytes[buffer_id] = buffer_bytes;

  buf_index = (SANE_Byte) buffer_id;
  do
    bytes_written = write (shm_channel->writer_put_pipe[1], &buf_index, 1);
  while ((bytes_written == 0) || (bytes_written == -1 && errno == EINTR));

  if (bytes_written == 1)
    return SANE_STATUS_GOOD;
  else
    return SANE_STATUS_IO_ERROR;
}

/** Close the writing half of the shared memory channel.
 *
 * @param shm_channel Shared memory channel object.
 */
SANE_Status
shm_channel_writer_close (Shm_Channel * shm_channel)
{
  SHM_CHANNEL_CHECK (shm_channel, "shm_channel_writer_close");

  shm_channel_fd_safe_close (&shm_channel->writer_put_pipe[1]);

  return SANE_STATUS_GOOD;
}


/** Initialize the shared memory channel in the reader process.
 *
 * This function should be called after the fork in the process which will
 * read data from the channel.
 *
 * @param shm_channel Shared memory channel object.
 */
SANE_Status
shm_channel_reader_init (Shm_Channel * shm_channel)
{
  SHM_CHANNEL_CHECK (shm_channel, "shm_channel_reader_init");

  shm_channel_fd_safe_close (&shm_channel->writer_put_pipe[1]);

  /* Don't close reader_put_pipe[0] here.  Otherwise, if the channel writer
   * process dies early, this process might get SIGPIPE - and I don't want to
   * mess with signals in the main process. */
  /* shm_channel_fd_safe_close (&shm_channel->reader_put_pipe[0]); */

  return SANE_STATUS_GOOD;
}

#if 0
/** Set non-blocking or blocking mode for the reading half of the shared memory
 * channel.
 *
 * @param shm_channel Shared memory channel object.
 * @param non_blocking SANE_TRUE to make the channel non-blocking, SANE_FALSE
 * to set blocking mode.
 *
 * @return
 * - SANE_STATUS_GOOD - the requested mode was set successfully.
 * - SANE_STATUS_IO_ERROR - error setting the requested mode.
 */
SANE_Status
shm_channel_reader_set_io_mode (Shm_Channel * shm_channel,
				SANE_Bool non_blocking)
{
  SHM_CHANNEL_CHECK (shm_channel, "shm_channel_reader_set_io_mode");

  return shm_channel_fd_set_non_blocking (shm_channel->writer_put_pipe[0],
					  non_blocking);
}

/** Get the file descriptor which will signal when some data is available in
 * the shared memory channel.
 *
 * The returned file descriptor can be used in select() or poll().  When one of
 * these functions signals that the file descriptor is ready for reading,
 * shm_channel_reader_get_buffer() should return some data without blocking.
 *
 * @param shm_channel Shared memory channel object.
 * @param fd_return The returned file descriptor.
 *
 * @return
 * - SANE_STATUS_GOOD - the file descriptor was returned.
 */
SANE_Status
shm_channel_reader_get_select_fd (Shm_Channel * shm_channel,
				  SANE_Int * fd_return)
{
  SHM_CHANNEL_CHECK (shm_channel, "shm_channel_reader_get_select_fd");

  *fd_return = shm_channel->writer_put_pipe[0];

  return SANE_STATUS_GOOD;
}
#endif

/** Start reading from the shared memory channel.
 *
 * A newly initialized shared memory channel is stopped - the writer process
 * will block on shm_channel_writer_get_buffer().  This function will pass all
 * available buffers to the writer process, starting the transfer through the
 * channel.
 *
 * @param shm_channel Shared memory channel object.
 */
SANE_Status
shm_channel_reader_start (Shm_Channel * shm_channel)
{
  int i, bytes_written;
  SANE_Byte buffer_id;

  SHM_CHANNEL_CHECK (shm_channel, "shm_channel_reader_start");

  for (i = 0; i < shm_channel->buf_count; ++i)
    {
      buffer_id = i;
      do
	bytes_written =
	  write (shm_channel->reader_put_pipe[1], &buffer_id, 1);
      while ((bytes_written == 0) || (bytes_written == -1 && errno == EINTR));

      if (bytes_written == -1)
	{
	  DBG (3, "shm_channel_reader_start: write error at buffer %d: %s\n",
	       i, strerror (errno));
	  return SANE_STATUS_IO_ERROR;
	}
    }

  return SANE_STATUS_GOOD;
}

/** Get the next shared memory buffer passed from the writer process.
 *
 * If the channel was not set to non-blocking mode, this function will block
 * until the data buffer arrives from the writer process.  In non-blocking mode
 * this function will place NULL in @a *buffer_addr_return and return
 * SANE_STATUS_GOOD if a buffer is not available immediately.
 *
 * After successful completion of this function (return value is
 * SANE_STATUS_GOOD and @a *buffer_addr_return is not NULL) the reader process
 * should process the data in the buffer and then call
 * shm_channel_reader_put_buffer() to release the buffer.
 *
 * @param shm_channel Shared memory channel object.
 * @param buffer_id_return Returned buffer identifier.
 * @param buffer_addr_return Returned buffer address.
 * @param buffer_bytes_return Returned number of data bytes in the buffer.
 *
 * @return
 * - SANE_STATUS_GOOD - no error.  If the channel was in non-blocking mode, @a
 *   *buffer_id_return may be NULL, indicating that no data was available.
 *   Otherwise, @a *buffer_id_return, @a *buffer_addr_return and @a
 *   *buffer_bytes return are filled with valid values.
 * - SANE_STATUS_EOF - the writer process has closed its half of the channel.
 * - SANE_STATUS_IO_ERROR - an I/O error occured.
 */
SANE_Status
shm_channel_reader_get_buffer (Shm_Channel * shm_channel,
			       SANE_Int * buffer_id_return,
			       SANE_Byte ** buffer_addr_return,
			       SANE_Int * buffer_bytes_return)
{
  SANE_Byte buf_index;
  int bytes_read;

  SHM_CHANNEL_CHECK (shm_channel, "shm_channel_reader_get_buffer");

  do
    bytes_read = read (shm_channel->writer_put_pipe[0], &buf_index, 1);
  while (bytes_read == -1 && errno == EINTR);

  if (bytes_read == 1)
    {
      SANE_Int index = buf_index;
      if (index < shm_channel->buf_count)
	{
	  *buffer_id_return = index;
	  *buffer_addr_return = shm_channel->buffers[index];
	  *buffer_bytes_return = shm_channel->buffer_bytes[index];
	  return SANE_STATUS_GOOD;
	}
    }

  *buffer_id_return = -1;
  *buffer_addr_return = NULL;
  *buffer_bytes_return = 0;
  if (bytes_read == 0)
    return SANE_STATUS_EOF;
  else
    return SANE_STATUS_IO_ERROR;
}

/** Release a shared memory buffer received by the reader process.
 *
 * This function must be called after shm_channel_reader_get_buffer() to
 * release the buffer and make it available for transferring the next portion
 * of data.
 *
 * After calling this function the reader process must not access the buffer
 * contents; any data which may be needed later should be copied into some
 * other place beforehand.
 *
 * @param shm_channel Shared memory channel object.
 * @param buffer_id Buffer identifier from shm_channel_reader_get_buffer().
 *
 * @return
 * - SANE_STATUS_GOOD - the buffer was successfully released.
 * - SANE_STATUS_IO_ERROR - the writer process has closed its half of the
 *   channel, or an unexpected I/O error occured.
 */
SANE_Status
shm_channel_reader_put_buffer (Shm_Channel * shm_channel, SANE_Int buffer_id)
{
  SANE_Byte buf_index;
  int bytes_written;

  SHM_CHANNEL_CHECK (shm_channel, "shm_channel_reader_put_buffer");

  if (buffer_id < 0 || buffer_id >= shm_channel->buf_count)
    {
      DBG (3, "shm_channel_reader_put_buffer: BUG: buffer_id=%d\n",
	   buffer_id);
      return SANE_STATUS_INVAL;
    }

  buf_index = (SANE_Byte) buffer_id;
  do
    bytes_written = write (shm_channel->reader_put_pipe[1], &buf_index, 1);
  while ((bytes_written == 0) || (bytes_written == -1 && errno == EINTR));

  if (bytes_written == 1)
    return SANE_STATUS_GOOD;
  else
    return SANE_STATUS_IO_ERROR;
}

#if 0
/** Close the reading half of the shared memory channel.
 *
 * @param shm_channel Shared memory channel object.
 */
SANE_Status
shm_channel_reader_close (Shm_Channel * shm_channel)
{
  SHM_CHANNEL_CHECK (shm_channel, "shm_channel_reader_close");

  shm_channel_fd_safe_close (&shm_channel->reader_put_pipe[1]);

  return SANE_STATUS_GOOD;
}
#endif
/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
